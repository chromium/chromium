// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
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
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/metrics/payments/manage_cards_prompt_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
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
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

namespace {

std::u16string GetWindowTitleForUploadSave() {
  switch (GetUpdatedDesktopUiTreatmentArm()) {
    case UpdatedDesktopUiTreatmentArm::kSecurityFocus:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_SECURITY);
    case UpdatedDesktopUiTreatmentArm::kConvenienceFocus:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_CONVENIENCE);
    case UpdatedDesktopUiTreatmentArm::kEducationFocus:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_EDUCATION);
    case UpdatedDesktopUiTreatmentArm::kDefault:
      return features::ShouldShowImprovedUserConsentForCreditCardSave()
                 ? l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V4)
                 : l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
  }
}

std::optional<std::u16string> GetUpdatedExplanatoryMessageForUploadSave() {
  switch (GetUpdatedDesktopUiTreatmentArm()) {
    case UpdatedDesktopUiTreatmentArm::kSecurityFocus:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_SECURITY);
    case UpdatedDesktopUiTreatmentArm::kConvenienceFocus:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_CONVENIENCE);
    case UpdatedDesktopUiTreatmentArm::kEducationFocus:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_EDUCATION);
    case UpdatedDesktopUiTreatmentArm::kDefault:
      return std::nullopt;
  }
}

}  // namespace

static bool g_ignore_window_activation_for_testing = false;

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<SaveCardBubbleControllerImpl>(
          *web_contents) {
  personal_data_manager_ = PersonalDataManagerFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  sync_service_ = SyncServiceFactory::GetForProfile(
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
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
        save_card_prompt_callback) {
  // If the confirmation view is still showing, close it before showing the new
  // offer.
  if (current_bubble_type_ == BubbleType::UPLOAD_COMPLETED) {
    HideBubble();
  }

  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;

  is_upload_save_ = false;
  is_reshow_ = false;
  is_triggered_by_user_gesture_ = false;
  options_ = options;
  card_ = card;
  local_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  legal_message_lines_.clear();
  current_bubble_type_ =
      options.card_save_type ==
              payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
          ? BubbleType::LOCAL_CVC_SAVE
          : BubbleType::LOCAL_SAVE;

  if (options.show_prompt)
    ShowBubble();
  else
    ShowIconOnly();
}

void SaveCardBubbleControllerImpl::OfferUploadSave(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
        save_card_prompt_callback) {
  // If the confirmation view is still showing, close it before showing the new
  // offer.
  if (current_bubble_type_ == BubbleType::UPLOAD_COMPLETED) {
    HideBubble();
  }

  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;

  is_upload_save_ = true;
  is_reshow_ = false;
  is_triggered_by_user_gesture_ = false;
  options_ = options;
  card_ = card;
  upload_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ =
      options.card_save_type ==
              payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
          ? BubbleType::UPLOAD_CVC_SAVE
          : BubbleType::UPLOAD_SAVE;

  // Reset legal_message_lines for CVC only upload as there is no legal message
  // for this case.
  // TODO(crbug.com/40931101): Refactor ConfirmSaveCreditCardToCloud to change
  // legal_message_lines_ to optional.
  if (current_bubble_type_ == BubbleType::UPLOAD_CVC_SAVE) {
    legal_message_lines_.clear();
  } else {
    legal_message_lines_ = legal_message_lines;
  }

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

void SaveCardBubbleControllerImpl::ReshowBubble(
    bool is_triggered_by_user_gesture) {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;

  is_reshow_ = true;
  is_triggered_by_user_gesture_ = is_triggered_by_user_gesture;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::ShowConfirmationBubbleView(
    bool card_saved,
    std::optional<
        payments::PaymentsAutofillClient::OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    // Hide the current bubble if still showing.
    HideBubble();

    is_reshow_ = false;
    is_triggered_by_user_gesture_ = false;
    current_bubble_type_ = BubbleType::UPLOAD_COMPLETED;
    confirmation_ui_params_ =
        card_saved ? SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                         CreateForSaveCardSuccess()
                   : SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                         CreateForSaveCardFailure();
    on_confirmation_closed_callback_ =
        std::move(on_confirmation_closed_callback);

    // Show upload confirmation bubble.
    ShowBubble();

    // Auto close confirmation bubble when card saved is successful.
    if (card_saved) {
      auto_close_confirmation_timer_.Start(
          FROM_HERE, kAutoCloseConfirmationBubbleWaitSec,
          base::BindOnce(&SaveCardBubbleControllerImpl::HideSaveCardBubble,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    autofill_metrics::LogCreditCardUploadConfirmationViewShownMetric(
        /*is_shown=*/false, card_saved);
  }
}

std::u16string SaveCardBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
    case BubbleType::LOCAL_CVC_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CVC_PROMPT_TITLE_LOCAL);
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::UPLOAD_IN_PROGRESS:
      return GetWindowTitleForUploadSave();
    case BubbleType::UPLOAD_CVC_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CVC_PROMPT_TITLE_TO_CLOUD);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(
          options_.card_save_type ==
                  payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
              ? IDS_AUTOFILL_CVC_SAVED
              : IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::UPLOAD_COMPLETED:
    case BubbleType::INACTIVE:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  if (current_bubble_type_ == BubbleType::LOCAL_SAVE &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFilling)) {
    CHECK_NE(options_.card_save_type,
             payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly);
    return l10n_util::GetStringUTF16(
        options_.card_save_type ==
                payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly
            ? IDS_AUTOFILL_SAVE_CARD_ONLY_PROMPT_EXPLANATION_LOCAL
            : IDS_AUTOFILL_SAVE_CARD_WITH_CVC_PROMPT_EXPLANATION_LOCAL);
  }

  if (current_bubble_type_ == BubbleType::LOCAL_CVC_SAVE) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CVC_PROMPT_EXPLANATION_LOCAL);
  }

  if (current_bubble_type_ == BubbleType::UPLOAD_CVC_SAVE) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CVC_PROMPT_EXPLANATION_UPLOAD);
  }

  if (current_bubble_type_ != BubbleType::UPLOAD_SAVE &&
      current_bubble_type_ != BubbleType::UPLOAD_IN_PROGRESS) {
    return std::u16string();
  }

  if (std::optional<std::u16string> updated_ui_explanatory_message =
          GetUpdatedExplanatoryMessageForUploadSave()) {
    return updated_ui_explanatory_message.value();
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFilling) &&
      options_.card_save_type ==
          payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CARD_WITH_CVC_PROMPT_EXPLANATION_UPLOAD);
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
    case BubbleType::LOCAL_CVC_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_LOCAL_SAVE_ACCEPT);
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::UPLOAD_CVC_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_UPLOAD_SAVE_ACCEPT);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DONE);
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::UPLOAD_COMPLETED:
    case BubbleType::INACTIVE:
      return std::u16string();
  }
}

