// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"

#include <string>

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// static
IbanBubbleController* IbanBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  IbanBubbleControllerImpl::CreateForWebContents(web_contents);
  return IbanBubbleControllerImpl::FromWebContents(web_contents);
}

IbanBubbleControllerImpl::~IbanBubbleControllerImpl() = default;

void IbanBubbleControllerImpl::OfferLocalSave(
    const IBAN& iban,
    bool should_show_prompt,
    AutofillClient::LocalSaveIBANPromptCallback save_iban_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  iban_ = iban;
  is_reshow_ = false;
  local_save_iban_prompt_callback_ = std::move(save_iban_prompt_callback);
  current_bubble_type_ = IbanBubbleType::kLocalSave;

  if (should_show_prompt) {
    // Local save callback should not be null for kLocalSave state.
    DCHECK(!local_save_iban_prompt_callback_.is_null());
    Show();
  } else {
    ShowIconOnly();
  }
}

void IbanBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  is_reshow_ = true;
  DCHECK(current_bubble_type_ != IbanBubbleType::kInactive);
  // Local save callback should not be null for kLocalSave state.
  DCHECK(!local_save_iban_prompt_callback_.is_null() ||
         current_bubble_type_ != IbanBubbleType::kLocalSave);
  Show();
}

std::u16string IbanBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_LOCAL);
    case IbanBubbleType::kManageSavedIban:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_IBAN_SAVED);
    case IbanBubbleType::kInactive:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string IbanBubbleControllerImpl::GetAcceptButtonText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_BUBBLE_LOCAL_SAVE_ACCEPT);
    case IbanBubbleType::kManageSavedIban:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DONE);
    case IbanBubbleType::kInactive:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string IbanBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_BUBBLE_LOCAL_SAVE_NO_THANKS);
    case IbanBubbleType::kManageSavedIban:
    case IbanBubbleType::kInactive:
      NOTREACHED();
      return std::u16string();
  }
}

const IBAN& IbanBubbleControllerImpl::GetIBAN() const {
  return iban_;
}

void IbanBubbleControllerImpl::OnAcceptButton(const std::u16string& nickname) {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      DCHECK(!local_save_iban_prompt_callback_.is_null());
      // Show an animated IBAN saved confirmation message next time
      // UpdatePageActionIcon() is called.
      should_show_iban_saved_label_animation_ = true;
      autofill_metrics::LogSaveIbanBubbleResultSavedWithNicknameMetric(
          !nickname.empty());
      iban_.set_nickname(nickname);
      std::move(local_save_iban_prompt_callback_)
          .Run(AutofillClient::SaveIBANOfferUserDecision::kAccepted, nickname);
      return;
    case IbanBubbleType::kManageSavedIban:
      return;
    case IbanBubbleType::kInactive:
      NOTREACHED();
  }
}

void IbanBubbleControllerImpl::OnManageSavedIbanExtraButtonClicked() {
  DCHECK(current_bubble_type_ == IbanBubbleType::kManageSavedIban);
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPaymentsSubPage);
  OnBubbleClosed(PaymentsBubbleClosedReason::kClosed);
}

void IbanBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  if (current_bubble_type_ == IbanBubbleType::kLocalSave) {
    if (closed_reason == PaymentsBubbleClosedReason::kCancelled) {
      std::move(local_save_iban_prompt_callback_)
          .Run(AutofillClient::SaveIBANOfferUserDecision::kDeclined,
               absl::nullopt);
    } else if (closed_reason == PaymentsBubbleClosedReason::kClosed) {
      std::move(local_save_iban_prompt_callback_)
          .Run(AutofillClient::SaveIBANOfferUserDecision::kIgnored,
               absl::nullopt);
    }
  }

  set_bubble_view(nullptr);

  // Log save IBAN prompt result according to the closed reason.
  if (current_bubble_type_ == IbanBubbleType::kLocalSave) {
    autofill_metrics::SaveIbanBubbleResult metric;
    switch (closed_reason) {
      case PaymentsBubbleClosedReason::kAccepted:
        metric = autofill_metrics::SaveIbanBubbleResult::kAccepted;
        break;
      case PaymentsBubbleClosedReason::kCancelled:
        metric = autofill_metrics::SaveIbanBubbleResult::kCancelled;
        break;
      case PaymentsBubbleClosedReason::kClosed:
        metric = autofill_metrics::SaveIbanBubbleResult::kClosed;
        break;
      case PaymentsBubbleClosedReason::kNotInteracted:
        metric = autofill_metrics::SaveIbanBubbleResult::kNotInteracted;
        break;
      case PaymentsBubbleClosedReason::kLostFocus:
        metric = autofill_metrics::SaveIbanBubbleResult::kLostFocus;
        break;
      case PaymentsBubbleClosedReason::kUnknown:
        metric = autofill_metrics::SaveIbanBubbleResult::kUnknown;
        NOTREACHED();
        break;
    }
    autofill_metrics::LogSaveIbanBubbleResultMetric(metric, is_reshow_);
  }

  // Handles `current_bubble_type_` change according to its current type and the
  // `closed_reason`.
  if (closed_reason == PaymentsBubbleClosedReason::kAccepted) {
    if (current_bubble_type_ == IbanBubbleType::kLocalSave) {
      current_bubble_type_ = IbanBubbleType::kManageSavedIban;
    } else {
      current_bubble_type_ = IbanBubbleType::kInactive;
    }
  } else if (closed_reason == PaymentsBubbleClosedReason::kCancelled) {
    current_bubble_type_ = IbanBubbleType::kInactive;
  }
  UpdatePageActionIcon();
}

