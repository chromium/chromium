// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
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
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics_desktop.h"
#include "components/autofill/core/browser/metrics/payments/manage_cards_prompt_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

namespace {

// SaveCardPromptMetricType will be either LegacySaveCardPromptResult or
// SaveCardPromptResultDesktop.
template <typename SaveCardPromptMetricType>
SaveCardPromptMetricType GetMetric(PaymentsUiClosedReason reason) {
  switch (reason) {
    case PaymentsUiClosedReason::kAccepted:
      return SaveCardPromptMetricType::kAccepted;
    case PaymentsUiClosedReason::kCancelled:
      return SaveCardPromptMetricType::kCancelled;
    case PaymentsUiClosedReason::kClosed:
      return SaveCardPromptMetricType::kClosed;
    case PaymentsUiClosedReason::kNotInteracted:
      return SaveCardPromptMetricType::kNotInteracted;
    case PaymentsUiClosedReason::kLostFocus:
      return SaveCardPromptMetricType::kLostFocus;
    case PaymentsUiClosedReason::kUnknown:
      return SaveCardPromptMetricType::kUnknown;
  }
}

}  // namespace

static bool g_ignore_window_activation_for_testing = false;

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<SaveCardBubbleControllerImpl>(*web_contents),
      payments_data_manager_(PersonalDataManagerFactory::GetForBrowserContext(
                                 web_contents->GetBrowserContext())
                                 ->payments_data_manager()),
      sync_service_(SyncServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() = default;

// static
SaveCardBubbleController* SaveCardBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents);
  return SaveCardBubbleControllerImpl::FromWebContents(web_contents);
}

void SaveCardBubbleControllerImpl::OfferLocalSave(
    const CreditCard& card,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
        save_card_prompt_callback) {
  // If the confirmation view is still showing, close it before showing the new
  // offer.
  if (current_bubble_type_ == PaymentsBubbleType::kUploadComplete) {
    HideBubble(/*initiated_by_bubble_manager=*/false);
  }

  // Don't show the bubble if it's already visible.
  if (bubble_view() || !MaySetUpBubble()) {
    return;
  }

  SetupLocalSave(card, options, std::move(save_card_prompt_callback));

  if (options.show_prompt) {
    CheckPreconditionsBeforeShowing();
    QueueOrShowBubble();
  } else {
    ShowIconOnly();
  }
}

void SaveCardBubbleControllerImpl::SetupLocalSave(
    CreditCard card,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
        save_card_prompt_callback) {
  was_bubble_shown_ = false;
  is_upload_save_ = false;
  is_reshow_ = false;
  is_triggered_by_user_gesture_ = false;
  options_ = options;
  card_ = std::move(card);
  local_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  legal_message_lines_.clear();
  current_bubble_type_ =
      options.card_save_type ==
              payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
          ? PaymentsBubbleType::kLocalCvcSave
          : PaymentsBubbleType::kLocalSave;
}

void SaveCardBubbleControllerImpl::OfferUploadSave(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
        save_card_prompt_callback) {
  // If the confirmation view is still showing, close it before showing the new
  // offer.
  if (current_bubble_type_ == PaymentsBubbleType::kUploadComplete) {
    HideBubble(/*initiated_by_bubble_manager=*/false);
  }

  // Don't show the bubble if it's already visible.
  if (bubble_view() || !MaySetUpBubble()) {
    return;
  }

  SetupUploadSave(card, legal_message_lines, options,
                  std::move(save_card_prompt_callback));

  if (options_.show_prompt) {
    CheckPreconditionsBeforeShowing();
    QueueOrShowBubble();
  } else {
    ShowIconOnly();
  }
}

void SaveCardBubbleControllerImpl::SetupUploadSave(
    CreditCard card,
    LegalMessageLines legal_message_lines,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
        save_card_prompt_callback) {
  was_bubble_shown_ = false;
  is_upload_save_ = true;
  is_reshow_ = false;
  is_triggered_by_user_gesture_ = false;
  options_ = options;
  card_ = std::move(card);
  upload_save_card_prompt_callback_ = std::move(save_card_prompt_callback);
  current_bubble_type_ =
      options.card_save_type ==
              payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
          ? PaymentsBubbleType::kUploadCvcSave
          : PaymentsBubbleType::kUploadSave;

  // Reset legal_message_lines for CVC only upload as there is no legal message
  // for this case.
  // TODO(crbug.com/40931101): Refactor ShowSaveCreditCardToCloud to change
  // legal_message_lines_ to optional.
  if (current_bubble_type_ == PaymentsBubbleType::kUploadCvcSave) {
    legal_message_lines_.clear();
  } else {
    legal_message_lines_ = std::move(legal_message_lines);
  }
}