std::u16string SaveCardBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
    case BubbleType::LOCAL_CVC_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_LOCAL_SAVE);
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::UPLOAD_CVC_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_UPLOAD_SAVE);
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::UPLOAD_COMPLETED:
    case BubbleType::MANAGE_CARDS:
    case BubbleType::INACTIVE:
      return std::u16string();
  }
}

AccountInfo SaveCardBubbleControllerImpl::GetAccountInfo() {
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

Profile* SaveCardBubbleControllerImpl::GetProfile() const {
  if (!web_contents())
    return nullptr;
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

const CreditCard& SaveCardBubbleControllerImpl::GetCard() const {
  return card_;
}

base::OnceCallback<void(PaymentsBubbleClosedReason)>
SaveCardBubbleControllerImpl::GetOnBubbleClosedCallback() {
  return base::BindOnce(&SaveCardBubbleControllerImpl::OnBubbleClosed,
                        weak_ptr_factory_.GetWeakPtr());
}

const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
SaveCardBubbleControllerImpl::GetConfirmationUiParams() const {
  CHECK(confirmation_ui_params_.has_value());
  return confirmation_ui_params_.value();
}

bool SaveCardBubbleControllerImpl::ShouldRequestNameFromUser() const {
  return options_.should_request_name_from_user;
}

bool SaveCardBubbleControllerImpl::ShouldRequestExpirationDateFromUser() const {
  return options_.should_request_expiration_date_from_user;
}

ui::ImageModel SaveCardBubbleControllerImpl::GetCreditCardImage() const {
  gfx::Image* card_art_image =
      personal_data_manager_->payments_data_manager()
          .GetCreditCardArtImageForUrl(card_.card_art_url());
  return ui::ImageModel::FromImage(
      card_art_image ? *card_art_image
                     : ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                           CreditCard::IconResourceId(card_.network())));
}

void SaveCardBubbleControllerImpl::OnSaveButton(
    const payments::PaymentsAutofillClient::UserProvidedCardDetails&
        user_provided_card_details) {
  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE: {
      CHECK(!upload_save_card_prompt_callback_.is_null());
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
        CHECK(ShouldRequestNameFromUser());
        base::TrimWhitespace(user_provided_card_details.cardholder_name,
                             base::TRIM_ALL, &name_provided_by_user);
      }

      // Log metrics now for the upload save card. The upload case is special
      // because we don't immediately close the bubble (at which time the other
      // metrics are logged) after OnSaveButton() and logging now aligns the
      // timing of the log with the other cases.
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
        autofill_metrics::LogSaveCardPromptResultMetric(
            autofill_metrics::SaveCardPromptResult::kAccepted, is_upload_save_,
            is_reshow_, options_,
            personal_data_manager_->payments_data_manager()
                .GetPaymentsSigninStateForMetrics(),
            /*has_saved_cards=*/
            !personal_data_manager_->payments_data_manager()
                 .GetCreditCards()
                 .empty());
        autofill_metrics::LogCreditCardUploadLoadingViewShownMetric(
            /*is_shown=*/true);
        current_bubble_type_ = BubbleType::UPLOAD_IN_PROGRESS;
      } else {
        autofill_metrics::LogCreditCardUploadLoadingViewShownMetric(
            /*is_shown=*/false);
      }

      std::move(upload_save_card_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveCardOfferUserDecision::
                   kAccepted,
               user_provided_card_details);
      break;
    }
    case BubbleType::UPLOAD_CVC_SAVE: {
      CHECK(!upload_save_card_prompt_callback_.is_null());
      if (auto* sentiment_service =
              TrustSafetySentimentServiceFactory::GetForProfile(GetProfile())) {
        sentiment_service->SavedCard();
      }
      std::move(upload_save_card_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveCardOfferUserDecision::
                   kAccepted,
               /*user_provided_card_details=*/{});
      break;
    }
    case BubbleType::LOCAL_SAVE:
    case BubbleType::LOCAL_CVC_SAVE:
      CHECK(!local_save_card_prompt_callback_.is_null());
      if (auto* sentiment_service =
              TrustSafetySentimentServiceFactory::GetForProfile(GetProfile())) {
        sentiment_service->SavedCard();
      }
      // Show an animated card saved confirmation message next time
      // UpdatePageActionIcon() is called.
      should_show_card_saved_label_animation_ = true;
      std::move(local_save_card_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveCardOfferUserDecision::
                   kAccepted);
      break;
    case BubbleType::MANAGE_CARDS:
      CHECK(!is_upload_save_);
      LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsDone);
      return;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::UPLOAD_COMPLETED:
    case BubbleType::INACTIVE:
      NOTREACHED_IN_MIGRATION();
  }
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  autofill_metrics::LogCreditCardUploadLegalMessageLinkClicked();
}

