// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      pref_service_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())) {
  security_level_ =
      SecurityStateTabHelper::FromWebContents(web_contents)->GetSecurityLevel();

  personal_data_manager_ =
      PersonalDataManagerFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() {
  if (save_card_bubble_view_)
    save_card_bubble_view_->Hide();
}

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
  if (save_card_bubble_view_)
    return;

  is_upload_save_ = false;
  is_reshow_ = false;
  options_ = options;
  legal_message_lines_.clear();

  card_ = card;
  local_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ = BubbleType::LOCAL_SAVE;

  if (options.show_prompt) {
    ShowBubble();
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
  } else {
    ShowIconOnly();
  }
}

void SaveCardBubbleControllerImpl::OfferUploadSave(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    AutofillClient::SaveCreditCardOptions options,
    AutofillClient::UploadSaveCardPromptCallback save_card_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  // Fetch the logged-in user's AccountInfo if it has not yet been done.
  if (options.should_request_name_from_user && account_info_.IsEmpty())
    FetchAccountInfo();

  is_upload_save_ = true;
  is_reshow_ = false;
  options_ = options;
  if (options.show_prompt) {
    // Can't move this into the other "if (show_bubble_)" below because an
    // invalid legal message would skip it.
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
  }

  card_ = card;
  upload_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ = BubbleType::UPLOAD_SAVE;
  legal_message_lines_ = legal_message_lines;

  if (options_.show_prompt)
    ShowBubble();
  else
    ShowIconOnly();
}

void SaveCardBubbleControllerImpl::MaybeShowBubbleForSignInPromo() {
  if (!ShouldShowSignInPromo())
    return;

  current_bubble_type_ = BubbleType::SIGN_IN_PROMO;

  // If DICe is disabled, then we need to know whether the user is signed in
  // to determine whether or not to show a sign-in vs sync promo.
  if (GetAccountInfo().IsEmpty())
    FetchAccountInfo();
  ShowBubble();
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
  UpdateSaveCardIcon();
}

void SaveCardBubbleControllerImpl::UpdateIconForSaveCardFailure() {
  current_bubble_type_ = BubbleType::FAILURE;
  ShowIconOnly();
}

void SaveCardBubbleControllerImpl::ShowBubbleForSaveCardFailureForTesting() {
  current_bubble_type_ = BubbleType::FAILURE;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::HideBubble() {
  if (save_card_bubble_view_) {
    save_card_bubble_view_->Hide();
    save_card_bubble_view_ = nullptr;
  }
}

void SaveCardBubbleControllerImpl::HideBubbleForSignInPromo() {
  if (current_bubble_type_ == BubbleType::SIGN_IN_PROMO)
    HideBubble();
}

void SaveCardBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  is_reshow_ = true;

  if (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
      current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
  }

  ShowBubble();
}

base::string16 SaveCardBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
    case BubbleType::UPLOAD_SAVE:
      return features::ShouldShowImprovedUserConsentForCreditCardSave()
                 ? l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V4)
                 : l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
    case BubbleType::SIGN_IN_PROMO:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      if (AccountConsistencyModeManager::IsDiceEnabledForProfile(
              GetProfile())) {
        return l10n_util::GetStringUTF16(IDS_AUTOFILL_SYNC_PROMO_MESSAGE);
      }
#endif
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::FAILURE:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_FAILURE_BUBBLE_TITLE);
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::INACTIVE:
      NOTREACHED();
      return base::string16();
  }
}

base::string16 SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  if (current_bubble_type_ == BubbleType::FAILURE)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_FAILURE_BUBBLE_EXPLANATION);

  if (current_bubble_type_ != BubbleType::UPLOAD_SAVE)
    return base::string16();

  if (options_.should_request_name_from_user) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_NAME);
  }

  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3);
}

base::string16 SaveCardBubbleControllerImpl::GetAcceptButtonText() const {
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
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      return base::string16();
  }
}

base::string16 SaveCardBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_LOCAL_SAVE);
    case BubbleType::UPLOAD_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_UPLOAD_SAVE);
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::MANAGE_CARDS:
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      return base::string16();
  }
}