// Exists for testing purposes only.
void SaveCardBubbleControllerImpl::ShowBubbleForManageCardsForTesting(
    const CreditCard& card) {
  card_ = card;
  current_bubble_type_ = PaymentsBubbleType::kManageCards;
  CheckPreconditionsBeforeShowing();
  QueueOrShowBubble();
}

void SaveCardBubbleControllerImpl::ReshowBubble(
    bool is_triggered_by_user_gesture) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }

  is_reshow_ = true;
  is_triggered_by_user_gesture_ = is_triggered_by_user_gesture;
  CheckPreconditionsBeforeShowing();
  QueueOrShowBubble(/*force_show=*/true);
}

void SaveCardBubbleControllerImpl::ShowConfirmationBubbleView(
    bool card_saved,
    std::optional<
        payments::PaymentsAutofillClient::OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  DoNotShowNextQueuedBubbleGuard guard = DoNotShowNextQueuedBubble();

  // Hide the current bubble if still showing.
  HideBubble(/*initiated_by_bubble_manager=*/false);

  is_reshow_ = false;
  is_triggered_by_user_gesture_ = false;
  current_bubble_type_ = PaymentsBubbleType::kUploadComplete;
  confirmation_ui_params_ =
      card_saved ? SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                       CreateForSaveCardSuccess()
                 : SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
                       CreateForSaveCardFailure();
  on_confirmation_closed_callback_ = std::move(on_confirmation_closed_callback);

  // Show upload confirmation bubble.
  CheckPreconditionsBeforeShowing();
  QueueOrShowBubble();

  // Auto close confirmation bubble when card saved is successful.
  if (card_saved) {
    auto_close_confirmation_timer_.Start(
        FROM_HERE, kAutoCloseConfirmationBubbleWaitSec,
        base::BindOnce(&SaveCardBubbleControllerImpl::HideSaveCardBubble,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

base::OnceClosure SaveCardBubbleControllerImpl::
    GetShowConfirmationForCardSuccessfullySavedCallback() {
  return base::BindOnce(
      &SaveCardBubbleControllerImpl::ShowConfirmationBubbleView,
      weak_ptr_factory_.GetWeakPtr(), true, std::nullopt);
}

base::OnceClosure
SaveCardBubbleControllerImpl::GetEndSaveCardPromptFlowCallback() {
  return base::BindOnce(&SaveCardBubbleControllerImpl::EndSaveCardPromptFlow,
                        weak_ptr_factory_.GetWeakPtr());
}

std::u16string SaveCardBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case PaymentsBubbleType::kLocalSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
    case PaymentsBubbleType::kLocalCvcSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CVC_PROMPT_TITLE_LOCAL);
    case PaymentsBubbleType::kUploadSave:
    case PaymentsBubbleType::kUploadInProgress:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_SECURITY);
    case PaymentsBubbleType::kUploadCvcSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CVC_PROMPT_TITLE_TO_CLOUD);
    case PaymentsBubbleType::kManageCards:
      return l10n_util::GetStringUTF16(
          options_.card_save_type ==
                  payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
              ? IDS_AUTOFILL_CVC_SAVED
              : IDS_AUTOFILL_CARD_SAVED);
    case PaymentsBubbleType::kUploadComplete:
    case PaymentsBubbleType::kInactive:
      NOTREACHED();
  }
}

std::u16string SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  if (current_bubble_type_ == PaymentsBubbleType::kLocalSave &&
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

  if (current_bubble_type_ == PaymentsBubbleType::kLocalCvcSave) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CVC_PROMPT_EXPLANATION_LOCAL);
  }

  if (current_bubble_type_ == PaymentsBubbleType::kUploadCvcSave) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CVC_PROMPT_EXPLANATION_UPLOAD);
  }

  if (current_bubble_type_ != PaymentsBubbleType::kUploadSave &&
      current_bubble_type_ != PaymentsBubbleType::kUploadInProgress) {
    return std::u16string();
  }

  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_SECURITY);
}

