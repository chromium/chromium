// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/metrics/payments/manage_cards_prompt_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<SaveCardBubbleControllerImpl>(
          *web_contents) {
  security_level_ =
      SecurityStateTabHelper::FromWebContents(web_contents)->GetSecurityLevel();

  personal_data_manager_ =
      PersonalDataManagerFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() = default;

// static
SaveCardBubbleController* SaveCardBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents);
  return SaveCardBubbleControllerImpl::FromWebContents(web_contents);
}

// static
SaveCardBubbleController* SaveCardBubbleController::Get(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return SaveCardBubbleControllerImpl::FromWebContents(web_contents);
}

void SaveCardBubbleControllerImpl::OfferLocalSave(
    const CreditCard& card,
    AutofillClient::SaveCreditCardOptions options,
    AutofillClient::LocalSaveCardPromptCallback save_card_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;

  is_upload_save_ = false;
  is_reshow_ = false;
  options_ = options;
  legal_message_lines_.clear();

  card_ = card;
  local_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ = BubbleType::LOCAL_SAVE;

  if (options.show_prompt)
    ShowBubble();
  else
    ShowIconOnly();
}

void SaveCardBubbleControllerImpl::OfferUploadSave(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    AutofillClient::SaveCreditCardOptions options,
    AutofillClient::UploadSaveCardPromptCallback save_card_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;

  is_upload_save_ = true;
  is_reshow_ = false;
  options_ = options;
  card_ = card;
  upload_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ = BubbleType::UPLOAD_SAVE;
  legal_message_lines_ = legal_message_lines;

  if (options_.show_prompt)
    ShowBubble();
  else
    ShowIconOnly();
}

// Exists for testing purposes only.
void SaveCardBubbleControllerImpl::ShowBubbleForManageCardsForTesting(
    const CreditCard& card) {
  card_ = card;
  current_bubble_type_ = BubbleType::MANAGE_CARDS;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::UpdateIconForSaveCardSuccess() {
  current_bubble_type_ = BubbleType::INACTIVE;
  UpdatePageActionIcon();
}

void SaveCardBubbleControllerImpl::UpdateIconForSaveCardFailure() {
  current_bubble_type_ = BubbleType::FAILURE;
  ShowIconOnly();
}

void SaveCardBubbleControllerImpl::ShowBubbleForSaveCardFailureForTesting() {
  current_bubble_type_ = BubbleType::FAILURE;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;

  is_reshow_ = true;
  ShowBubble();
}

std::u16string SaveCardBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
    case BubbleType::UPLOAD_SAVE:
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableNewSaveCardBubbleUi)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V5);
      }
      return features::ShouldShowImprovedUserConsentForCreditCardSave()
                 ? l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V4)
                 : l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::FAILURE:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_FAILURE_BUBBLE_TITLE);
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::INACTIVE:
      NOTREACHED();
      return std::u16string();
  }
}

std::u16string SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  if (current_bubble_type_ == BubbleType::FAILURE)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_FAILURE_BUBBLE_EXPLANATION);

  if (current_bubble_type_ != BubbleType::UPLOAD_SAVE)
    return std::u16string();

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableNewSaveCardBubbleUi)) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V4);
  }

  if (options_.should_request_name_from_user) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_NAME);
  }

  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3);
}

std::u16string SaveCardBubbleControllerImpl::GetAcceptButtonText() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_LOCAL_SAVE_ACCEPT);
    case BubbleType::UPLOAD_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_UPLOAD_SAVE_ACCEPT);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DONE);
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      return std::u16string();
  }
}

std::u16string SaveCardBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_LOCAL_SAVE);
    case BubbleType::UPLOAD_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_UPLOAD_SAVE);
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::MANAGE_CARDS:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      return std::u16string();
  }
}

const AccountInfo& SaveCardBubbleControllerImpl::GetAccountInfo() {
  if (account_info_.IsEmpty())
    FetchAccountInfo();

  return account_info_;
}