void SaveCardBubbleControllerImpl::OnManageCardsClicked() {
  CHECK(current_bubble_type_ == BubbleType::MANAGE_CARDS);
  CHECK(!is_upload_save_);

  LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsManageCards);

  ShowPaymentsSettingsPage();
}

void SaveCardBubbleControllerImpl::ShowPaymentsSettingsPage() {
  chrome::ShowSettingsSubPage(chrome::FindBrowserWithTab(web_contents()),
                              chrome::kPaymentsSubPage);
}

void SaveCardBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);

  // If the dialog should be re-shown, do not change the bubble type or log
  // metrics.
  // TODO(crbug.com/316391673): Determine if we should track metrics on the
  // usage of this member.
  if (was_url_opened_) {
    return;
  }

  auto get_metric = [](PaymentsBubbleClosedReason reason) {
    switch (reason) {
      case PaymentsBubbleClosedReason::kAccepted:
        return autofill_metrics::SaveCardPromptResult::kAccepted;
      case PaymentsBubbleClosedReason::kCancelled:
        return autofill_metrics::SaveCardPromptResult::kCancelled;
      case PaymentsBubbleClosedReason::kClosed:
        return autofill_metrics::SaveCardPromptResult::kClosed;
      case PaymentsBubbleClosedReason::kNotInteracted:
        return autofill_metrics::SaveCardPromptResult::kNotInteracted;
      case PaymentsBubbleClosedReason::kLostFocus:
        return autofill_metrics::SaveCardPromptResult::kLostFocus;
      case PaymentsBubbleClosedReason::kUnknown:
        return autofill_metrics::SaveCardPromptResult::kUnknown;
    }
  };

  // Log save card prompt result according to the closed reason.
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_CVC_SAVE:
    case BubbleType::UPLOAD_CVC_SAVE:
      autofill_metrics::LogSaveCvcPromptResultMetric(
          get_metric(closed_reason), is_upload_save_, is_reshow_);
      break;
    case BubbleType::LOCAL_SAVE:
    case BubbleType::UPLOAD_SAVE:
      autofill_metrics::LogSaveCardPromptResultMetric(
          get_metric(closed_reason), is_upload_save_, is_reshow_, options_,
          personal_data_manager_->payments_data_manager()
              .GetPaymentsSigninStateForMetrics(),
          /*has_saved_cards=*/
          !personal_data_manager_->payments_data_manager()
               .GetCreditCards()
               .empty());
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
      autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
          get_metric(closed_reason));
      break;
    case BubbleType::UPLOAD_COMPLETED:
      autofill_metrics::LogCreditCardUploadConfirmationViewResultMetric(
          get_metric(closed_reason), confirmation_ui_params_->is_success);
      break;
    case BubbleType::INACTIVE:
    case BubbleType::MANAGE_CARDS:
      break;
  }

  // If the bubble is closed with the current_bubble_type_ as
  // UPLOAD_COMPLETED, transition the current_bubble_type_ to INACTIVE, reset
  // the confirmation_ui_model and run `on_confirmation_closed_callback_`.
  if (current_bubble_type_ == BubbleType::UPLOAD_COMPLETED) {
    current_bubble_type_ = BubbleType::INACTIVE;
    confirmation_ui_params_.reset();

    UpdatePageActionIcon();

    if (on_confirmation_closed_callback_) {
      (*std::exchange(on_confirmation_closed_callback_, std::nullopt)).Run();
    }
    auto_close_confirmation_timer_.Stop();
    return;
  }

  // Handles |current_bubble_type_| change according to its current type and the
  // |closed_reason|.
  using SaveCardOfferUserDecision =
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision;
  std::optional<SaveCardOfferUserDecision> user_decision;
  switch (closed_reason) {
    case PaymentsBubbleClosedReason::kAccepted:
      user_decision = SaveCardOfferUserDecision::kAccepted;
      switch (current_bubble_type_) {
        case BubbleType::LOCAL_SAVE:
        case BubbleType::LOCAL_CVC_SAVE:
          current_bubble_type_ = BubbleType::MANAGE_CARDS;
          break;
        case BubbleType::UPLOAD_SAVE:
        case BubbleType::UPLOAD_CVC_SAVE:
        case BubbleType::MANAGE_CARDS:
          current_bubble_type_ = BubbleType::INACTIVE;
          break;
        case BubbleType::INACTIVE:
        case BubbleType::UPLOAD_IN_PROGRESS:
        case BubbleType::UPLOAD_COMPLETED:
          NOTREACHED_IN_MIGRATION();
      }
      break;
    case PaymentsBubbleClosedReason::kCancelled:
      user_decision = SaveCardOfferUserDecision::kDeclined;
      break;
    case PaymentsBubbleClosedReason::kClosed:
      user_decision = SaveCardOfferUserDecision::kIgnored;
      break;
    case PaymentsBubbleClosedReason::kUnknown:
    case PaymentsBubbleClosedReason::kNotInteracted:
    case PaymentsBubbleClosedReason::kLostFocus:
      break;
  }

  if (user_decision && *user_decision != SaveCardOfferUserDecision::kAccepted) {
    switch (current_bubble_type_) {
      case BubbleType::LOCAL_SAVE:
      case BubbleType::LOCAL_CVC_SAVE:
        std::move(local_save_card_prompt_callback_).Run(*user_decision);
        break;
      case BubbleType::UPLOAD_SAVE:
      case BubbleType::UPLOAD_CVC_SAVE:
        std::move(upload_save_card_prompt_callback_)
            .Run(*user_decision, /*user_provided_card_details=*/{});
        break;
      default:
        break;
    }
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

bool SaveCardBubbleControllerImpl::
    IsPaymentsSyncTransportEnabledWithoutSyncFeature() const {
  // TODO(crbug.com/40067296): Migrate away from IsSyncFeatureEnabled() when the
  // API returns false on desktop.
  return personal_data_manager_->payments_data_manager()
             .IsPaymentsDownloadActive() &&
         !sync_service_->IsSyncFeatureEnabled();
}

void SaveCardBubbleControllerImpl::HideSaveCardBubble() {
  HideBubble();
}

std::u16string SaveCardBubbleControllerImpl::GetSavePaymentIconTooltipText()
    const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
    case BubbleType::UPLOAD_SAVE:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD);
    case BubbleType::LOCAL_CVC_SAVE:
    case BubbleType::UPLOAD_CVC_SAVE:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CVC);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(
          options_.card_save_type ==
                  payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
              ? IDS_TOOLTIP_SAVE_CVC
              : IDS_TOOLTIP_SAVE_CREDIT_CARD);
    case BubbleType::UPLOAD_IN_PROGRESS:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD_PENDING);
    case BubbleType::UPLOAD_COMPLETED:
    case BubbleType::INACTIVE:
      return std::u16string();
  }
}