std::u16string SaveCardBubbleControllerImpl::GetAcceptButtonText() const {
  switch (current_bubble_type_) {
    case PaymentsBubbleType::kLocalSave:
    case PaymentsBubbleType::kLocalCvcSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_LOCAL_SAVE_ACCEPT);
    case PaymentsBubbleType::kUploadSave:
    case PaymentsBubbleType::kUploadCvcSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_BUBBLE_UPLOAD_SAVE_ACCEPT);
    case PaymentsBubbleType::kManageCards:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DONE);
    case PaymentsBubbleType::kUploadInProgress:
    case PaymentsBubbleType::kUploadComplete:
    case PaymentsBubbleType::kInactive:
      return std::u16string();
  }
}

std::u16string SaveCardBubbleControllerImpl::GetDeclineButtonText() const {
  switch (current_bubble_type_) {
    case PaymentsBubbleType::kLocalSave:
    case PaymentsBubbleType::kLocalCvcSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_LOCAL_SAVE);
    case PaymentsBubbleType::kUploadSave:
    case PaymentsBubbleType::kUploadCvcSave:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_NO_THANKS_DESKTOP_UPLOAD_SAVE);
    case PaymentsBubbleType::kUploadInProgress:
    case PaymentsBubbleType::kUploadComplete:
    case PaymentsBubbleType::kManageCards:
    case PaymentsBubbleType::kInactive:
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

  return identity_manager->FindExtendedAccountInfo(
      payments_data_manager_->GetAccountInfoForPaymentsServer());
}

Profile* SaveCardBubbleControllerImpl::GetProfile() const {
  if (!web_contents()) {
    return nullptr;
  }
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

const CreditCard& SaveCardBubbleControllerImpl::GetCard() const {
  return card_;
}

base::OnceCallback<void(PaymentsUiClosedReason)>
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
  const gfx::Image* const card_art_image =
      payments_data_manager_->GetCachedCardArtImageForUrl(card_.card_art_url());
  return ui::ImageModel::FromImage(
      card_art_image ? *card_art_image
                     : ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                           CreditCard::IconResourceId(card_.network())));
}

void SaveCardBubbleControllerImpl::OnSaveButton(
    const payments::PaymentsAutofillClient::UserProvidedCardDetails&
        user_provided_card_details) {
  switch (current_bubble_type_) {
    case PaymentsBubbleType::kUploadSave: {
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
      autofill_metrics::LogSaveCreditCardPromptResultMetricDesktop(
          autofill_metrics::SaveCardPromptResultDesktop::kAccepted,
          is_upload_save_, options_,
          /*has_saved_cards=*/
          !payments_data_manager_->GetCreditCards().empty());
      autofill_metrics::LogSaveCardPromptResultMetric(
          autofill_metrics::LegacySaveCardPromptResult::kAccepted,
          is_upload_save_, is_reshow_, options_,
          payments_data_manager_->GetPaymentsSigninStateForMetrics(),
          /*has_saved_cards=*/
          !payments_data_manager_->GetCreditCards().empty());
      autofill_metrics::LogCreditCardUploadLoadingViewShownMetric(
          /*is_shown=*/true);

      current_bubble_type_ = PaymentsBubbleType::kUploadInProgress;

      std::move(upload_save_card_prompt_callback_)
          .Run(payments::PaymentsAutofillClient::SaveCardOfferUserDecision::
                   kAccepted,
               user_provided_card_details);
      break;
    }
    case PaymentsBubbleType::kUploadCvcSave: {
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
    case PaymentsBubbleType::kLocalSave:
    case PaymentsBubbleType::kLocalCvcSave:
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
    case PaymentsBubbleType::kManageCards:
      CHECK(!is_upload_save_);
      LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsDone);
      return;
    case PaymentsBubbleType::kUploadInProgress:
    case PaymentsBubbleType::kUploadComplete:
    case PaymentsBubbleType::kInactive:
      NOTREACHED();
  }
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  autofill_metrics::LogCreditCardUploadLegalMessageLinkClicked();
}

void SaveCardBubbleControllerImpl::OnManageCardsClicked() {
  CHECK(current_bubble_type_ == PaymentsBubbleType::kManageCards);
  CHECK(!is_upload_save_);

  LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsManageCards);

  ShowPaymentsSettingsPage();
}

void SaveCardBubbleControllerImpl::ShowPaymentsSettingsPage() {
  chrome::ShowSettingsSubPage(chrome::FindBrowserWithTab(web_contents()),
                              chrome::kPaymentsSubPage);
}

void SaveCardBubbleControllerImpl::OnBubbleDiscarded() {
  LogBubbleCloseMetrics(was_bubble_shown_
                            ? PaymentsUiClosedReason::kNotInteracted
                            : PaymentsUiClosedReason::kUnknown);
}

