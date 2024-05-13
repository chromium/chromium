// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"

namespace autofill {

struct AutofillErrorDialogContext;
class AutofillSaveCardBottomSheetBridge;
enum class AutofillProgressDialogType;
class CardUnmaskDelegate;
struct CardUnmaskPromptOptions;
class CreditCard;
class CreditCardCvcAuthenticator;
class CreditCardOtpAuthenticator;
class Iban;
class CreditCardRiskBasedAuthenticator;
class MigratableCreditCard;
class OtpUnmaskDelegate;
struct CardUnmaskChallengeOption;
enum class OtpUnmaskResult;
class VirtualCardEnrollmentManager;

namespace payments {

class PaymentsNetworkInterface;
class PaymentsWindowManager;

// A payments-specific client interface that handles dependency injection, and
// its implementations serve as the integration for platform-specific code. One
// per WebContents, owned by the AutofillClient. Created lazily in the
// AutofillClient when it is needed.
class PaymentsAutofillClient : public RiskDataLoader {
 public:
  ~PaymentsAutofillClient() override;

  enum class SaveIbanOfferUserDecision {
    // The user accepted IBAN save.
    kAccepted,

    // The user explicitly declined IBAN save.
    kDeclined,

    // The user ignored the IBAN save prompt.
    kIgnored,
  };

  // Callback to run if user presses the Save button in the migration dialog.
  // Will pass a vector of GUIDs of cards that the user selected to upload to
  // LocalCardMigrationManager.
  using LocalCardMigrationCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  // Callback to run if the user presses the trash can button in the
  // action-required dialog. Will pass to LocalCardMigrationManager a
  // string of GUID of the card that the user selected to delete from local
  // storage.
  using MigrationDeleteCardCallback =
      base::RepeatingCallback<void(const std::string&)>;
  // Callback to run after local/upload IBAN save is offered. The callback runs
  // with `user_decision` indicating whether the prompt was accepted, declined,
  // or ignored. `nickname` is optionally provided by the user when IBAN local
  // or upload save is offered, and can be an empty string.
  using SaveIbanPromptCallback =
      base::OnceCallback<void(SaveIbanOfferUserDecision user_decision,
                              std::u16string_view nickname)>;

#if BUILDFLAG(IS_ANDROID)
  // Gets the AutofillSaveCardBottomSheetBridge or creates one if it doesn't
  // exist.
  virtual AutofillSaveCardBottomSheetBridge*
  GetOrCreateAutofillSaveCardBottomSheetBridge();
#elif !BUILDFLAG(IS_IOS)
  // Runs `show_migration_dialog_closure` if the user accepts the card
  // migration offer. This causes the card migration dialog to be shown.
  virtual void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure);

  // Shows a dialog with the given `legal_message_lines` and the `user_email`.
  // Runs `start_migrating_cards_callback` if the user would like the selected
  // cards in the `migratable_credit_cards` to be uploaded to cloud.
  virtual void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback);

  // Will show a dialog containing a error message if `has_server_error`
  // is true, or the migration results for cards in
  // `migratable_credit_cards` otherwise. If migration succeeds the dialog will
  // contain a `tip_message`. `migratable_credit_cards` will be used when
  // constructing the dialog. The dialog is invoked when the migration process
  // is finished. Runs `delete_local_card_callback` if the user chose to delete
  // one invalid card from local storage.
  virtual void ShowLocalCardMigrationResults(
      bool has_server_error,
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback);

  // Called after virtual card enrollment is finished. Shows enrollment
  // result to users. `is_vcn_enrolled` indicates if the card was successfully
  // enrolled as a virtual card.
  virtual void VirtualCardEnrollCompleted(bool is_vcn_enrolled);
#endif  // BUILDFLAG(IS_ANDROID)

  // Called after credit card upload is finished. Will show upload result to
  // users. `card_saved` indicates if the card is successfully saved.
  // TODO(crbug.com/40614280): This function is overridden in iOS codebase and
  // in the desktop codebase. If iOS is not using it to do anything, please keep
  // this function for desktop.
  virtual void CreditCardUploadCompleted(bool card_saved);

