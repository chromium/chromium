// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/risk_data_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill::payments {

ChromePaymentsAutofillClient::ChromePaymentsAutofillClient(
    ContentAutofillClient* client)
    : content::WebContentsObserver(&client->GetWebContents()),
      client_(CHECK_DEREF(client)) {}

ChromePaymentsAutofillClient::~ChromePaymentsAutofillClient() = default;

void ChromePaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  risk_util::LoadRiskData(
      0, web_contents(),
      base::BindOnce(
          [](base::OnceCallback<void(const std::string&)> callback,
             base::TimeTicks start_time, const std::string& risk_data) {
            autofill::autofill_metrics::LogRiskDataLoadingLatency(
                base::TimeTicks::Now() - start_time);
            std::move(callback).Run(risk_data);
          },
          std::move(callback), base::TimeTicks::Now()));
}

#if !BUILDFLAG(IS_ANDROID)
void ChromePaymentsAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  ManageMigrationUiController::CreateForWebContents(web_contents());
  ManageMigrationUiController* controller =
      ManageMigrationUiController::FromWebContents(web_contents());
  controller->ShowBubble(std::move(show_migration_dialog_closure));
}

void ChromePaymentsAutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  ManageMigrationUiController::CreateForWebContents(web_contents());
  ManageMigrationUiController* controller =
      ManageMigrationUiController::FromWebContents(web_contents());
  controller->ShowOfferDialog(legal_message_lines, user_email,
                              migratable_credit_cards,
                              std::move(start_migrating_cards_callback));
}

void ChromePaymentsAutofillClient::ShowLocalCardMigrationResults(
    bool has_server_error,
    const std::u16string& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {
  ManageMigrationUiController::CreateForWebContents(web_contents());
  ManageMigrationUiController* controller =
      ManageMigrationUiController::FromWebContents(web_contents());
  controller->UpdateCreditCardIcon(has_server_error, tip_message,
                                   migratable_credit_cards,
                                   delete_local_card_callback);
}

void ChromePaymentsAutofillClient::VirtualCardEnrollCompleted(
    bool is_vcn_enrolled) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
    VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
    VirtualCardEnrollBubbleControllerImpl* controller =
        VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());
    if (controller && controller->IsIconVisible()) {
      controller->ShowConfirmationBubbleView(is_vcn_enrolled);
    }
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

void ChromePaymentsAutofillClient::CreditCardUploadCompleted(bool card_saved) {
#if !BUILDFLAG(IS_ANDROID)
  if (SaveCardBubbleControllerImpl* controller =
          SaveCardBubbleControllerImpl::FromWebContents(web_contents())) {
    controller->ShowConfirmationBubbleView(card_saved);
  }
#endif
}

bool ChromePaymentsAutofillClient::IsSaveCardPromptVisible() const {
#if !BUILDFLAG(IS_ANDROID)
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  return controller && controller->IsIconVisible();
#else
  return false;
#endif
}

void ChromePaymentsAutofillClient::HideSaveCardPromptPrompt() {
#if !BUILDFLAG(IS_ANDROID)
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  if (controller) {
    controller->HideSaveCardBubble();
  }
#endif
}

void ChromePaymentsAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  autofill_progress_dialog_controller_ =
      std::make_unique<AutofillProgressDialogControllerImpl>(
          autofill_progress_dialog_type, std::move(cancel_callback));
  autofill_progress_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowProgressDialog,
                     autofill_progress_dialog_controller_->GetWeakPtr(),
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {
  DCHECK(autofill_progress_dialog_controller_);
  autofill_progress_dialog_controller_->DismissDialog(
      show_confirmation_before_closing,
      std::move(no_interactive_authentication_callback));
}

void ChromePaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  card_unmask_otp_input_dialog_controller_ =
      std::make_unique<CardUnmaskOtpInputDialogControllerImpl>(challenge_option,
                                                               delegate);
  card_unmask_otp_input_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowOtpInputDialog,
                     card_unmask_otp_input_dialog_controller_->GetWeakPtr(),
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  CHECK(card_unmask_otp_input_dialog_controller_);
  card_unmask_otp_input_dialog_controller_->OnOtpVerificationResult(
      unmask_result);
}

PaymentsNetworkInterface*
ChromePaymentsAutofillClient::GetPaymentsNetworkInterface() {
  if (!payments_network_interface_) {
    payments_network_interface_ = std::make_unique<PaymentsNetworkInterface>(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->GetURLLoaderFactory(),
        client_->GetIdentityManager(),
        &client_->GetPersonalDataManager()->payments_data_manager(),
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->IsOffTheRecord());
  }
  return payments_network_interface_.get();
}

void ChromePaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext context) {
  autofill_error_dialog_controller_ =
      std::make_unique<AutofillErrorDialogControllerImpl>(std::move(context));
  autofill_error_dialog_controller_->Show(
      base::BindOnce(&CreateAndShowAutofillErrorDialog,
                     base::Unretained(autofill_error_dialog_controller_.get()),
                     base::Unretained(web_contents())));
}

PaymentsWindowManager*
ChromePaymentsAutofillClient::GetPaymentsWindowManager() {
#if !BUILDFLAG(IS_ANDROID)
  if (!payments_window_manager_) {
    payments_window_manager_ =
        std::make_unique<DesktopPaymentsWindowManager>(&*client_);
  }

  return payments_window_manager_.get();
#else
  return nullptr;
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_ = std::make_unique<CardUnmaskPromptControllerImpl>(
      user_prefs::UserPrefs::Get(client_->GetWebContents().GetBrowserContext()),
      card, card_unmask_prompt_options, delegate);
  unmask_controller_->ShowPrompt(base::BindOnce(
      &CreateCardUnmaskPromptView, base::Unretained(unmask_controller_.get()),
      base::Unretained(web_contents())));
}

// TODO(crbug.com/40186650): Refactor this for both CVC and Biometrics flows.
void ChromePaymentsAutofillClient::OnUnmaskVerificationResult(
    AutofillClient::PaymentsRpcResult result) {
  if (unmask_controller_) {
    unmask_controller_->OnVerificationResult(result);
  }
#if BUILDFLAG(IS_ANDROID)
  // For VCN-related errors, on Android we show a new error dialog instead of
  // updating the CVC unmask prompt with the error message.
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/true));
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/false));
      break;
    case AutofillClient::PaymentsRpcResult::kSuccess:
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      // Do nothing
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace autofill::payments
