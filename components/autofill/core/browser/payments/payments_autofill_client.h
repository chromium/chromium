// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "components/autofill/core/browser/ui/suggestion.h"

#if !BUILDFLAG(IS_IOS)
namespace webauthn {
class InternalAuthenticator;
}
#endif

namespace autofill {

class AutofillDriver;
struct AutofillErrorDialogContext;
class AutofillOfferData;
class AutofillOfferManager;
enum class AutofillProgressDialogType;
class AutofillSaveCardBottomSheetBridge;
struct CardUnmaskChallengeOption;
class CardUnmaskDelegate;
struct CardUnmaskPromptOptions;
class CreditCard;
class CreditCardCvcAuthenticator;
class CreditCardOtpAuthenticator;
class CreditCardRiskBasedAuthenticator;
class Iban;
class IbanAccessManager;
class IbanManager;
class MerchantPromoCodeManager;
class MigratableCreditCard;
struct OfferNotificationOptions;
class OtpUnmaskDelegate;
enum class OtpUnmaskResult;
class TouchToFillDelegate;
struct VirtualCardEnrollmentFields;
class VirtualCardEnrollmentManager;
struct VirtualCardManualFallbackBubbleOptions;
enum class WebauthnDialogCallbackType;

namespace payments {

class MandatoryReauthManager;
class PaymentsNetworkInterface;
class PaymentsWindowManager;

// A payments-specific client interface that handles dependency injection, and
// its implementations serve as the integration for platform-specific code. One
// per WebContents, owned by the AutofillClient. Created lazily in the
// AutofillClient when it is needed.
class PaymentsAutofillClient : public RiskDataLoader {
 public:
  ~PaymentsAutofillClient() override;

  // The type of the credit card the Payments RPC fetches.
  enum class PaymentsRpcCardType {
    // Unknown type.
    kUnknown = 0,
    // Server card.
    kServerCard = 1,
    // Virtual card.
    kVirtualCard = 2,
  };

  enum class PaymentsRpcResult {
    // Empty result. Used for initializing variables and should generally
    // not be returned nor passed as arguments unless explicitly allowed by
    // the API.
    kNone,

    // Request succeeded.
    kSuccess,

    // Request failed; try again.
    kTryAgainFailure,

    // Request failed; don't try again.
    kPermanentFailure,

    // Unable to connect to Payments servers. Prompt user to check internet
    // connection.
    kNetworkError,

    // Request failed in retrieving virtual card information; try again.
    kVcnRetrievalTryAgainFailure,

    // Request failed in retrieving virtual card information; don't try again.
    kVcnRetrievalPermanentFailure,

    // Request took longer time to finish than the set client-side timeout.
    kClientSideTimeout,
  };

  enum class SaveIbanOfferUserDecision {
    // The user accepted IBAN save.
    kAccepted,

    // The user explicitly declined IBAN save.
    kDeclined,

    // The user ignored the IBAN save prompt.
    kIgnored,
  };

  enum class UnmaskCardReason {
    // The card is being unmasked for PaymentRequest.
    kPaymentRequest,

    // The card is being unmasked for Autofill.
    kAutofill,
  };

  // Authentication methods for card unmasking.
  enum class UnmaskAuthMethod {
    kUnknown = 0,
    // Require user to unmask via CVC.
    kCvc = 1,
    // Suggest use of FIDO authenticator for card unmasking.
    kFido = 2,
  };

  enum class CardSaveType {
    // Credit card is saved without the CVC.
    kCardSaveOnly = 0,
    // Credit card is saved with the CVC.
    kCardSaveWithCvc = 1,
    // Only CVC is saved.
    kCvcSaveOnly = 2,
  };

  // Used for options of upload prompt.
  struct SaveCreditCardOptions {
    SaveCreditCardOptions& with_should_request_name_from_user(bool b) {
      should_request_name_from_user = b;
      return *this;
    }

    SaveCreditCardOptions& with_should_request_expiration_date_from_user(
        bool b) {
      should_request_expiration_date_from_user = b;
      return *this;
    }

    SaveCreditCardOptions& with_show_prompt(bool b = true) {
      show_prompt = b;
      return *this;
    }

    SaveCreditCardOptions& with_has_multiple_legal_lines(bool b = true) {
      has_multiple_legal_lines = b;
      return *this;
    }

    SaveCreditCardOptions&
    with_same_last_four_as_server_card_but_different_expiration_date(bool b) {
      has_same_last_four_as_server_card_but_different_expiration_date = b;
      return *this;
    }

    SaveCreditCardOptions& with_card_save_type(CardSaveType b) {
      card_save_type = b;
      return *this;
    }

    bool should_request_name_from_user = false;
    bool should_request_expiration_date_from_user = false;
    bool show_prompt = false;
    bool has_multiple_legal_lines = false;
    bool has_same_last_four_as_server_card_but_different_expiration_date =
        false;
    CardSaveType card_save_type = CardSaveType::kCardSaveOnly;
  };

