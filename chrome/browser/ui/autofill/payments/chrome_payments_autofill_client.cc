// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/core/browser/metrics/payments/risk_data_metrics.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill::payments {

ChromePaymentsAutofillClient::ChromePaymentsAutofillClient(
    content::WebContents* web_contents) {
  content::WebContentsObserver::Observe(web_contents);
}

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

}  // namespace autofill::payments