bool SaveCardBubbleControllerImpl::ShouldShowSavingPaymentAnimation() const {
  return !base::FeatureList::IsEnabled(
             features::kAutofillEnableSaveCardLoadingAndConfirmation) &&
         current_bubble_type_ == BubbleType::UPLOAD_IN_PROGRESS;
}

bool SaveCardBubbleControllerImpl::ShouldShowPaymentSavedLabelAnimation()
    const {
  return should_show_card_saved_label_animation_;
}

void SaveCardBubbleControllerImpl::OnAnimationEnded() {
  // Do not repeat the animation next time UpdatePageActionIcon() is called,
  // unless explicitly set somewhere else.
  should_show_card_saved_label_animation_ = false;
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  if (current_bubble_type_ == BubbleType::INACTIVE) {
    CHECK(!bubble_view());
    // If there is no bubble to show, then there should be no icon.
    return false;
  }
  return true;
}

AutofillBubbleBase* SaveCardBubbleControllerImpl::GetPaymentBubbleView() const {
  return bubble_view();
}

int SaveCardBubbleControllerImpl::GetSaveSuccessAnimationStringId() const {
  return options_.card_save_type ==
                 payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
             ? IDS_AUTOFILL_CVC_SAVED
             : IDS_AUTOFILL_CARD_SAVED;
}

