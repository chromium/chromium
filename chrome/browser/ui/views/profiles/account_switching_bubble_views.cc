// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/signin/account_switching_manager.h"
#include "chrome/browser/ui/signin/account_switching_bubble.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kAccountSwitchingBubblePrimaryAccountEmailElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kAccountSwitchingBubbleTargetAccountEmailElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kAccountSwitchingBubbleAcceptButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kAccountSwitchingBubbleDeclineButtonElementId);

namespace {

int GetBubbleTitleMessageId() {
  switch (switches::kAccountPreviewSwitchingAccountDesktopArm.Get()) {
    case switches::AccountPreviewSwitchingAccountDesktopArm::kInProfileSwitch:
      return IDS_ACCOUNT_SWITCHING_IN_PROFILE_BUBBLE_TITLE;
    case switches::AccountPreviewSwitchingAccountDesktopArm::kNewProfile:
      return IDS_ACCOUNT_SWITCHING_NEW_PROFILE_BUBBLE_TITLE;
  }
}

int GetAcceptButtonLabelMessageId() {
  switch (switches::kAccountPreviewSwitchingAccountDesktopArm.Get()) {
    case switches::AccountPreviewSwitchingAccountDesktopArm::kInProfileSwitch:
      return IDS_ACCOUNT_SWITCHING_IN_PROFILE_BUBBLE_ACCEPT_BUTTON_LABEL;
    case switches::AccountPreviewSwitchingAccountDesktopArm::kNewProfile:
      return IDS_ACCOUNT_SWITCHING_NEW_PROFILE_BUBBLE_ACCEPT_BUTTON_LABEL;
  }
}

class AccountSwitchingBubbleDelegate
    : public ui::DialogModelDelegate,
      public signin::IdentityManager::Observer {
 public:
  using ResultCallback =
      base::OnceCallback<void(signin::AccountSwitchingPromoResult)>;

  AccountSwitchingBubbleDelegate(signin::IdentityManager& identity_manager,
                                 CoreAccountId primary_account_id,
                                 CoreAccountId target_account_id,
                                 base::ScopedClosureRunner keep_visible,
                                 ResultCallback result_callback)
      : primary_account_id_(std::move(primary_account_id)),
        target_account_id_(std::move(target_account_id)),
        keep_visible_(std::move(keep_visible)),
        result_callback_(std::move(result_callback)) {
    identity_manager_observation_.Observe(&identity_manager);
  }

  ~AccountSwitchingBubbleDelegate() override = default;

  void OnAccept() {
    NotifyResult(signin::AccountSwitchingPromoResult::kAccepted);
  }

  void OnDecline() {
    NotifyResult(signin::AccountSwitchingPromoResult::kDeclined);
  }

  void OnWidgetClosed(views::Widget::ClosedCallback on_close,
                      views::Widget::ClosedReason reason) {
    identity_manager_observation_.Reset();
    keep_visible_.RunAndReset();
    ResultCallback callback = std::move(result_callback_);
    if (on_close) {
      std::move(on_close).Run(reason);
    }
    if (callback) {
      std::move(callback).Run(
          (reason == views::Widget::ClosedReason::kCloseButtonClicked ||
           reason == views::Widget::ClosedReason::kEscKeyPressed)
              ? signin::AccountSwitchingPromoResult::kDismissed
              : signin::AccountSwitchingPromoResult::kIgnored);
    }
  }

  void OnDialogDestroying() {
    identity_manager_observation_.Reset();
    keep_visible_.RunAndReset();
    NotifyResult(signin::AccountSwitchingPromoResult::kIgnored);
  }

  base::WeakPtr<AccountSwitchingBubbleDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
        signin::PrimaryAccountChangeEvent::Type::kNone) {
      CloseForInvalidation();
    }
  }

  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    if (IsObservedAccount(account_id)) {
      CloseForInvalidation();
    }
  }

  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override {
    if (error.IsPersistentError() &&
        IsObservedAccount(account_info.account_id)) {
      CloseForInvalidation();
    }
  }

  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override {
    identity_manager_observation_.Reset();
  }

 private:
  bool IsObservedAccount(const CoreAccountId& account_id) const {
    return account_id == primary_account_id_ ||
           account_id == target_account_id_;
  }

  void CloseForInvalidation() {
    identity_manager_observation_.Reset();
    if (dialog_model() && dialog_model()->host()) {
      // `Close()` synchronously invokes `OnWidgetClosed()`, which clears state,
      // schedules widget destruction via `on_close`, and notifies the result
      // callback.
      dialog_model()->host()->Close();
    }
  }

  void NotifyResult(signin::AccountSwitchingPromoResult result) {
    // Stop observing before invoking `result_callback_` so synchronous account
    // mutations triggered by accepting the switch do not re-entrantly close the
    // dialog while the button callback is still executing.
    identity_manager_observation_.Reset();
    if (result_callback_) {
      std::move(result_callback_).Run(result);
    }
  }

  const CoreAccountId primary_account_id_;
  const CoreAccountId target_account_id_;
  base::ScopedClosureRunner keep_visible_;
  ResultCallback result_callback_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::WeakPtrFactory<AccountSwitchingBubbleDelegate> weak_ptr_factory_{this};
};

}  // namespace