IbanBubbleControllerImpl::IbanBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<IbanBubbleControllerImpl>(*web_contents),
      personal_data_manager_(
          PersonalDataManagerFactory::GetInstance()->GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
}

IbanBubbleType IbanBubbleControllerImpl::GetBubbleType() const {
  return current_bubble_type_;
}

std::u16string IbanBubbleControllerImpl::GetSavePaymentIconTooltipText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
    case IbanBubbleType::kManageSavedIban:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_IBAN);
    case IbanBubbleType::kInactive:
      return std::u16string();
  }
}

bool IbanBubbleControllerImpl::ShouldShowSavingPaymentAnimation() const {
  return false;
}

bool IbanBubbleControllerImpl::ShouldShowPaymentSavedLabelAnimation() const {
  return should_show_iban_saved_label_animation_;
}

bool IbanBubbleControllerImpl::ShouldShowSaveFailureBadge() const {
  return false;
}

void IbanBubbleControllerImpl::OnAnimationEnded() {
  // Do not repeat the animation next time UpdatePageActionIcon() is called,
  // unless explicitly set somewhere else.
  should_show_iban_saved_label_animation_ = false;
}

bool IbanBubbleControllerImpl::IsIconVisible() const {
  // If there is no bubble to show, then there should be no icon.
  return current_bubble_type_ != IbanBubbleType::kInactive;
}

AutofillBubbleBase* IbanBubbleControllerImpl::GetPaymentBubbleView() const {
  return bubble_view();
}

SavePaymentIconController::PaymentBubbleType
IbanBubbleControllerImpl::GetPaymentBubbleType() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return PaymentBubbleType::kSaveIban;
    case IbanBubbleType::kManageSavedIban:
      return PaymentBubbleType::kManageSavedIban;
    case IbanBubbleType::kInactive:
      return PaymentBubbleType::kUnknown;
  }
}

int IbanBubbleControllerImpl::GetSaveSuccessAnimationStringId() const {
  return IDS_AUTOFILL_IBAN_SAVED;
}

PageActionIconType IbanBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveIban;
}

void IbanBubbleControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  AutofillBubbleHandler* autofill_bubble_handler =
      browser->window()->GetAutofillBubbleHandler();
  set_bubble_view(autofill_bubble_handler->ShowIbanBubble(
      web_contents(), this, /*is_user_gesture=*/is_reshow_,
      current_bubble_type_));
  DCHECK(bubble_view());
  DCHECK(current_bubble_type_ != IbanBubbleType::kInactive);

  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      autofill_metrics::LogSaveIbanBubbleOfferMetric(
          autofill_metrics::SaveIbanPromptOffer::kShown, is_reshow_);
      break;
    case IbanBubbleType::kManageSavedIban:
      // TODO(crbug.com/1349109): Add metrics for manage saved IBAN mode.
      break;
    case IbanBubbleType::kInactive:
      NOTREACHED();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

void IbanBubbleControllerImpl::ShowIconOnly() {
  DCHECK(current_bubble_type_ != IbanBubbleType::kInactive);
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!local_save_iban_prompt_callback_.is_null() ||
         current_bubble_type_ != IbanBubbleType::kLocalSave);
  DCHECK(!bubble_view());

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdatePageActionIcon();

  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      autofill_metrics::LogSaveIbanBubbleOfferMetric(
          autofill_metrics::SaveIbanPromptOffer::kNotShownMaxStrikesReached,
          is_reshow_);
      break;
    case IbanBubbleType::kManageSavedIban:
      break;
    case IbanBubbleType::kInactive:
      NOTREACHED();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnIconShown();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IbanBubbleControllerImpl);

}  // namespace autofill
