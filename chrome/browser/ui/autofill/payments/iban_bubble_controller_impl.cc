// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"

#include <string>

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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
#include "components/signin/public/identity_manager/identity_manager.h"
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
    const Iban& iban,
    bool should_show_prompt,
    payments::PaymentsAutofillClient::SaveIbanPromptCallback
        save_iban_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  iban_ = iban;
  is_reshow_ = false;
  is_upload_save_ = false;
  legal_message_lines_.clear();
  save_iban_prompt_callback_ = std::move(save_iban_prompt_callback);
  current_bubble_type_ = IbanBubbleType::kLocalSave;
  // Save callback should not be null for IBAN save.
  CHECK(!save_iban_prompt_callback_.is_null());

  if (should_show_prompt) {
    Show();
  } else {
    ShowIconOnly();
  }
}

void IbanBubbleControllerImpl::OfferUploadSave(
    const Iban& iban,
    LegalMessageLines legal_message_lines,
    bool should_show_prompt,
    payments::PaymentsAutofillClient::SaveIbanPromptCallback
        save_iban_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  iban_ = iban;
  is_reshow_ = false;
  is_upload_save_ = true;
  legal_message_lines_ = std::move(legal_message_lines);
  save_iban_prompt_callback_ = std::move(save_iban_prompt_callback);
  current_bubble_type_ = IbanBubbleType::kUploadSave;
  // Save callback should not be null for IBAN save.
  CHECK(!save_iban_prompt_callback_.is_null());
  if (should_show_prompt) {
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
  CHECK(current_bubble_type_ != IbanBubbleType::kInactive);
  if (current_bubble_type_ == IbanBubbleType::kLocalSave ||
      current_bubble_type_ == IbanBubbleType::kUploadSave) {
    CHECK(!save_iban_prompt_callback_.is_null());
  } else {
    CHECK(current_bubble_type_ == IbanBubbleType::kManageSavedIban);
  }
  Show();
}

void IbanBubbleControllerImpl::ShowConfirmationBubbleView(
    bool iban_saved,
    bool hit_max_strikes) {
  // Hide the current bubble if still showing.
  set_bubble_view(nullptr);

  is_reshow_ = false;
  current_bubble_type_ = IbanBubbleType::kUploadCompleted;
  confirmation_ui_params_ =
      iban_saved ? SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                       CreateForSaveIbanSuccess()
                 : SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                       CreateForSaveIbanFailure(hit_max_strikes);

  // Show upload confirmation bubble.
  AutofillBubbleHandler* autofill_bubble_handler =
      chrome::FindBrowserWithTab(web_contents())
          ->window()
          ->GetAutofillBubbleHandler();
  set_bubble_view(autofill_bubble_handler->ShowSaveIbanConfirmationBubble(
      web_contents(), this));
  // Auto close confirmation bubble when IBAN saved is successful.
  if (iban_saved) {
    auto_close_confirmation_timer_.Start(
        FROM_HERE, kAutoCloseConfirmationBubbleWaitSec,
        base::BindOnce(&IbanBubbleControllerImpl::HideBubble,
                       base::Unretained(this)));
  }
}

std::u16string IbanBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_LOCAL);
    case IbanBubbleType::kUploadSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_SERVER);
    case IbanBubbleType::kManageSavedIban:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_IBAN_SAVED);
    case IbanBubbleType::kUploadCompleted:
    case IbanBubbleType::kInactive:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string IbanBubbleControllerImpl::GetExplanatoryMessage() const {
  if (current_bubble_type_ == IbanBubbleType::kUploadSave) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_UPLOAD_IBAN_PROMPT_EXPLANATION);
  }
  return std::u16string();
}

std::u16string IbanBubbleControllerImpl::GetAcceptButtonText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
    case IbanBubbleType::kUploadSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_BUBBLE_SAVE_ACCEPT);
    case IbanBubbleType::kManageSavedIban:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DONE);
    case IbanBubbleType::kUploadCompleted:
    case IbanBubbleType::kInactive:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string IbanBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
    case IbanBubbleType::kUploadSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_BUBBLE_SAVE_NO_THANKS);
    case IbanBubbleType::kManageSavedIban:
    case IbanBubbleType::kUploadCompleted:
    case IbanBubbleType::kInactive:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