// static
base::AutoReset<bool>
SaveCardBubbleControllerImpl::IgnoreWindowActivationForTesting() {
  return base::AutoReset<bool>(&g_ignore_window_activation_for_testing, true);
}

void SaveCardBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    if (visibility == content::Visibility::VISIBLE &&
        (was_url_opened_ ||
         current_bubble_type_ == BubbleType::UPLOAD_COMPLETED)) {
      ReshowBubble(/*is_user_gesture=*/false);
    } else if (visibility == content::Visibility::HIDDEN) {
      HideBubble();
    }
    return;
  }

  AutofillBubbleControllerBase::OnVisibilityChanged(visibility);
}

PageActionIconType SaveCardBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveCard;
}

void SaveCardBubbleControllerImpl::DoShowBubble() {
  if (!IsWebContentsActive()) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (current_bubble_type_ == BubbleType::UPLOAD_COMPLETED) {
    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowSaveCardConfirmationBubble(web_contents(), this));
  } else {
    set_bubble_view(
        browser->window()->GetAutofillBubbleHandler()->ShowSaveCreditCardBubble(
            web_contents(), this, is_triggered_by_user_gesture_));
  }
  CHECK(bubble_view());

  // Do not log metrics for re-shows triggered by link clicks.
  // TODO(issuetracker.google.com/316391673): Determine whether we should log
  // metrics when using `was_url_opened_`.
  if (was_url_opened_) {
    was_url_opened_ = false;
    return;
  }

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      autofill_metrics::LogSaveCardPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kShown, is_upload_save_,
          is_reshow_, options_,
          personal_data_manager_->payments_data_manager()
              .GetPaymentsSigninStateForMetrics());
      break;
    case BubbleType::UPLOAD_CVC_SAVE:
    case BubbleType::LOCAL_CVC_SAVE:
      autofill_metrics::LogSaveCvcPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kShown, is_upload_save_,
          is_reshow_);
      break;
    case BubbleType::MANAGE_CARDS:
      CHECK(!is_upload_save_);
      LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsShown);
      break;
    case BubbleType::UPLOAD_COMPLETED:
      autofill_metrics::LogCreditCardUploadConfirmationViewShownMetric(
          /*is_shown=*/true, confirmation_ui_params_->is_success);
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
      break;
    case BubbleType::INACTIVE:
      NOTREACHED_IN_MIGRATION();
  }
}