  enum class SaveCardOfferUserDecision {
    // The user accepted credit card save.
    kAccepted,

    // The user explicitly declined credit card save.
    kDeclined,

    // The user ignored the credit card save prompt.
    kIgnored,
  };

  // Used for explicitly requesting the user to enter/confirm cardholder name,
  // expiration date month and year.
  struct UserProvidedCardDetails {
    std::u16string cardholder_name;
    std::u16string expiration_date_month;
    std::u16string expiration_date_year;
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

  // Callback to run after credit card or IBAN upload confirmation prompt is
  // closed.
  using OnConfirmationClosedCallback = base::OnceClosure;

  // Callback to run if the OK button or the cancel button in a
  // Webauthn dialog is clicked.
  using WebauthnDialogCallback =
      base::RepeatingCallback<void(WebauthnDialogCallbackType)>;

  // Callback to run when the credit card has been scanned.
  using CreditCardScanCallback = base::OnceCallback<void(const CreditCard&)>;

  // Callback to run after local credit card save or local CVC save is offered.
  // Sends whether the prompt was accepted, declined, or ignored in
  // `user_decision`.
  using LocalSaveCardPromptCallback =
      base::OnceCallback<void(SaveCardOfferUserDecision user_decision)>;

  // Callback to run after upload credit card save or upload CVC save for
  // existing server card is offered. Sends whether the prompt was accepted,
  // declined, or ignored in `user_decision`, and additional
  // `user_provided_card_details` if applicable.
  using UploadSaveCardPromptCallback = base::OnceCallback<void(
      SaveCardOfferUserDecision user_decision,
      const UserProvidedCardDetails& user_provided_card_details)>;

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

  // TODO(crbug.com/40639086): Find a way to merge these two functions.
  // Shouldn't use WebauthnDialogState as that state is a purely UI state
  // (should not be accessible for managers?), and some of the states
  // `KInactive` may be confusing here. Do we want to add another Enum?

  // Will show a dialog offering the option to use device's platform
  // authenticator in the future instead of CVC to verify the card being
  // unmasked. Runs `offer_dialog_callback` if the OK button or the cancel
  // button in the dialog is clicked.
  virtual void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback);

  // Will show a dialog indicating the card verification is in progress. It is
  // shown after verification starts only if the WebAuthn is enabled.
  virtual void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback);

  // Will update the WebAuthn dialog content when there is an error fetching the
  // challenge.
  virtual void UpdateWebauthnOfferDialogWithError();

  // Will close the current visible WebAuthn dialog. Returns true if dialog was
  // visible and has been closed.
  virtual bool CloseWebauthnDialog();

  // Hides the virtual card enroll bubble and icon if it is visible.
  virtual void HideVirtualCardEnrollBubbleAndIconIfVisible();
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Display the cardholder name fix flow prompt and run the `callback` if
  // the card should be uploaded to payments with updated name from the user.
  virtual void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback);

  // Display the expiration date fix flow prompt with the `card` details
  // and run the `callback` if the card should be uploaded to payments with
  // updated expiration date from the user.
  virtual void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Returns true if both the platform and the device support scanning credit
  // cards. Should be called before ScanCreditCard().
  virtual bool HasCreditCardScanFeature() const;

  // Shows the user interface for scanning a credit card. Invokes the `callback`
  // when a credit card is scanned successfully. Should be called only if
  // HasCreditCardScanFeature() returns true.
  virtual void ScanCreditCard(CreditCardScanCallback callback);