AccountInfo IbanBubbleControllerImpl::GetAccountInfo() {
  // The results of this call should not be cached because the user can update
  // their account info at any time.
  Profile* profile = GetProfile();
  if (!profile) {
    return AccountInfo();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return AccountInfo();
  }
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserContext(profile);
  if (!personal_data_manager) {
    return AccountInfo();
  }

  return identity_manager->FindExtendedAccountInfo(
      personal_data_manager->payments_data_manager()
          .GetAccountInfoForPaymentsServer());
}

const Iban& IbanBubbleControllerImpl::GetIban() const {
  return iban_;
}

base::OnceCallback<void(PaymentsBubbleClosedReason)>
IbanBubbleControllerImpl::GetOnBubbleClosedCallback() {
  return base::BindOnce(&IbanBubbleControllerImpl::OnBubbleClosed,
                        weak_ptr_factory_.GetWeakPtr());
}

void IbanBubbleControllerImpl::OnAcceptButton(const std::u16string& nickname) {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      CHECK(!save_iban_prompt_callback_.is_null());
      // Show an animated IBAN saved confirmation message next time
      // UpdatePageActionIcon() is called.
      should_show_iban_saved_label_animation_ = true;
      autofill_metrics::LogSaveIbanBubbleResultSavedWithNicknameMetric(
          !nickname.empty(), /*is_upload_save=*/false);
      iban_.set_nickname(nickname);
      std::move(save_iban_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::
                   kAccepted,
               nickname);
      return;
    case IbanBubbleType::kUploadSave:
      CHECK(!save_iban_prompt_callback_.is_null());
      autofill_metrics::LogSaveIbanBubbleResultSavedWithNicknameMetric(
          !nickname.empty(), /*is_upload_save=*/true);
      iban_.set_nickname(nickname);
      std::move(save_iban_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::
                   kAccepted,
               nickname);
      return;
    case IbanBubbleType::kManageSavedIban:
      return;
    case IbanBubbleType::kUploadCompleted:
    case IbanBubbleType::kInactive:
      NOTREACHED_IN_MIGRATION();
  }
}

void IbanBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void IbanBubbleControllerImpl::OnManageSavedIbanExtraButtonClicked() {
  CHECK(current_bubble_type_ == IbanBubbleType::kManageSavedIban);
  chrome::ShowSettingsSubPage(chrome::FindBrowserWithTab(web_contents()),
                              chrome::kPaymentsSubPage);
  OnBubbleClosed(PaymentsBubbleClosedReason::kClosed);
}

void IbanBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  if (current_bubble_type_ == IbanBubbleType::kLocalSave ||
      current_bubble_type_ == IbanBubbleType::kUploadSave) {
    if (closed_reason == PaymentsBubbleClosedReason::kCancelled) {
      std::move(save_iban_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::
                   kDeclined,
               /*nickname=*/u"");
    } else if (closed_reason == PaymentsBubbleClosedReason::kClosed) {
      std::move(save_iban_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::
                   kIgnored,
               /*nickname=*/u"");
    }
  }

  set_bubble_view(nullptr);

  // Log save IBAN prompt result according to the closed reason.
  if (current_bubble_type_ == IbanBubbleType::kLocalSave ||
      current_bubble_type_ == IbanBubbleType::kUploadSave) {
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
        NOTREACHED_IN_MIGRATION();
        break;
    }
    autofill_metrics::LogSaveIbanBubbleResultMetric(
        metric, is_reshow_,
        /*is_upload_save=*/current_bubble_type_ == IbanBubbleType::kUploadSave);
  }

  if (current_bubble_type_ == IbanBubbleType::kUploadCompleted) {
    current_bubble_type_ = IbanBubbleType::kInactive;

    UpdatePageActionIcon();
    confirmation_ui_params_.reset();
    return;
  }

  // Handles `current_bubble_type_` change according to its current type and the
  // `closed_reason`.
  if (closed_reason == PaymentsBubbleClosedReason::kAccepted) {
    if (current_bubble_type_ == IbanBubbleType::kLocalSave ||
        current_bubble_type_ == IbanBubbleType::kUploadSave) {
      current_bubble_type_ = IbanBubbleType::kManageSavedIban;
    } else {
      current_bubble_type_ = IbanBubbleType::kInactive;
    }
  } else if (closed_reason == PaymentsBubbleClosedReason::kCancelled) {
    current_bubble_type_ = IbanBubbleType::kInactive;
  }
  UpdatePageActionIcon();
  confirmation_ui_params_.reset();
}