Profile* SaveCardBubbleControllerImpl::GetProfile() const {
  if (!web_contents())
    return nullptr;
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

const CreditCard& SaveCardBubbleControllerImpl::GetCard() const {
  return card_;
}

bool SaveCardBubbleControllerImpl::ShouldRequestNameFromUser() const {
  return options_.should_request_name_from_user;
}

bool SaveCardBubbleControllerImpl::ShouldRequestExpirationDateFromUser() const {
  return options_.should_request_expiration_date_from_user;
}

void SaveCardBubbleControllerImpl::OnSaveButton(
    const AutofillClient::UserProvidedCardDetails& user_provided_card_details) {
  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE: {
      DCHECK(!upload_save_card_prompt_callback_.is_null());
      if (auto* sentiment_service =
              TrustSafetySentimentServiceFactory::GetForProfile(GetProfile())) {
        sentiment_service->SavedCard();
      }
      std::u16string name_provided_by_user;
      if (!user_provided_card_details.cardholder_name.empty()) {
        // Log whether the name was changed by the user or simply accepted
        // without edits.
        autofill_metrics::LogSaveCardCardholderNameWasEdited(
            user_provided_card_details.cardholder_name !=
            base::UTF8ToUTF16(GetAccountInfo().full_name));
        // Trim the cardholder name provided by the user and send it in the
        // callback so it can be included in the final request.
        DCHECK(ShouldRequestNameFromUser());
        base::TrimWhitespace(user_provided_card_details.cardholder_name,
                             base::TRIM_ALL, &name_provided_by_user);
      }
      std::move(upload_save_card_prompt_callback_)
          .Run(AutofillClient::SaveCardOfferUserDecision::kAccepted,
               user_provided_card_details);
      break;
    }
    case BubbleType::LOCAL_SAVE:
      DCHECK(!local_save_card_prompt_callback_.is_null());
      if (auto* sentiment_service =
              TrustSafetySentimentServiceFactory::GetForProfile(GetProfile())) {
        sentiment_service->SavedCard();
      }
      // Show an animated card saved confirmation message next time
      // UpdatePageActionIcon() is called.
      should_show_card_saved_label_animation_ = true;
      std::move(local_save_card_prompt_callback_)
          .Run(AutofillClient::SaveCardOfferUserDecision::kAccepted);
      break;
    case BubbleType::MANAGE_CARDS:
      LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsDone,
                                 is_upload_save_);
      return;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }
}

void SaveCardBubbleControllerImpl::OnCancelButton() {
  if (current_bubble_type_ == BubbleType::LOCAL_SAVE) {
    std::move(local_save_card_prompt_callback_)
        .Run(AutofillClient::SaveCardOfferUserDecision::kDeclined);
  } else if (current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    std::move(upload_save_card_prompt_callback_)
        .Run(AutofillClient::SaveCardOfferUserDecision::kDeclined, {});
  }
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  autofill_metrics::LogCreditCardUploadLegalMessageLinkClicked();
}

void SaveCardBubbleControllerImpl::OnManageCardsClicked() {
  DCHECK(current_bubble_type_ == BubbleType::MANAGE_CARDS);

  LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsManageCards,
                             is_upload_save_);

  ShowPaymentsSettingsPage();
}

void SaveCardBubbleControllerImpl::ShowPaymentsSettingsPage() {
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPaymentsSubPage);
}

void SaveCardBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);

  // Log save card prompt result according to the closed reason.
  if ((current_bubble_type_ == BubbleType::LOCAL_SAVE ||
       current_bubble_type_ == BubbleType::UPLOAD_SAVE)) {
    autofill_metrics::SaveCardPromptResult metric;
    switch (closed_reason) {
      case PaymentsBubbleClosedReason::kAccepted:
        metric = autofill_metrics::SaveCardPromptResult::kAccepted;
        break;
      case PaymentsBubbleClosedReason::kCancelled:
        metric = autofill_metrics::SaveCardPromptResult::kCancelled;
        break;
      case PaymentsBubbleClosedReason::kClosed:
        metric = autofill_metrics::SaveCardPromptResult::kClosed;
        break;
      case PaymentsBubbleClosedReason::kNotInteracted:
        metric = autofill_metrics::SaveCardPromptResult::kNotInteracted;
        break;
      case PaymentsBubbleClosedReason::kLostFocus:
        metric = autofill_metrics::SaveCardPromptResult::kLostFocus;
        break;
      case PaymentsBubbleClosedReason::kUnknown:
        metric = autofill_metrics::SaveCardPromptResult::kUnknown;
        break;
    }
    autofill_metrics::LogSaveCardPromptResultMetric(
        metric, is_upload_save_, is_reshow_, options_, GetSecurityLevel(),
        GetSyncState());
  }

  // Handles |current_bubble_type_| change according to its current type and the
  // |closed_reason|.
  if (closed_reason == PaymentsBubbleClosedReason::kAccepted) {
    if (current_bubble_type_ == BubbleType::LOCAL_SAVE) {
      current_bubble_type_ = BubbleType::MANAGE_CARDS;
    } else if (current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
      current_bubble_type_ = BubbleType::INACTIVE;
    } else {
      DCHECK_EQ(current_bubble_type_, BubbleType::MANAGE_CARDS);
      current_bubble_type_ = BubbleType::INACTIVE;
    }
  } else if (closed_reason == PaymentsBubbleClosedReason::kCancelled) {
    current_bubble_type_ = BubbleType::INACTIVE;
  } else if (current_bubble_type_ == BubbleType::FAILURE) {
    // Unlike other bubbles, the save failure bubble should not be reshown. If
    // the save card failure bubble is closed, the credit card icon should be
    // dismissed as well.
    current_bubble_type_ = BubbleType::INACTIVE;
  }

  UpdatePageActionIcon();
}