  // Runs `callback` once the user makes a decision with respect to the
  // offer-to-save prompt. This includes both the save local card prompt and the
  // save CVC for a local card prompt. On desktop, shows the offer-to-save
  // bubble if `options.show_prompt` is true; otherwise only shows the omnibox
  // icon. On mobile, shows the offer-to-save infobar if `options.show_prompt`
  // is true; otherwise does not offer to save at all.
  virtual void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback);

  // Runs `callback` once the user makes a decision with respect to the
  // offer-to-save prompt. This includes both the save server card prompt and
  // the save CVC for a server card prompt. Displays the contents of
  // `legal_message_lines` to the user. Displays a cardholder name textfield in
  // the bubble if `options.should_request_name_from_user` is true. Displays a
  // pair of expiration date dropdowns in the bubble if
  // `should_request_expiration_date_from_user` is true. On desktop, shows the
  // offer-to-save bubble if `options.show_prompt` is true;
  // otherwise only shows the omnibox icon. On mobile, shows the offer-to-save
  // infobar if `options.show_prompt` is true; otherwise does
  // not offer to save at all.
  // TODO (crbug.com/1462821): Make `legal_message_lines` optional, as CVC
  // upload has no legal message.
  virtual void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback);

  // Shows upload result to users. Called after credit card upload is finished.
  // `result` holds the outcome for credit card upload.
  // `on_confirmation_closed_callback` should run after confirmation prompt is
  // closed.
  // TODO(crbug.com/40614280): This function is overridden in iOS codebase and
  // in the desktop codebase. If iOS is not using it to do anything, please keep
  // this function for desktop.
  virtual void CreditCardUploadCompleted(
      PaymentsRpcResult result,
      std::optional<OnConfirmationClosedCallback>
          on_confirmation_closed_callback);

  // Hides save card offer or confirmation prompt.
  virtual void HideSaveCardPrompt();

  // Shows a dialog for the user to enroll in a virtual card.
  virtual void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback);

  // Called after virtual card enrollment is finished. Shows enrollment
  // result to users. `result` holds the outcome of virtual card enrollment.
  virtual void VirtualCardEnrollCompleted(PaymentsRpcResult result);

  // Called when the virtual card has been fetched successfully. Uses the
  // necessary information in `options` to show the manual fallback bubble.
  virtual void OnVirtualCardDataAvailable(
      const VirtualCardManualFallbackBubbleOptions& options);

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

  // Shows upload result to users. Called after IBAN upload is finished.
  // `iban_saved` indicates if the IBAN was successfully saved.
  // `hit_max_strikes` indicates whether the maximum number of strikes has been
  // reached when the offer to upload IBAN request fails.
  virtual void IbanUploadCompleted(bool iban_saved, bool hit_max_strikes);

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
  virtual void OnUnmaskVerificationResult(PaymentsRpcResult result);

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

  // Prompt the user to enable mandatory reauthentication for payment method
  // autofill. When enabled, the user will be asked to authenticate using
  // biometrics or device unlock before filling in payment method information.
  virtual void ShowMandatoryReauthOptInPrompt(
      base::OnceClosure accept_mandatory_reauth_callback,
      base::OnceClosure cancel_mandatory_reauth_callback,
      base::RepeatingClosure close_mandatory_reauth_callback);

  // Gets the IbanManager instance associated with the client.
  virtual IbanManager* GetIbanManager();

  // Gets the IbanAccessManager instance associated with the client.
  virtual IbanAccessManager* GetIbanAccessManager();

  // Gets the MerchantPromoCodeManager instance associated with the
  // client (can be null for unsupported platforms).
  virtual MerchantPromoCodeManager* GetMerchantPromoCodeManager();

  // Should only be called when we are sure re-showing the bubble will display a
  // confirmation bubble. If the most recent bubble was an opt-in bubble and it
  // was accepted, this will display the re-auth opt-in confirmation bubble.
  virtual void ShowMandatoryReauthOptInConfirmation();

  // TODO(crbug.com/40134864): Rename all the "domain" in this flow to origin.
  //                          The server is passing down full origin of the
  //                          urls. "Domain" is no longer accurate.
  // Notifies the client to update the offer notification when the `offer` is
  // available. `options` carries extra configuration options for the offer
  // notification.
  virtual void UpdateOfferNotification(const AutofillOfferData& offer,
                                       const OfferNotificationOptions& options);

  // Dismiss any visible offer notification on the current tab.
  virtual void DismissOfferNotification();

  // Navigates to `url` in a new tab. `url` links to the promo code offer
  // details page for the offers in a promo code suggestions popup. Every offer
  // in a promo code suggestions popup links to the same offer details page.
  virtual void OpenPromoCodeOfferDetailsURL(const GURL& url);

  // Gets an AutofillOfferManager instance (can be null for unsupported
  // platforms).
  virtual AutofillOfferManager* GetAutofillOfferManager();
  const AutofillOfferManager* GetAutofillOfferManager() const;

  // Shows the Touch To Fill surface for filling credit card information, if
  // possible, and returns `true` on success. `delegate` will be notified of
  // events. `suggestions` are generated using the `cards_to_suggest` data and
  // include fields such as `main_text`, `minor_text`, and
  // `apply_deactivated_style`. Should be called only if the feature is
  // supported by the platform. This function is implemented on all platforms,
  // so this should be a pure virtual function to enforce the override
  // implementation.
  virtual bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest,
      base::span<const Suggestion> suggestions);

  // Shows the Touch To Fill surface for filling IBAN information, if
  // possible, returning `true` on success. `delegate` will be notified of
  // events. This function is not implemented on iOS and iOS WebView, and
  // should not be used on those platforms.
  virtual bool ShowTouchToFillIban(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::Iban> ibans_to_suggest);

  // Hides the Touch To Fill surface for filling payment information if one is
  // currently shown. Should be called only if the feature is supported by the
  // platform.
  virtual void HideTouchToFillPaymentMethod();

#if !BUILDFLAG(IS_IOS)
  // Creates the appropriate implementation of InternalAuthenticator. May be
  // null for platforms that don't support this, in which case standard CVC
  // authentication will be used instead.
  virtual std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver);
#endif

  // Gets or creates a payments autofill mandatory re-auth manager. This will be
  // used to handle payments mandatory re-auth related flows.
  virtual payments::MandatoryReauthManager*
  GetOrCreatePaymentsMandatoryReauthManager();
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