IbanBubbleControllerImpl::IbanBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<IbanBubbleControllerImpl>(*web_contents),
      personal_data_manager_(PersonalDataManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {}

IbanBubbleType IbanBubbleControllerImpl::GetBubbleType() const {
  return current_bubble_type_;
}

std::u16string IbanBubbleControllerImpl::GetSavePaymentIconTooltipText() const {
  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
    case IbanBubbleType::kUploadSave:
    case IbanBubbleType::kManageSavedIban:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_IBAN);
    case IbanBubbleType::kUploadCompleted:
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

int IbanBubbleControllerImpl::GetSaveSuccessAnimationStringId() const {
  return IDS_AUTOFILL_IBAN_SAVED;
}

const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
IbanBubbleControllerImpl::GetConfirmationUiParams() const {
  CHECK(confirmation_ui_params_.has_value());
  return confirmation_ui_params_.value();
}

PageActionIconType IbanBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveIban;
}

void IbanBubbleControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  AutofillBubbleHandler* autofill_bubble_handler =
      browser->window()->GetAutofillBubbleHandler();
  set_bubble_view(autofill_bubble_handler->ShowIbanBubble(
      web_contents(), this, /*is_user_gesture=*/is_reshow_,
      current_bubble_type_));
  CHECK(bubble_view());
  CHECK(current_bubble_type_ != IbanBubbleType::kInactive);

  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      autofill_metrics::LogSaveIbanBubbleOfferMetric(
          autofill_metrics::SaveIbanPromptOffer::kShown, is_reshow_,
          /*is_upload_save=*/false);
      break;
    case IbanBubbleType::kUploadSave:
      autofill_metrics::LogSaveIbanBubbleOfferMetric(
          autofill_metrics::SaveIbanPromptOffer::kShown, is_reshow_,
          /*is_upload_save=*/true);
      break;
    case IbanBubbleType::kManageSavedIban:
      // TODO(crbug.com/40233611): Add metrics for manage saved IBAN mode.
      break;
    case IbanBubbleType::kUploadCompleted:
    case IbanBubbleType::kInactive:
      NOTREACHED_IN_MIGRATION();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

Profile* IbanBubbleControllerImpl::GetProfile() {
  if (!web_contents()) {
    return nullptr;
  }
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

void IbanBubbleControllerImpl::ShowIconOnly() {
  CHECK(current_bubble_type_ != IbanBubbleType::kInactive);
  if (current_bubble_type_ == IbanBubbleType::kLocalSave ||
      current_bubble_type_ == IbanBubbleType::kUploadSave) {
    CHECK(!save_iban_prompt_callback_.is_null());
  } else {
    CHECK(current_bubble_type_ == IbanBubbleType::kManageSavedIban);
  }
  CHECK(!bubble_view());

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdatePageActionIcon();

  switch (current_bubble_type_) {
    case IbanBubbleType::kLocalSave:
      autofill_metrics::LogSaveIbanBubbleOfferMetric(
          autofill_metrics::SaveIbanPromptOffer::kNotShownMaxStrikesReached,
          is_reshow_, /*is_upload_save=*/false);
      break;
    case IbanBubbleType::kUploadSave:
      autofill_metrics::LogSaveIbanBubbleOfferMetric(
          autofill_metrics::SaveIbanPromptOffer::kNotShownMaxStrikesReached,
          is_reshow_, /*is_upload_save=*/true);
      break;
    case IbanBubbleType::kManageSavedIban:
    case IbanBubbleType::kUploadCompleted:
      break;
    case IbanBubbleType::kInactive:
      NOTREACHED_IN_MIGRATION();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnIconShown();
  }
}

bool IbanBubbleControllerImpl::IsUploadSave() const {
  return is_upload_save_;
}

const LegalMessageLines& IbanBubbleControllerImpl::GetLegalMessageLines()
    const {
  return legal_message_lines_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IbanBubbleControllerImpl);

}  // namespace autofill