const AccountInfo& SaveCardBubbleControllerImpl::GetAccountInfo() const {
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

SaveCardBubbleView* SaveCardBubbleControllerImpl::GetSaveCardBubbleView()
    const {
  return save_card_bubble_view_;
}

bool SaveCardBubbleControllerImpl::ShouldRequestNameFromUser() const {
  return options_.should_request_name_from_user;
}

bool SaveCardBubbleControllerImpl::ShouldRequestExpirationDateFromUser() const {
  return options_.should_request_expiration_date_from_user;
}

bool SaveCardBubbleControllerImpl::ShouldShowSignInPromo() const {
  if (is_upload_save_)
    return false;

  if (!GetProfile()->GetPrefs()->GetBoolean(::prefs::kSigninAllowed))
    return false;

  const syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());

  return !sync_service ||
         sync_service->HasDisableReason(
             syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN) ||
         sync_service->HasDisableReason(
             syncer::SyncService::DISABLE_REASON_USER_CHOICE);
}

void SaveCardBubbleControllerImpl::OnSyncPromoAccepted(
    const AccountInfo& account,
    signin_metrics::AccessPoint access_point,
    bool is_default_promo_account) {
  DCHECK(current_bubble_type_ == BubbleType::SIGN_IN_PROMO ||
         current_bubble_type_ == BubbleType::MANAGE_CARDS);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  signin_ui_util::EnableSyncFromPromo(browser, account, access_point,
                                      is_default_promo_account);
}

void SaveCardBubbleControllerImpl::OnSaveButton(
    const AutofillClient::UserProvidedCardDetails& user_provided_card_details) {
  save_card_bubble_view_ = nullptr;

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE: {
      DCHECK(!upload_save_card_prompt_callback_.is_null());

      base::string16 name_provided_by_user;
      if (!user_provided_card_details.cardholder_name.empty()) {
        // Log whether the name was changed by the user or simply accepted
        // without edits.
        AutofillMetrics::LogSaveCardCardholderNameWasEdited(
            user_provided_card_details.cardholder_name !=
            base::UTF8ToUTF16(account_info_.full_name));
        // Trim the cardholder name provided by the user and send it in the
        // callback so it can be included in the final request.
        DCHECK(ShouldRequestNameFromUser());
        base::TrimWhitespace(user_provided_card_details.cardholder_name,
                             base::TRIM_ALL, &name_provided_by_user);
      }
      std::move(upload_save_card_prompt_callback_)
          .Run(AutofillClient::ACCEPTED, user_provided_card_details);
      break;
    }
    case BubbleType::LOCAL_SAVE:
      DCHECK(!local_save_card_prompt_callback_.is_null());
      // Show an animated card saved confirmation message next time
      // UpdateSaveCardIcon() is called.
      should_show_card_saved_label_animation_ = true;
      std::move(local_save_card_prompt_callback_).Run(AutofillClient::ACCEPTED);
      break;
    case BubbleType::MANAGE_CARDS:
      AutofillMetrics::LogManageCardsPromptMetric(
          AutofillMetrics::MANAGE_CARDS_DONE, is_upload_save_);
      return;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::FAILURE:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  if (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
      current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());

    // If the experiment is not enabled, update user's previous decision here.
    // Otherwise since the logging will happen in OnBubbleClosed() which is
    // invoked after OnSaveButton(), the previous decision should be set there.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableFixedPaymentsBubbleLogging)) {
      pref_service_->SetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState,
          prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED);
    }
  }
}

void SaveCardBubbleControllerImpl::OnCancelButton() {
  if (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
      current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, is_upload_save_,
        is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());

    // If the experiment is not enabled, update user's previous decision here.
    // Otherwise since the logging will happen in OnBubbleClosed() which is
    // invoked after OnCancelButton(), the previous decision should be set
    // there.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableFixedPaymentsBubbleLogging)) {
      pref_service_->SetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState,
          prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED);
    }

    if (current_bubble_type_ == BubbleType::LOCAL_SAVE) {
      std::move(local_save_card_prompt_callback_).Run(AutofillClient::DECLINED);
    } else {  // BubbleType::UPLOAD_SAVE
      std::move(upload_save_card_prompt_callback_)
          .Run(AutofillClient::DECLINED, {});
    }
  }
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE,
      is_upload_save_, is_reshow_, options_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel(), GetSyncState());

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableFixedPaymentsBubbleLogging)) {
    AutofillMetrics::LogCreditCardUploadLegalMessageLinkClicked();
  }
}