void SaveCardBubbleControllerImpl::ShowBubble() {
  CHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE or
  // UPLOAD_CVC_SAVE state.
  CHECK(!upload_save_card_prompt_callback_.is_null() ||
        (current_bubble_type_ != BubbleType::UPLOAD_SAVE &&
         current_bubble_type_ != BubbleType::UPLOAD_CVC_SAVE));
  // Local save callback should not be null for LOCAL_SAVE or LOCAL_CVC_SAVE
  // state.
  CHECK(!local_save_card_prompt_callback_.is_null() ||
        (current_bubble_type_ != BubbleType::LOCAL_SAVE &&
         current_bubble_type_ != BubbleType::LOCAL_CVC_SAVE));
  CHECK(!bubble_view());
  Show();
}

void SaveCardBubbleControllerImpl::ShowIconOnly() {
  CHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE or
  // UPLOAD_CVC_SAVE state.
  CHECK(!upload_save_card_prompt_callback_.is_null() ||
        (current_bubble_type_ != BubbleType::UPLOAD_SAVE &&
         current_bubble_type_ != BubbleType::UPLOAD_CVC_SAVE));
  // Local save callback should not be null for LOCAL_SAVE or LOCAL_CVC_SAVE
  // state.
  CHECK(!local_save_card_prompt_callback_.is_null() ||
        current_bubble_type_ != BubbleType::LOCAL_SAVE &&
            current_bubble_type_ != BubbleType::LOCAL_CVC_SAVE);
  CHECK(!bubble_view());

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdatePageActionIcon();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      autofill_metrics::LogSaveCardPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached,
          is_upload_save_, is_reshow_, options_,
          personal_data_manager_->payments_data_manager()
              .GetPaymentsSigninStateForMetrics());
      break;
    case BubbleType::UPLOAD_CVC_SAVE:
    case BubbleType::LOCAL_CVC_SAVE:
      autofill_metrics::LogSaveCvcPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached,
          is_upload_save_, is_reshow_);
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::UPLOAD_COMPLETED:
    case BubbleType::MANAGE_CARDS:
    case BubbleType::INACTIVE:
      NOTREACHED_IN_MIGRATION();
  }
}

void SaveCardBubbleControllerImpl::OpenUrl(const GURL& url) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    was_url_opened_ = true;
  }

  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

bool SaveCardBubbleControllerImpl::IsWebContentsActive() {
  if (g_ignore_window_activation_for_testing) {
    return true;
  }

  Browser* active_browser = chrome::FindBrowserWithActiveWindow();
  return active_browser &&
         active_browser->tab_strip_model()->GetActiveWebContents() ==
             web_contents();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveCardBubbleControllerImpl);

}  // namespace autofill
