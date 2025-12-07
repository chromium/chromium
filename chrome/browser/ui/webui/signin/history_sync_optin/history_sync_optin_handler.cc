// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"

#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom-data-view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace {

using history_sync_optin::mojom::ScreenMode;

constexpr char kSigninAccountCapabilitiesUserVisibleLatency[] =
    "Signin.AccountCapabilities.UserVisibleLatency";
constexpr char kSigninAccountCapabilitiesFetchLatency[] =
    "Signin.AccountCapabilities.FetchLatency";
constexpr char kSigninAccountCapabilitiesImmediatelyAvailable[] =
    "Signin.AccountCapabilities.ImmediatelyAvailable";
constexpr char kSigninSyncButtonsShown[] = "Signin.SyncButtons.Shown";

enum class ButtonType : bool { kAccept = true, kReject = false };

ScreenMode GetHistorySyncScreenMode(const AccountCapabilities& capabilities) {
  switch (
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions()) {
    case signin::Tribool::kUnknown:
      return ScreenMode::kPending;
    case signin::Tribool::kFalse:
      return ScreenMode::kRestricted;
    case signin::Tribool::kTrue:
      return ScreenMode::kUnrestricted;
  }
}

// Convert ScreenMode to the metric describing Accept/Reject button types.
signin_metrics::SyncButtonsType GetButtonTypeMetricValue(ScreenMode mode) {
  switch (mode) {
    case ScreenMode::kRestricted:
      return signin_metrics::SyncButtonsType::kSyncEqualWeightedFromCapability;
    case ScreenMode::kDeadlined:
      return signin_metrics::SyncButtonsType::kSyncEqualWeightedFromDeadline;
    case ScreenMode::kUnrestricted:
      return signin_metrics::SyncButtonsType::kSyncNotEqualWeighted;
    // Metrics are not emitted when the buttons are not visible.
    case ScreenMode::kPending:
      NOTREACHED();
  }
}

history_sync_optin::mojom::AccountInfoPtr CreateAccountInfoDataMojo(
    const AccountInfo& info) {
  history_sync_optin::mojom::AccountInfoPtr account_info_mojo =
      history_sync_optin::mojom::AccountInfo::New();
  account_info_mojo->account_image_src =
      GURL(signin::GetAccountPictureUrl(info));
  return account_info_mojo;
}

}  // namespace

HistorySyncOptinHandler::HistorySyncOptinHandler(
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver,
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    Browser* browser,
    Profile* profile,
    std::optional<bool> should_close_modal_dialog,
    HistorySyncOptinHelper::FlowCompletedCallback
        history_optin_completed_callback)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser ? browser->AsWeakPtr() : nullptr),
      profile_(profile),
      should_close_modal_dialog_(should_close_modal_dialog),
      history_optin_completed_callback_(
          std::move(history_optin_completed_callback)),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)) {
  CHECK(profile_);
  CHECK(identity_manager_);
  CHECK(!history_optin_completed_callback_->is_null());
  if (browser) {
    CHECK(should_close_modal_dialog.has_value());
  }
}

HistorySyncOptinHandler::~HistorySyncOptinHandler() {
  if (!history_optin_completed_callback_->is_null()) {
    // Runs the callback in case the dialog is not dismissed via the buttons,
    // but e.g. using an accelerator or close button.
    std::move(history_optin_completed_callback_.value())
        .Run(HistorySyncOptinHelper::ScreenChoiceResult::kDismissed);
  }
}

void HistorySyncOptinHandler::Accept() {
  AddHistorySyncConsent();
  FinishAndCloseDialog(HistorySyncOptinHelper::ScreenChoiceResult::kAccepted);
}

void HistorySyncOptinHandler::Reject() {
  FinishAndCloseDialog(HistorySyncOptinHelper::ScreenChoiceResult::kDeclined);
}

void HistorySyncOptinHandler::RequestAccountInfo() {
  MaybeGetAccountInfo();
}

void HistorySyncOptinHandler::MaybeGetAccountInfo() {
  AccountInfo primary_account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  if (!primary_account_info.IsEmpty()) {
    DispatchAccountInfoUpdate(primary_account_info);
    if (avatar_changed_ && screen_mode_changed_) {
      // Both avatar and screen mode are immediately available.
      identity_manager_observation_.Reset();
      return;
    }
  }

  if (!screen_mode_changed_) {
    // If capabilities are still being fetched, the screen mode remains pending.
    // Start a timer to fall back to a default mode. This prevents the dialog
    // from being stuck if the capabilities fetch is slow or fails.
    CHECK(!user_visible_latency_.has_value());
    user_visible_latency_.emplace();
    screen_mode_timeout_.Start(FROM_HERE,
                               signin::GetMinorModeRestrictionsDeadline(), this,
                               &HistorySyncOptinHandler::OnScreenModeTimeout);
  }

  if (!identity_manager_observation_.IsObserving()) {
    identity_manager_observation_.Observe(identity_manager_);
  }
}

