// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill {

class AutofillErrorDialogControllerImpl;
class ContentAutofillClient;
class CardUnmaskOtpInputDialogControllerImpl;
class OtpUnmaskDelegate;
struct CardUnmaskChallengeOption;
enum class OtpUnmaskResult;

namespace payments {

class PaymentsWindowManager;

// Chrome implementation of PaymentsAutofillClient. Used for Chrome Desktop
// and Clank. Owned by the ChromeAutofillClient. Created lazily in the
// ChromeAutofillClient when it is needed, and it observes the same
// WebContents as its owning ChromeAutofillClient.
class ChromePaymentsAutofillClient : public PaymentsAutofillClient,
                                     public content::WebContentsObserver {
 public:
  explicit ChromePaymentsAutofillClient(ContentAutofillClient* client);
  ChromePaymentsAutofillClient(const ChromePaymentsAutofillClient&) = delete;
  ChromePaymentsAutofillClient& operator=(const ChromePaymentsAutofillClient&) =
      delete;
  ~ChromePaymentsAutofillClient() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // PaymentsAutofillClient:
#if !BUILDFLAG(IS_ANDROID)
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ShowLocalCardMigrationResults(
      bool has_server_error,
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback) override;
  void VirtualCardEnrollCompleted(bool is_vcn_enrolled) override;
#endif  // !BUILDFLAG(IS_ANDROID)
  void CreditCardUploadCompleted(bool card_saved) override;
  bool IsSaveCardPromptVisible() const override;
  void HideSaveCardPromptPrompt() override;
  void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) override;
  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_interactive_authentication_callback) override;
  void ShowCardUnmaskOtpInputDialog(
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate) override;
  void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result) override;
  PaymentsNetworkInterface* GetPaymentsNetworkInterface() override;
  void ShowAutofillErrorDialog(AutofillErrorDialogContext context) override;
  PaymentsWindowManager* GetPaymentsWindowManager() override;
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      AutofillClient::PaymentsRpcResult result) override;

  AutofillProgressDialogControllerImpl*
  AutofillProgressDialogControllerForTesting() {
    return autofill_progress_dialog_controller_.get();
  }

  std::unique_ptr<CardUnmaskPromptControllerImpl>
  ExtractCardUnmaskControllerForTesting() {
    return std::move(unmask_controller_);
  }
  void SetCardUnmaskControllerForTesting(
      std::unique_ptr<CardUnmaskPromptControllerImpl> test_controller) {
    unmask_controller_ = std::move(test_controller);
  }

 private:
  const raw_ref<ContentAutofillClient> client_;

  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;

  std::unique_ptr<AutofillProgressDialogControllerImpl>
      autofill_progress_dialog_controller_;

  std::unique_ptr<AutofillErrorDialogControllerImpl>
      autofill_error_dialog_controller_;

  std::unique_ptr<CardUnmaskOtpInputDialogControllerImpl>
      card_unmask_otp_input_dialog_controller_;

  std::unique_ptr<PaymentsWindowManager> payments_window_manager_;

  std::unique_ptr<CardUnmaskPromptControllerImpl> unmask_controller_;
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