std::unique_ptr<views::Widget> CreateAccountSwitchingBubble(
    signin::IdentityManager& identity_manager,
    views::BubbleAnchor anchor,
    base::ScopedClosureRunner keep_visible,
    const AccountInfo& primary_account,
    const AccountInfo& target_account,
    base::OnceCallback<void(signin::AccountSwitchingPromoResult)>
        result_callback,
    views::Widget::ClosedCallback on_close) {
  CHECK(!anchor.IsNull());

  // TODO(crbug.com/590787536): Tie the "Switch account?" avatar pill expansion
  // to this bubble's lifetime via
  // `AvatarToolbarButtonInterface::SetExplicitButtonState()`.
  auto model_delegate = std::make_unique<AccountSwitchingBubbleDelegate>(
      identity_manager, primary_account.GetAccountId(),
      target_account.GetAccountId(), std::move(keep_visible),
      std::move(result_callback));
  auto* model_delegate_ptr = model_delegate.get();

  // TODO(crbug.com/590786918): Replace this minimal skeleton content with the
  // full UX layout, visual assets, and exact preview counts.
  auto dialog_model =
      ui::DialogModel::Builder(std::move(model_delegate))
          .SetInternalName("AccountSwitchingBubbleViews")
          .SetTitle(l10n_util::GetStringUTF16(GetBubbleTitleMessageId()))
          .OverrideShowCloseButton(true)
          .DisableCloseOnDeactivate()
          .AddParagraph(ui::DialogModelLabel(
                            base::UTF8ToUTF16(primary_account.GetEmail())),
                        /*header=*/std::u16string(),
                        kAccountSwitchingBubblePrimaryAccountEmailElementId)
          .AddParagraph(ui::DialogModelLabel(
                            base::UTF8ToUTF16(target_account.GetEmail())),
                        /*header=*/std::u16string(),
                        kAccountSwitchingBubbleTargetAccountEmailElementId)
          .AddOkButton(base::BindOnce(&AccountSwitchingBubbleDelegate::OnAccept,
                                      base::Unretained(model_delegate_ptr)),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(
                               GetAcceptButtonLabelMessageId()))
                           .SetId(kAccountSwitchingBubbleAcceptButtonElementId))
          .AddCancelButton(
              base::BindOnce(&AccountSwitchingBubbleDelegate::OnDecline,
                             base::Unretained(model_delegate_ptr)),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_ACCOUNT_SWITCHING_BUBBLE_CANCEL_BUTTON_LABEL))
                  .SetId(kAccountSwitchingBubbleDeclineButtonElementId))
          // Do not register `SetCloseActionCallback`, as
          // `DialogDelegate::WindowWillClose()` invokes it on any
          // `Widget::Close()` (including programmatic
          // `ClosedReason::kUnspecified`). `OnWidgetClosed()` handles all
          // `Widget::Close()` closures synchronously via
          // `MakeCloseSynchronous()`, while `SetDialogDestroyingCallback()`
          // remains an idempotent fallback if the widget is destroyed directly
          // without `Close()`.
          .SetDialogDestroyingCallback(base::BindOnce(
              &AccountSwitchingBubbleDelegate::OnDialogDestroying,
              base::Unretained(model_delegate_ptr)))
          .Build();

  auto bubble_delegate = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor, views::BubbleBorder::TOP_RIGHT);
  return views::BubbleDialogDelegate::CreateBubble(
      bubble_delegate.release(),
      base::BindOnce(&AccountSwitchingBubbleDelegate::OnWidgetClosed,
                     model_delegate_ptr->GetWeakPtr(), std::move(on_close)));
}