void SaveCardBubbleControllerImpl::LogBubbleCloseMetrics(
    PaymentsUiClosedReason closed_reason) {
  autofill_metrics::LegacySaveCardPromptResult legacy_metric =
      GetMetric<autofill_metrics::LegacySaveCardPromptResult>(closed_reason);

  // Log save card prompt result according to the closed reason.
  switch (current_bubble_type_) {
    case PaymentsBubbleType::kLocalCvcSave:
    case PaymentsBubbleType::kUploadCvcSave:
      autofill_metrics::LogSaveCvcPromptResultMetric(
          legacy_metric, is_upload_save_, is_reshow_);
      break;
    case PaymentsBubbleType::kLocalSave:
    case PaymentsBubbleType::kUploadSave:
      if (!is_reshow_) {
        autofill_metrics::LogSaveCreditCardPromptResultMetricDesktop(
            GetMetric<autofill_metrics::SaveCardPromptResultDesktop>(
                closed_reason),
            is_upload_save_,
            /*save_credit_card_options=*/options_, /*has_saved_cards=*/
            !payments_data_manager_->GetCreditCards().empty());
      }
      autofill_metrics::LogSaveCardPromptResultMetric(
          legacy_metric, is_upload_save_, is_reshow_, options_,
          payments_data_manager_->GetPaymentsSigninStateForMetrics(),
          /*has_saved_cards=*/
          !payments_data_manager_->GetCreditCards().empty());
      break;
    case PaymentsBubbleType::kUploadInProgress:
      autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
          legacy_metric);
      break;
    case PaymentsBubbleType::kUploadComplete:
      autofill_metrics::LogCreditCardUploadConfirmationViewResultMetric(
          legacy_metric, confirmation_ui_params_->is_success);
      break;
    case PaymentsBubbleType::kInactive:
    case PaymentsBubbleType::kManageCards:
      break;
  }
}

void SaveCardBubbleControllerImpl::OnBubbleClosed(
    PaymentsUiClosedReason closed_reason) {
  ResetBubbleViewAndInformBubbleManager();

  // If the dialog should be re-shown, do not change the bubble type or log
  // metrics.
  // TODO(crbug.com/316391673): Determine if we should track metrics on the
  // usage of this member.
  if (was_url_opened_) {
    return;
  }

  if (!bubble_hide_initiated_by_bubble_manager_) {
    LogBubbleCloseMetrics(closed_reason);
  }

  // If the bubble is closed with the current_bubble_type_ as
  // kUploadComplete, transition the current_bubble_type_ to kInactive, reset
  // the confirmation_ui_model and run `on_confirmation_closed_callback_`.
  if (current_bubble_type_ == PaymentsBubbleType::kUploadComplete) {
    current_bubble_type_ = PaymentsBubbleType::kInactive;
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
    case PaymentsUiClosedReason::kAccepted:
      user_decision = SaveCardOfferUserDecision::kAccepted;
      switch (current_bubble_type_) {
        case PaymentsBubbleType::kLocalSave:
        case PaymentsBubbleType::kLocalCvcSave:
          current_bubble_type_ = PaymentsBubbleType::kManageCards;
          break;
        case PaymentsBubbleType::kUploadSave:
        case PaymentsBubbleType::kUploadCvcSave:
        case PaymentsBubbleType::kManageCards:
          current_bubble_type_ = PaymentsBubbleType::kInactive;
          break;
        case PaymentsBubbleType::kInactive:
        case PaymentsBubbleType::kUploadInProgress:
        case PaymentsBubbleType::kUploadComplete:
          NOTREACHED();
      }
      break;
    case PaymentsUiClosedReason::kCancelled:
      user_decision = SaveCardOfferUserDecision::kDeclined;
      break;
    case PaymentsUiClosedReason::kClosed:
      user_decision = SaveCardOfferUserDecision::kIgnored;
      break;
    case PaymentsUiClosedReason::kUnknown:
    case PaymentsUiClosedReason::kNotInteracted:
    case PaymentsUiClosedReason::kLostFocus:
      break;
  }

  if (user_decision && *user_decision != SaveCardOfferUserDecision::kAccepted) {
    switch (current_bubble_type_) {
      case PaymentsBubbleType::kLocalSave:
      case PaymentsBubbleType::kLocalCvcSave:
        std::move(local_save_card_prompt_callback_).Run(*user_decision);
        break;
      case PaymentsBubbleType::kUploadSave:
      case PaymentsBubbleType::kUploadCvcSave:
        std::move(upload_save_card_prompt_callback_)
            .Run(*user_decision, /*user_provided_card_details=*/{});
        break;
      default:
        break;
    }
    current_bubble_type_ = PaymentsBubbleType::kInactive;
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

PaymentsBubbleType SaveCardBubbleControllerImpl::GetPaymentsBubbleType() const {
  return current_bubble_type_;
}

bool SaveCardBubbleControllerImpl::
    IsPaymentsSyncTransportEnabledWithoutSyncFeature() const {
  // TODO(crbug.com/40067296): Migrate away from IsSyncFeatureEnabled() when the
  // API returns false on desktop.
  return payments_data_manager_->IsPaymentsDownloadActive() &&
         !sync_service_->IsSyncFeatureEnabled();
}

void SaveCardBubbleControllerImpl::HideSaveCardBubble() {
  HideBubble(/*initiated_by_bubble_manager=*/false);
}

std::u16string SaveCardBubbleControllerImpl::GetSavePaymentIconTooltipText()
    const {
  switch (current_bubble_type_) {
    case PaymentsBubbleType::kLocalSave:
    case PaymentsBubbleType::kUploadSave:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD);
    case PaymentsBubbleType::kLocalCvcSave:
    case PaymentsBubbleType::kUploadCvcSave:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CVC);
    case PaymentsBubbleType::kManageCards:
      return l10n_util::GetStringUTF16(
          options_.card_save_type ==
                  payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly
              ? IDS_TOOLTIP_SAVE_CVC
              : IDS_TOOLTIP_SAVE_CREDIT_CARD);
    case PaymentsBubbleType::kUploadInProgress:
      return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD_PENDING);
    case PaymentsBubbleType::kUploadComplete:
    case PaymentsBubbleType::kInactive:
      return std::u16string();
  }
}