const LegalMessageLines& SaveCardBubbleControllerImpl::GetLegalMessageLines()
    const {
  return legal_message_lines_;
}

bool SaveCardBubbleControllerImpl::IsUploadSave() const {
  return is_upload_save_;
}

BubbleType SaveCardBubbleControllerImpl::GetBubbleType() const {
  return current_bubble_type_;
}

AutofillSyncSigninState SaveCardBubbleControllerImpl::GetSyncState() const {
  return personal_data_manager_->GetSyncSigninState();
}

std::u16string SaveCardBubbleControllerImpl::GetSavePaymentIconTooltipText()
    const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD);
    case BubbleType::UPLOAD_IN_PROGRESS:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD_PENDING);
    case BubbleType::FAILURE:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD_FAILURE);
    case BubbleType::INACTIVE:
      return std::u16string();
  }
}

bool SaveCardBubbleControllerImpl::ShouldShowSavingPaymentAnimation() const {
  return current_bubble_type_ == BubbleType::UPLOAD_IN_PROGRESS;
}

bool SaveCardBubbleControllerImpl::ShouldShowPaymentSavedLabelAnimation()
    const {
  return should_show_card_saved_label_animation_;
}

bool SaveCardBubbleControllerImpl::ShouldShowSaveFailureBadge() const {
  return current_bubble_type_ == BubbleType::FAILURE;
}

void SaveCardBubbleControllerImpl::OnAnimationEnded() {
  // Do not repeat the animation next time UpdatePageActionIcon() is called,
  // unless explicitly set somewhere else.
  should_show_card_saved_label_animation_ = false;
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  // If there is no bubble to show, then there should be no icon.
  return current_bubble_type_ != BubbleType::INACTIVE;
}

AutofillBubbleBase* SaveCardBubbleControllerImpl::GetPaymentBubbleView() const {
  return bubble_view();
}

SavePaymentIconController::PaymentBubbleType
SaveCardBubbleControllerImpl::GetPaymentBubbleType() const {
  return PaymentBubbleType::kCreditCard;
}

PageActionIconType SaveCardBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveCard;
}

void SaveCardBubbleControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  set_bubble_view(
      browser->window()->GetAutofillBubbleHandler()->ShowSaveCreditCardBubble(
          web_contents(), this, is_reshow_));
  DCHECK(bubble_view());

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      autofill_metrics::LogSaveCardPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kShown, is_upload_save_,
          is_reshow_, options_, GetSecurityLevel(), GetSyncState());
      break;
    case BubbleType::MANAGE_CARDS:
      LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsShown,
                                 is_upload_save_);
      break;
    case BubbleType::FAILURE:
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

void SaveCardBubbleControllerImpl::FetchAccountInfo() {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return;
  auto* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(profile);
  if (!personal_data_manager)
    return;
  account_info_ = identity_manager->FindExtendedAccountInfo(
      personal_data_manager->GetAccountInfoForPaymentsServer());
}

void SaveCardBubbleControllerImpl::ShowBubble() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!bubble_view());
  Show();
}

void SaveCardBubbleControllerImpl::ShowIconOnly() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!bubble_view());

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdatePageActionIcon();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      autofill_metrics::LogSaveCardPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached,
          is_upload_save_, is_reshow_, options_, GetSecurityLevel(),
          GetSyncState());
      break;
    case BubbleType::FAILURE:
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::MANAGE_CARDS:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  if (observer_for_testing_)
    observer_for_testing_->OnIconShown();
}

void SaveCardBubbleControllerImpl::OpenUrl(const GURL& url) {
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

security_state::SecurityLevel SaveCardBubbleControllerImpl::GetSecurityLevel()
    const {
  return security_level_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveCardBubbleControllerImpl);

}  // namespace autofill