void SaveCardBubbleControllerImpl::OnManageCardsClicked() {
  DCHECK(current_bubble_type_ == BubbleType::MANAGE_CARDS);

  AutofillMetrics::LogManageCardsPromptMetric(
      AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, is_upload_save_);

  ShowPaymentsSettingsPage();
}

void SaveCardBubbleControllerImpl::ShowPaymentsSettingsPage() {
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPaymentsSubPage);
}

void SaveCardBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  save_card_bubble_view_ = nullptr;

  // Log save card prompt result according to the closed reason.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableFixedPaymentsBubbleLogging) &&
      (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
       current_bubble_type_ == BubbleType::UPLOAD_SAVE)) {
    AutofillMetrics::SaveCardPromptResultMetric metric;
    switch (closed_reason) {
      case PaymentsBubbleClosedReason::kAccepted:
        metric = AutofillMetrics::SAVE_CARD_PROMPT_ACCEPTED;
        pref_service_->SetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState,
            prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED);
        break;
      case PaymentsBubbleClosedReason::kCancelled:
        metric = AutofillMetrics::SAVE_CARD_PROMPT_CANCELLED;
        pref_service_->SetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState,
            prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED);
        break;
      case PaymentsBubbleClosedReason::kClosed:
        metric = AutofillMetrics::SAVE_CARD_PROMPT_CLOSED;
        break;
      case PaymentsBubbleClosedReason::kNotInteracted:
        metric = AutofillMetrics::SAVE_CARD_PROMPT_NOT_INTERACTED;
        break;
      case PaymentsBubbleClosedReason::kLostFocus:
        metric = AutofillMetrics::SAVE_CARD_PROMPT_LOST_FOCUS;
        break;
      case PaymentsBubbleClosedReason::kUnknown:
        metric = AutofillMetrics::SAVE_CARD_PROMPT_RESULT_UNKNOWN;
        break;
    }
    AutofillMetrics::LogSaveCardPromptResultMetric(
        metric, is_upload_save_, is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());
  }

  // Handles |current_bubble_type_| change according to its current type and the
  // |closed_reason|.
  if (closed_reason == PaymentsBubbleClosedReason::kAccepted) {
    if (current_bubble_type_ == BubbleType::LOCAL_SAVE) {
      current_bubble_type_ = base::FeatureList::IsEnabled(
                                 features::kAutofillCreditCardUploadFeedback)
                                 ? BubbleType::INACTIVE
                                 : BubbleType::MANAGE_CARDS;
    } else if (current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
      if (base::FeatureList::IsEnabled(
              features::kAutofillCreditCardUploadFeedback)) {
        current_bubble_type_ = BubbleType::UPLOAD_IN_PROGRESS;

        // Log this metric here since for each bubble, the bubble state will
        // only be changed to UPLOAD_IN_PROGRESS once.
        // SavePaymentIconView::Update is not guaranteed to be called only once
        // so logging in any functions related to it is not reliable.
        AutofillMetrics::LogCreditCardUploadFeedbackMetric(
            AutofillMetrics::
                CREDIT_CARD_UPLOAD_FEEDBACK_LOADING_ANIMATION_SHOWN);
      } else {
        current_bubble_type_ = BubbleType::INACTIVE;
      }
    } else {
      DCHECK_EQ(current_bubble_type_, BubbleType::MANAGE_CARDS);
      current_bubble_type_ = BubbleType::INACTIVE;
    }
  } else if (closed_reason == PaymentsBubbleClosedReason::kCancelled) {
    current_bubble_type_ = BubbleType::INACTIVE;
  } else {
    // Needs to handle some special cases for other closed reasons.
    if (current_bubble_type_ == BubbleType::SIGN_IN_PROMO) {
      // If experiment is enabled, hide the icon.
      // Otherwise sign-in promo should only be shown once, so if it was
      // displayed presently, reopening the bubble will show the card management
      // bubble.
      current_bubble_type_ =
          base::FeatureList::IsEnabled(
              autofill::features::kAutofillCreditCardUploadFeedback)
              ? BubbleType::INACTIVE
              : BubbleType::MANAGE_CARDS;
    } else if (current_bubble_type_ == BubbleType::FAILURE) {
      // Unlike other bubbles, the save failure bubble should not be reshown. If
      // the save card failure bubble is closed, the credit card icon should be
      // dismissed as well.
      current_bubble_type_ = BubbleType::INACTIVE;
    }
  }

  UpdateSaveCardIcon();

  if (observer_for_testing_)
    observer_for_testing_->OnBubbleClosed();
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