// TODO(crbug.com/374815809): Remove this method.
bool SaveCardBubbleControllerImpl::ShouldShowSavingPaymentAnimation() const {
  return false;
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
  if (current_bubble_type_ == PaymentsBubbleType::kInactive) {
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
  if (IsBubbleManagerEnabled()) {
    // BubbleManager will handle the effects of tab changes.
    return;
  }

  if (visibility == content::Visibility::VISIBLE &&
      (was_url_opened_ ||
       current_bubble_type_ == PaymentsBubbleType::kUploadComplete)) {
    ReshowBubble(/*is_user_gesture=*/false);
  } else if (visibility == content::Visibility::HIDDEN) {
    HideBubble(/*initiated_by_bubble_manager=*/false);
  }
}

std::optional<PageActionIconType>
SaveCardBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveCard;
}

#if !BUILDFLAG(IS_ANDROID)
std::optional<actions::ActionId>
SaveCardBubbleControllerImpl::GetActionIdForPageAction() {
  return kActionShowPaymentsBubbleOrPage;
}

std::optional<std::u16string>
SaveCardBubbleControllerImpl::GetPageActionTooltipText() {
  return GetSavePaymentIconTooltipText();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void SaveCardBubbleControllerImpl::DoShowBubble() {
  if (!IsWebContentsActive()) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (current_bubble_type_ == PaymentsBubbleType::kUploadComplete) {
    SetBubbleView(*browser->window()
                       ->GetAutofillBubbleHandler()
                       ->ShowSaveCardConfirmationBubble(web_contents(), this));
  } else {
    SetBubbleView(
        *browser->window()
             ->GetAutofillBubbleHandler()
             ->ShowSaveCreditCardBubble(web_contents(), this,
                                        is_triggered_by_user_gesture_));
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
    case PaymentsBubbleType::kUploadSave:
    case PaymentsBubbleType::kLocalSave:
      if (!is_reshow_) {
        autofill_metrics::LogSaveCreditCardPromptOfferMetricDesktop(
            autofill_metrics::SaveCardPromptOffer::kShown, is_upload_save_,
            /*save_credit_card_options=*/options_);
      }
      autofill_metrics::LogSaveCardPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kShown, is_upload_save_,
          is_reshow_, options_,
          payments_data_manager_->GetPaymentsSigninStateForMetrics());
      break;
    case PaymentsBubbleType::kUploadCvcSave:
    case PaymentsBubbleType::kLocalCvcSave:
      autofill_metrics::LogSaveCvcPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kShown, is_upload_save_,
          is_reshow_);
      break;
    case PaymentsBubbleType::kManageCards:
      CHECK(!is_upload_save_);
      LogManageCardsPromptMetric(ManageCardsPromptMetric::kManageCardsShown);
      break;
    case PaymentsBubbleType::kUploadComplete:
      autofill_metrics::LogCreditCardUploadConfirmationViewShownMetric(
          /*is_shown=*/true, confirmation_ui_params_->is_success);
      break;
    case PaymentsBubbleType::kUploadInProgress:
      break;
    case PaymentsBubbleType::kInactive:
      NOTREACHED();
  }
}