void HistorySyncOptinHandler::UpdateDialogHeight(uint32_t height) {
  if (browser_) {
    browser_->GetFeatures().signin_view_controller()->SetModalSigninHeight(
        height);
  }
}

void HistorySyncOptinHandler::FinishAndCloseDialog(
    HistorySyncOptinHelper::ScreenChoiceResult result) {
  // The callback is moved to a local variable to ensure that it can be safely
  // executed even if `this` is destroyed by `CloseModalSignin()` (this should
  // not happen as the dialog destruction should be asynchronous).
  auto callback = std::move(history_optin_completed_callback_);

  if (browser_ && should_close_modal_dialog_.value_or(false)) {
    browser_->GetFeatures().signin_view_controller()->CloseModalSignin();
  }
  if (!callback->is_null()) {
    std::move(callback.value()).Run(result);
  } else {
    // The user may have double-clicked on an action, which could have
    // caused the callback to execute already.
    // TODO(crbug.com/456458942): Disabled the buttons so that this is not
    // possible. Convert back to a check after we verify we no longer hit this.
    base::debug::DumpWithoutCrashing();
  }
}

void HistorySyncOptinHandler::AddHistorySyncConsent() {
  CHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  signin_util::EnableHistorySync(SyncServiceFactory::GetForProfile(profile_));
}

void HistorySyncOptinHandler::OnScreenModeChanged(ScreenMode screen_mode) {
  CHECK_EQ(screen_mode_, ScreenMode::kPending);
  CHECK_NE(screen_mode, ScreenMode::kPending);
  CHECK(!screen_mode_changed_) << "Must be called only once";
  screen_mode_timeout_.Stop();
  screen_mode_ = screen_mode;
  screen_mode_changed_ = true;

  page_->SendScreenMode(screen_mode_);

  if (user_visible_latency_.has_value()) {
    base::TimeDelta elapsed = user_visible_latency_->Elapsed();
    base::UmaHistogramTimes(kSigninAccountCapabilitiesUserVisibleLatency,
                            elapsed);
    base::UmaHistogramTimes(kSigninAccountCapabilitiesFetchLatency, elapsed);
    base::UmaHistogramBoolean(kSigninAccountCapabilitiesImmediatelyAvailable,
                              false);
  } else {
    base::UmaHistogramTimes(kSigninAccountCapabilitiesUserVisibleLatency,
                            base::Seconds(0));
    base::UmaHistogramBoolean(kSigninAccountCapabilitiesImmediatelyAvailable,
                              true);
  }

  base::UmaHistogramEnumeration(kSigninSyncButtonsShown,
                                GetButtonTypeMetricValue(screen_mode_));
}

void HistorySyncOptinHandler::OnAvatarChanged(const AccountInfo& info) {
  CHECK(info.IsValid());
  avatar_changed_ = true;
  page_->SendAccountInfo(CreateAccountInfoDataMojo(info));
}

void HistorySyncOptinHandler::DispatchAccountInfoUpdate(
    const AccountInfo& info) {
  if (info.IsEmpty()) {
    // No account is signed in, so there is nothing to be displayed in the sync
    // confirmation dialog.
    return;
  }

  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }

  ScreenMode screen_mode = GetHistorySyncScreenMode(info.capabilities);
  if (!screen_mode_changed_ && screen_mode != ScreenMode::kPending) {
    OnScreenModeChanged(screen_mode);
  }

  if (info.IsValid() && !avatar_changed_) {
    OnAvatarChanged(info);
  }
}

void HistorySyncOptinHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DispatchAccountInfoUpdate(info);

  if (avatar_changed_ && screen_mode_changed_) {
    // The IdentityManager emitted both avatar and screen mode information.
    identity_manager_observation_.Reset();
  }
}

void HistorySyncOptinHandler::OnScreenModeTimeout() {
  if (screen_mode_changed_) {
    return;
  }

  // Default to kDeadlined if capabilities cannot be fetched in time,
  // ensuring the more cautious button presentation (equally weighted).
  OnScreenModeChanged(ScreenMode::kDeadlined);
}