  // Returns true if save card offer or confirmation prompt is visible.
  virtual bool IsSaveCardPromptVisible() const;

  // Hides save card offer or confirmation prompt.
  virtual void HideSaveCardPromptPrompt();

  // Runs `callback` once the user makes a decision with respect to the
  // offer-to-save prompt. On desktop, shows the offer-to-save bubble if
  // `should_show_prompt` is true; otherwise only shows the omnibox icon.
  virtual void ConfirmSaveIbanLocally(const Iban& iban,
                                      bool should_show_prompt,
                                      SaveIbanPromptCallback callback);

  // Runs `callback` once the user makes a decision with respect to the
  // offer-to-upload prompt. On desktop, shows the offer-to-upload bubble if
  // `should_show_prompt` is true; otherwise only shows the omnibox icon.
  virtual void ConfirmUploadIbanToCloud(const Iban& iban,
                                        LegalMessageLines legal_message_lines,
                                        bool should_show_prompt,
                                        SaveIbanPromptCallback callback);

  // Show/dismiss the progress dialog which contains a throbber and a text
  // message indicating that something is in progress.
  virtual void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback);
  virtual void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_interactive_authentication_callback);

  // Show the OTP unmask dialog to accept user-input OTP value.
  virtual void ShowCardUnmaskOtpInputDialog(
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate);

  // Shows a dialog for the user to choose/confirm the authentication
  // to use in card unmasking.
  virtual void ShowUnmaskAuthenticatorSelectionDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmask_challenge_option_callback,
      base::OnceClosure cancel_unmasking_closure);

  // Dismisses the selection dialog to open the authentication dialog.
  // `server_success` dictates whether we received a success response
  // from the server, with true representing success and false representing
  // failure. A successful server response means that the issuer has sent an OTP
  // and we can move on to the next portion of this flow.
  // This should be invoked upon server accepting the authentication method, in
  // which case, we dismiss the selection dialog to open the authentication
  // dialog.
  virtual void DismissUnmaskAuthenticatorSelectionDialog(bool server_success);

  // Invoked when we receive the server response of the OTP unmask request.
  virtual void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result);

  // Gets the payments::PaymentsNetworkInterface instance owned by the client.
  virtual PaymentsNetworkInterface* GetPaymentsNetworkInterface();

  // Shows an error dialog when card retrieval errors happen. The type of error
  // dialog that is shown will match the `type` in `context`. If the
  // `server_returned_title` and `server_returned_description` in `context` are
  // both set, the error dialog that is displayed will have these fields
  // displayed for the title and description, respectively.
  virtual void ShowAutofillErrorDialog(AutofillErrorDialogContext context);

  // Gets the PaymentsWindowManager owned by the client.
  virtual PaymentsWindowManager* GetPaymentsWindowManager();

  // A user has attempted to use a masked card. Prompt them for further
  // information to proceed.
  virtual void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate);
  virtual void OnUnmaskVerificationResult(
      AutofillClient::PaymentsRpcResult result);

  // Returns a pointer to a VirtualCardEnrollmentManager that is owned by
  // PaymentsAutofillClient. VirtualCardEnrollmentManager is used for virtual
  // card enroll and unenroll related flows. This function will return a nullptr
  // on iOS WebView.
  virtual VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager();

  // Gets the CreditCardCvcAuthenticator owned by the client.
  virtual CreditCardCvcAuthenticator& GetCvcAuthenticator() = 0;

  // Gets the CreditCardOtpAuthenticator owned by the client. This function will
  // return a nullptr on iOS WebView.
  virtual CreditCardOtpAuthenticator* GetOtpAuthenticator();

  // Gets the RiskBasedAuthenticator owned by the client. This function will
  // return a nullptr on iOS WebView.
  virtual CreditCardRiskBasedAuthenticator* GetRiskBasedAuthenticator();
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