base::string16 SaveCardBubbleControllerImpl::GetSavePaymentIconTooltipText()
    const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
    case BubbleType::UPLOAD_SAVE:
    // TODO(crbug.com/932818): With |kAutofillCreditCardUploadFeedback| being
    // enabled, sign in promo will not be shown from the credit card icon, and
    // there will not be manage cards bubble. These two will be cleaned up in
    // the future.
    case BubbleType::MANAGE_CARDS:
    case BubbleType::SIGN_IN_PROMO:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD);
    case BubbleType::UPLOAD_IN_PROGRESS:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD_PENDING);
    case BubbleType::FAILURE:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD_FAILURE);
    case BubbleType::INACTIVE:
      return base::string16();
  }
}

bool SaveCardBubbleControllerImpl::ShouldShowSavingCardAnimation() const {
  return current_bubble_type_ == BubbleType::UPLOAD_IN_PROGRESS;
}

bool SaveCardBubbleControllerImpl::ShouldShowCardSavedLabelAnimation() const {
  // If experiment is on, does not show the "Card Saved" animation but instead
  // hides the icon.
  return !base::FeatureList::IsEnabled(
             features::kAutofillCreditCardUploadFeedback) &&
         should_show_card_saved_label_animation_;
}

bool SaveCardBubbleControllerImpl::ShouldShowSaveFailureBadge() const {
  return current_bubble_type_ == BubbleType::FAILURE;
}

void SaveCardBubbleControllerImpl::OnAnimationEnded() {
  // Do not repeat the animation next time UpdateSaveCardIcon() is called,
  // unless explicitly set somewhere else.
  should_show_card_saved_label_animation_ = false;

  // We do not want to show the promo if the user clicked on the icon and the
  // manage cards bubble started to show.
  if (!save_card_bubble_view_)
    MaybeShowBubbleForSignInPromo();
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback) &&
      current_bubble_type_ == BubbleType::SIGN_IN_PROMO) {
    return false;
  }

  // If there is no bubble to show, then there should be no icon.
  return current_bubble_type_ != BubbleType::INACTIVE;
}

SaveCardBubbleView* SaveCardBubbleControllerImpl::GetSaveBubbleView() const {
  return GetSaveCardBubbleView();
}

void SaveCardBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableStickyPaymentsBubble)) {
    return;
  }

  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Nothing to do if there's no bubble available.
  if (current_bubble_type_ == BubbleType::INACTIVE)
    return;

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  const base::TimeDelta elapsed_time =
      AutofillClock::Now() - bubble_shown_timestamp_;
  if (elapsed_time < kCardBubbleSurviveNavigationTime)
    return;

  bool bubble_was_visible = save_card_bubble_view_;

  if (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
      current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        bubble_was_visible
            ? AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING
            : AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN,
        is_upload_save_, is_reshow_, options_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel(), GetSyncState());

    if (current_bubble_type_ == BubbleType::LOCAL_SAVE) {
      DCHECK(!local_save_card_prompt_callback_.is_null());
      std::move(local_save_card_prompt_callback_).Run(AutofillClient::IGNORED);
    } else {  // BubbleType::UPLOAD_SAVE
      DCHECK(!upload_save_card_prompt_callback_.is_null());
      std::move(upload_save_card_prompt_callback_)
          .Run(AutofillClient::IGNORED, {});
    }
  }

  // Otherwise, get rid of the bubble and icon.
  current_bubble_type_ = BubbleType::INACTIVE;

  if (bubble_was_visible) {
    save_card_bubble_view_->Hide();
  } else {
    UpdateSaveCardIcon();
  }
}

void SaveCardBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HideBubble();
}

void SaveCardBubbleControllerImpl::WebContentsDestroyed() {
  HideBubble();
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
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          personal_data_manager->GetAccountInfoForPaymentsServer());
  account_info_ = account_info.value_or(AccountInfo{});
}

void SaveCardBubbleControllerImpl::ShowBubble() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!save_card_bubble_view_);

  // Need to create location bar icon before bubble, otherwise bubble will be
  // unanchored.
  UpdateSaveCardIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillCreditCardUploadFeedback) &&
      current_bubble_type_ == BubbleType::SIGN_IN_PROMO) {
    // The sign in promo bubble will never be re-shown.
    DCHECK(!is_reshow_);
    save_card_bubble_view_ =
        browser->window()
            ->GetAutofillBubbleHandler()
            ->ShowSaveCardSignInPromoBubble(web_contents(), this);
  } else {
    save_card_bubble_view_ =
        browser->window()->GetAutofillBubbleHandler()->ShowSaveCreditCardBubble(
            web_contents(), this, is_reshow_);
  }
  DCHECK(save_card_bubble_view_);

  // Update icon after creating |save_card_bubble_view_| so that icon will show
  // its "toggled on" state.
  UpdateSaveCardIcon();

  bubble_shown_timestamp_ = AutofillClock::Now();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      AutofillMetrics::LogSaveCardPromptMetric(
          AutofillMetrics::SAVE_CARD_PROMPT_SHOWN_DEPRECATED, is_upload_save_,
          is_reshow_, options_,
          pref_service_->GetInteger(
              prefs::kAutofillAcceptSaveCreditCardPromptState),
          GetSecurityLevel(), GetSyncState());
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableFixedPaymentsBubbleLogging)) {
        AutofillMetrics::LogSaveCardPromptOfferMetric(
            AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, is_upload_save_,
            is_reshow_, options_,
            pref_service_->GetInteger(
                prefs::kAutofillAcceptSaveCreditCardPromptState),
            GetSecurityLevel(), GetSyncState());
      }
      break;
    case BubbleType::MANAGE_CARDS:
      AutofillMetrics::LogManageCardsPromptMetric(
          AutofillMetrics::MANAGE_CARDS_SHOWN, is_upload_save_);
      break;
    case BubbleType::SIGN_IN_PROMO:
      break;
    case BubbleType::FAILURE:
      AutofillMetrics::LogCreditCardUploadFeedbackMetric(
          AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_BUBBLE_SHOWN);
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

void SaveCardBubbleControllerImpl::ShowIconOnly() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_prompt_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!save_card_bubble_view_);

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdateSaveCardIcon();

  bubble_shown_timestamp_ = AutofillClock::Now();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      AutofillMetrics::LogSaveCardPromptMetric(
          AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, is_upload_save_,
          is_reshow_, options_,
          pref_service_->GetInteger(
              prefs::kAutofillAcceptSaveCreditCardPromptState),
          GetSecurityLevel(), GetSyncState());
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableFixedPaymentsBubbleLogging)) {
        AutofillMetrics::LogSaveCardPromptOfferMetric(
            AutofillMetrics::SAVE_CARD_PROMPT_NOT_SHOWN_MAX_STRIKES_REACHED,
            is_upload_save_, is_reshow_, options_,
            pref_service_->GetInteger(
                prefs::kAutofillAcceptSaveCreditCardPromptState),
            GetSecurityLevel(), GetSyncState());
      }
      break;
    case BubbleType::FAILURE:
      AutofillMetrics::LogCreditCardUploadFeedbackMetric(
          AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_ICON_SHOWN);
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::MANAGE_CARDS:
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }
}

void SaveCardBubbleControllerImpl::UpdateSaveCardIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser)
    browser->window()->UpdatePageActionIcon(PageActionIconType::kSaveCard);
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveCardBubbleControllerImpl)

}  // namespace autofill