bool SaveCardBubbleControllerImpl::CanBeReshown() const {
  return current_bubble_type_ != PaymentsBubbleType::kUploadComplete &&
         current_bubble_type_ != PaymentsBubbleType::kInactive;
}

BubbleType SaveCardBubbleControllerImpl::GetBubbleType() const {
  return BubbleType::kSaveUpdateCard;
}

base::WeakPtr<BubbleControllerBase>
SaveCardBubbleControllerImpl::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SaveCardBubbleControllerImpl::CheckPreconditionsBeforeShowing() {
  CHECK(current_bubble_type_ != PaymentsBubbleType::kInactive);
  // Upload save callback should not be null for kUploadSave or
  // kUploadCvcSave state.
  CHECK(!upload_save_card_prompt_callback_.is_null() ||
        (current_bubble_type_ != PaymentsBubbleType::kUploadSave &&
         current_bubble_type_ != PaymentsBubbleType::kUploadCvcSave));
  // Local save callback should not be null for kLocalSave or kLocalCvcSave
  // state.
  CHECK(!local_save_card_prompt_callback_.is_null() ||
        (current_bubble_type_ != PaymentsBubbleType::kLocalSave &&
         current_bubble_type_ != PaymentsBubbleType::kLocalCvcSave));
  CHECK(!bubble_view());
}

void SaveCardBubbleControllerImpl::ShowIconOnly() {
  CHECK(current_bubble_type_ != PaymentsBubbleType::kInactive);
  // Upload save callback should not be null for kUploadSave or
  // kUploadCvcSave state.
  CHECK(!upload_save_card_prompt_callback_.is_null() ||
        (current_bubble_type_ != PaymentsBubbleType::kUploadSave &&
         current_bubble_type_ != PaymentsBubbleType::kUploadCvcSave));
  // Local save callback should not be null for kLocalSave or kLocalCvcSave
  // state.
  CHECK(!local_save_card_prompt_callback_.is_null() ||
        current_bubble_type_ != PaymentsBubbleType::kLocalSave &&
            current_bubble_type_ != PaymentsBubbleType::kLocalCvcSave);
  CHECK(!bubble_view());

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdatePageActionIcon();

  switch (current_bubble_type_) {
    case PaymentsBubbleType::kUploadSave:
    case PaymentsBubbleType::kLocalSave:
      if (!is_reshow_) {
        autofill_metrics::LogSaveCreditCardPromptOfferMetricDesktop(
            autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached,
            is_upload_save_, /*save_credit_card_options=*/options_);
      }
      autofill_metrics::LogSaveCardPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached,
          is_upload_save_, is_reshow_, options_,
          payments_data_manager_->GetPaymentsSigninStateForMetrics());
      break;
    case PaymentsBubbleType::kUploadCvcSave:
    case PaymentsBubbleType::kLocalCvcSave:
      autofill_metrics::LogSaveCvcPromptOfferMetric(
          autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached,
          is_upload_save_, is_reshow_);
      break;
    case PaymentsBubbleType::kUploadInProgress:
    case PaymentsBubbleType::kUploadComplete:
    case PaymentsBubbleType::kManageCards:
    case PaymentsBubbleType::kInactive:
      NOTREACHED();
  }
}

void SaveCardBubbleControllerImpl::OpenUrl(const GURL& url) {
  was_url_opened_ = true;

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

  // Return false if the tab is inactive, occluded by another window, or out
  // of screen bounds.
  return web_contents() &&
         web_contents()->GetVisibility() == content::Visibility::VISIBLE;
}

void SaveCardBubbleControllerImpl::EndSaveCardPromptFlow() {
  HideBubble(/*initiated_by_bubble_manager=*/false);
  current_bubble_type_ = PaymentsBubbleType::kInactive;
  confirmation_ui_params_.reset();
  UpdatePageActionIcon();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveCardBubbleControllerImpl);

}  // namespace autofill
