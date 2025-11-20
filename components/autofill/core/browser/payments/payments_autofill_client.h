// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/signin/public/identity_manager/account_info.h"

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
class AutofillSaveIbanBottomSheetBridge;
class BnplIssuer;
struct BnplTosModel;
struct CardUnmaskChallengeOption;
class CardUnmaskDelegate;
class AutofillProgressDialogController;
class CardUnmaskOtpInputDialogController;
class CardUnmaskPromptController;
struct CardUnmaskPromptOptions;
class CreditCard;
class CreditCardCvcAuthenticator;
class CreditCardOtpAuthenticator;
class CreditCardRiskBasedAuthenticator;
class Iban;
class IbanAccessManager;
class IbanManager;
class LoyaltyCard;
class MerchantPromoCodeManager;
struct OfferNotificationOptions;
class OtpUnmaskDelegate;
class PaymentsDataManager;
enum class OtpUnmaskResult;
class TouchToFillDelegate;
struct VirtualCardEnrollmentFields;
class VirtualCardEnrollmentManager;
struct FilledCardInformationBubbleOptions;
enum class WebauthnDialogCallbackType;

namespace payments {

struct BnplIssuerContext;
class BnplStrategy;
class BnplUiDelegate;
class MandatoryReauthManager;
class MultipleRequestPaymentsNetworkInterface;
class PaymentsNetworkInterface;
class PaymentsWindowManager;
class SaveAndFillManager;

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

    SaveCreditCardOptions& with_num_strikes(const int strikes) {
      num_strikes = strikes;
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
    std::optional<int> num_strikes;
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

  // Carries card details that were explicitly provided or confirmed by the
  // user in a save/update UI. This can include data from a fix flow
  // (e.g., corrected name) or optional data from an initial save (e.g., CVC).
  struct UserProvidedCardDetails {
    UserProvidedCardDetails();
    UserProvidedCardDetails(const UserProvidedCardDetails&);
    UserProvidedCardDetails& operator=(const UserProvidedCardDetails&);
    UserProvidedCardDetails(UserProvidedCardDetails&&);
    UserProvidedCardDetails& operator=(UserProvidedCardDetails&&);
    ~UserProvidedCardDetails();
    std::u16string cardholder_name;
    std::u16string expiration_date_month;
    std::u16string expiration_date_year;
    std::u16string cvc;
  };

  enum class CardSaveAndFillDialogUserDecision {
    // The user accepted credit card Save and Fill dialog.
    kAccepted,

    // The user explicitly declined credit card Save and Fill dialog.
    kDeclined,
  };

  // Used to hold the data entered by the user in the Save and Fill dialog,
  // including card number, expiration date, name on card, and an optional
  // security code.
  struct UserProvidedCardSaveAndFillDetails : public UserProvidedCardDetails {
    UserProvidedCardSaveAndFillDetails();
    UserProvidedCardSaveAndFillDetails(
        const UserProvidedCardSaveAndFillDetails&);
    UserProvidedCardSaveAndFillDetails& operator=(
        const UserProvidedCardSaveAndFillDetails&);
    ~UserProvidedCardSaveAndFillDetails();

    std::u16string card_number;
    std::optional<std::u16string> security_code;
  };

  // Callback to run after the local/upload card Save and Fill dialog is shown.
  // The callback runs with `user_decision` indicating whether the dialog was
  // accepted, declined, or ignored. `user_provided_card_save_and_fill_details`
  // contains the data entered by the user, such as card number, expiration
  // date, name on card, and security code.
  using CardSaveAndFillDialogCallback =
      base::OnceCallback<void(CardSaveAndFillDialogUserDecision user_decision,
                              const UserProvidedCardSaveAndFillDetails&
                                  user_provided_card_save_and_fill_details)>;

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
  GetOrCreateAutofillSaveCardBottomSheetBridge() = 0;

  // Gets the AutofillSaveIbanBottomSheetBridge or creates one if it doesn't
  // exist.
  virtual AutofillSaveIbanBottomSheetBridge*
  GetOrCreateAutofillSaveIbanBottomSheetBridge() = 0;
#elif !BUILDFLAG(IS_IOS)  // && !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40639086): Find a way to merge these two functions.
  // Shouldn't use WebauthnDialogState as that state is a purely UI state
  // (should not be accessible for managers?), and some of the states
  // `KInactive` may be confusing here. Do we want to add another Enum?

  // Will show a dialog offering the option to use device's platform
  // authenticator in the future instead of CVC to verify the card being
  // unmasked. Runs `offer_dialog_callback` if the OK button or the cancel
  // button in the dialog is clicked.
  virtual void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback) = 0;

  // Will show a dialog indicating the card verification is in progress. It is
  // shown after verification starts only if the WebAuthn is enabled.
  virtual void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback) = 0;

  // Will update the WebAuthn dialog content when there is an error fetching the
  // challenge.
  virtual void UpdateWebauthnOfferDialogWithError() = 0;

  // Will close the current visible WebAuthn dialog. Returns true if dialog was
  // visible and has been closed.
  virtual bool CloseWebauthnDialog() = 0;

  // Hides the virtual card enroll bubble and icon if it is visible.
  virtual void HideVirtualCardEnrollBubbleAndIconIfVisible() = 0;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Display the cardholder name fix flow prompt and run the `callback` if
  // the card should be uploaded to payments with updated name from the user.
  virtual void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) = 0;

  // Display the expiration date fix flow prompt with the `card` details
  // and run the `callback` if the card should be uploaded to payments with
  // updated expiration date from the user.
  virtual void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) = 0;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Returns true if both the platform and the device support scanning credit
  // cards. Should be called before ScanCreditCard().
  virtual bool HasCreditCardScanFeature() const = 0;

  // Shows the user interface for scanning a credit card. Invokes the `callback`
  // when a credit card is scanned successfully. Should be called only if
  // HasCreditCardScanFeature() returns true.
  virtual void ScanCreditCard(CreditCardScanCallback callback) = 0;

  // Returns true if credit card local save is supported by the client.
  virtual bool LocalCardSaveIsSupported() = 0;

  // Runs `callback` once the user makes a decision with respect to the
  // offer-to-save prompt. This includes both the save local card prompt and the
  // save CVC for a local card prompt. On desktop, shows the offer-to-save
  // bubble if `options.show_prompt` is true; otherwise only shows the omnibox
  // icon. On mobile, shows the offer-to-save infobar if `options.show_prompt`
  // is true; otherwise does not offer to save at all.
  virtual void ShowSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) = 0;

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
  virtual void ShowSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) = 0;

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
          on_confirmation_closed_callback) = 0;

  // Hides save card offer or confirmation prompt.
  virtual void HideSaveCardPrompt() = 0;

  // Shows a dialog for the user to enroll in a virtual card.
  virtual void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) = 0;

  // Called after virtual card enrollment is finished. Shows enrollment
  // result to users. `result` holds the outcome of virtual card enrollment.
  virtual void VirtualCardEnrollCompleted(PaymentsRpcResult result) = 0;

  // Called when the card has been fetched successfully. Uses the necessary
  // information in `options` to show the FilledCardInformationBubble.
  virtual void OnCardDataAvailable(
      const FilledCardInformationBubbleOptions& options) = 0;

  // Runs `callback` once the user makes a decision with respect to the
  // offer-to-save prompt. On desktop, shows the offer-to-save bubble if
  // `should_show_prompt` is true; otherwise only shows the omnibox icon.
  virtual void ConfirmSaveIbanLocally(const Iban& iban,
                                      bool should_show_prompt,
                                      SaveIbanPromptCallback callback) = 0;

  // Runs `callback` once the user makes a decision with respect to the
  // offer-to-upload prompt. On desktop, shows the offer-to-upload bubble if
  // `should_show_prompt` is true; otherwise only shows the omnibox icon.
  virtual void ConfirmUploadIbanToCloud(const Iban& iban,
                                        LegalMessageLines legal_message_lines,
                                        bool should_show_prompt,
                                        SaveIbanPromptCallback callback) = 0;

  // Shows upload result to users. Called after IBAN upload is finished.
  // `iban_saved` indicates if the IBAN was successfully saved.
  // `hit_max_strikes` indicates whether the maximum number of strikes has been
  // reached when the offer to upload IBAN request fails.
  virtual void IbanUploadCompleted(bool iban_saved, bool hit_max_strikes) = 0;

  // Show/dismiss the progress dialog which contains a throbber and a text
  // message indicating that something is in progress.
  virtual void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) = 0;
  virtual void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_interactive_authentication_callback) = 0;

  // Show the OTP unmask dialog to accept user-input OTP value.
  virtual void ShowCardUnmaskOtpInputDialog(
      CreditCard::RecordType card_type,
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate) = 0;

  // Invoked when we receive the server response of the OTP unmask request.
  virtual void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result) = 0;

  // Shows a dialog for the user to choose/confirm the authentication
  // to use in card unmasking.
  virtual void ShowUnmaskAuthenticatorSelectionDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmask_challenge_option_callback,
      base::OnceClosure cancel_unmasking_closure) = 0;

  // Dismisses the selection dialog to open the authentication dialog.
  // `server_success` dictates whether we received a success response
  // from the server, with true representing success and false representing
  // failure. A successful server response means that the issuer has sent an OTP
  // and we can move on to the next portion of this flow.
  // This should be invoked upon server accepting the authentication method, in
  // which case, we dismiss the selection dialog to open the authentication
  // dialog.
  virtual void DismissUnmaskAuthenticatorSelectionDialog(
      bool server_success) = 0;

  // Gets the payments::PaymentsNetworkInterface instance owned by the client.
  virtual PaymentsNetworkInterface* GetPaymentsNetworkInterface() = 0;

  // Same as above. However this network interface can support multiple active
  // requests at a time. Sending a request will not affect other ongoing
  // requests. This is a complete upgrade of the
  // `PaymentsNetworkInterface` so all new flows should use this
  // function. All existing flows should be migrated to this. Note that since
  // each flow should migrate in its own effort, we would need to keep these
  // functions separate, instead of updating the logic inside
  // GetPaymentsNetworkInterface. When all migrations are finished, above
  // function and the PaymentsNetworkInterface class should be cleaned up.
  virtual MultipleRequestPaymentsNetworkInterface*
  GetMultipleRequestPaymentsNetworkInterface() = 0;

  // Shows an error dialog when card retrieval errors happen. The type of error
  // dialog that is shown will match the `type` in `context`. If the
  // `server_returned_title` and `server_returned_description` in `context` are
  // both set, the error dialog that is displayed will have these fields
  // displayed for the title and description, respectively.
  virtual void ShowAutofillErrorDialog(AutofillErrorDialogContext context) = 0;

  // Gets the PaymentsWindowManager owned by the client.
  virtual PaymentsWindowManager* GetPaymentsWindowManager() = 0;

  // A user has attempted to use a masked card. Prompt them for further
  // information to proceed.
  virtual void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) = 0;

  virtual void OnUnmaskVerificationResult(PaymentsRpcResult result) = 0;

#if BUILDFLAG(IS_IOS)
  virtual std::unique_ptr<AutofillProgressDialogController>
  ExtractProgressDialogModel() = 0;

  virtual std::unique_ptr<CardUnmaskOtpInputDialogController>
  ExtractOtpInputDialogModel() = 0;

  virtual CardUnmaskPromptController* GetCardUnmaskPromptModel() = 0;
#endif

  // Returns a pointer to a VirtualCardEnrollmentManager that is owned by
  // PaymentsAutofillClient. VirtualCardEnrollmentManager is used for virtual
  // card enroll and unenroll related flows. This function will return a nullptr
  // on iOS WebView.
  virtual VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() = 0;

  // Gets the CreditCardCvcAuthenticator owned by the client.
  virtual CreditCardCvcAuthenticator& GetCvcAuthenticator() = 0;

  // Gets the CreditCardOtpAuthenticator owned by the client. This function will
  // return a nullptr on iOS WebView.
  virtual CreditCardOtpAuthenticator* GetOtpAuthenticator() = 0;

  // Gets the RiskBasedAuthenticator owned by the client. This function will
  // return a nullptr on iOS WebView.
  virtual CreditCardRiskBasedAuthenticator* GetRiskBasedAuthenticator() = 0;

  // Returns true if Hagrid (risk based authentication) is supported on this
  // platform. Override in subclasses, return true in supported platform,
  // defaults to false.
  virtual bool IsRiskBasedAuthEffectivelyAvailable() const = 0;

  // Returns true if Mandatory Reauth is supported on this platform and enabled
  // by the user, if applicable.
  virtual bool IsMandatoryReauthEnabled() = 0;

  // Prompt the user to enable mandatory reauthentication for payment method
  // autofill. When enabled, the user will be asked to authenticate using
  // biometrics or device unlock before filling in payment method information.
  virtual void ShowMandatoryReauthOptInPrompt(
      base::OnceClosure accept_mandatory_reauth_callback,
      base::OnceClosure cancel_mandatory_reauth_callback,
      base::RepeatingClosure close_mandatory_reauth_callback) = 0;

  // Should only be called when we are sure re-showing the bubble will display a
  // confirmation bubble. If the most recent bubble was an opt-in bubble and it
  // was accepted, this will display the re-auth opt-in confirmation bubble.
  virtual void ShowMandatoryReauthOptInConfirmation() = 0;

  // Returns true if the value of the AutofillCreditCardEnabled pref is true
  // and the client supports Autofill.
  virtual bool IsAutofillPaymentMethodsEnabled() const = 0;

  // Disables payments autofill support for this client. Used when the client's
  // WebContents does not support autofill, such as in an Ephemeral Tab.
  virtual void DisablePaymentsAutofill() = 0;

  // Gets the IbanManager instance associated with the client.
  virtual IbanManager* GetIbanManager() = 0;

  // Gets the IbanAccessManager instance associated with the client.
  virtual IbanAccessManager* GetIbanAccessManager() = 0;

  // Gets the MerchantPromoCodeManager instance associated with the
  // client (can be null for unsupported platforms).
  virtual MerchantPromoCodeManager* GetMerchantPromoCodeManager() = 0;

  // Navigates to `url` in a new tab. `url` links to the promo code offer
  // details page for the offers in a promo code suggestions popup. Every offer
  // in a promo code suggestions popup links to the same offer details page.
  virtual void OpenPromoCodeOfferDetailsURL(const GURL& url) = 0;

  // Gets an AutofillOfferManager instance (can be null for unsupported
  // platforms).
  virtual AutofillOfferManager* GetAutofillOfferManager() = 0;

  // Gets a const version of the AutofillOfferManager.
  const AutofillOfferManager* GetAutofillOfferManager() const;

  // TODO(crbug.com/40134864): Rename all the "domain" in this flow to origin.
  //                          The server is passing down full origin of the
  //                          urls. "Domain" is no longer accurate.
  // Notifies the client to update the offer notification when the `offer` is
  // available. `options` carries extra configuration options for the offer
  // notification.
  virtual void UpdateOfferNotification(
      const AutofillOfferData& offer,
      const OfferNotificationOptions& options) = 0;

  // Dismiss any visible offer notification on the current tab.
  virtual void DismissOfferNotification() = 0;

  // Shows the Touch To Fill surface for filling credit card information, if
  // possible, and returns `true` on success. `delegate` will be notified of
  // events. `suggestions` are generated using the `cards_to_suggest` data and
  // include fields such as `main_text`, `minor_text`, and
  // `HasDeactivatedStyle` member function. Should be called only if the feature
  // is supported by the platform. This function is implemented on all
  // platforms so this should be a pure virtual function to enforce the override
  // implementation.
  virtual bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const Suggestion> suggestions) = 0;

  // Shows the Touch To Fill surface for filling IBAN information, if
  // possible, returning `true` on success. `delegate` will be notified of
  // events. This function is not implemented on iOS and iOS WebView, and
  // should not be used on those platforms.
  virtual bool ShowTouchToFillIban(base::WeakPtr<TouchToFillDelegate> delegate,
                                   base::span<const Iban> ibans_to_suggest) = 0;

  // Shows the Touch To Fill surface for filling Wallet loyalty card
  // information, if possible, returning `true` on success. `delegate` will be
  // notified of events. This function is not implemented on iOS and iOS
  // WebView, and should not be used on those platforms.
  virtual bool ShowTouchToFillLoyaltyCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      std::vector<LoyaltyCard> loyalty_cards_to_suggest) = 0;

  // Updates the BNPL UI, returning true on success. This either:
  // 1. Updates the BNPL payment method option on the Touch To Fill surface, OR
  // 2. Updates the progress screen with the selection screen or error screen,
  // based on whether the extracted amount exists or not.
  // Should be called only on Android if the feature is supported by the
  // platform.
  virtual bool OnPurchaseAmountExtracted(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) = 0;

  // Shows the BNPL progress screen, if possible, returning `true` on success.
  // Should be called only on Android if the feature is supported by the
  // platform. `cancel_callback` will be run if the screen is dismissed by the
  // user. This function is not implemented on iOS and iOS WebView, and should
  // not be used on those platforms.
  virtual bool ShowTouchToFillProgress(base::OnceClosure cancel_callback) = 0;

  // Shows the Touch To Fill surface with BNPL issuer information, if possible,
  // returning `true` on success. `bnpl_issuer_contexts` provides a read-only
  // list of BNPL issuer contexts to be shown. `app_locale` provides the
  // application's current language and region code for localization.
  // `selected_issuer_callback` provides a one-time callback to be invoked when
  // an issuer is selected. `cancel_callback` provides a one-time callback to be
  // invoked to reset the BNPL flow. This function is not implemented on iOS
  // and iOS WebView, and should not be used on those platforms.
  virtual bool ShowTouchToFillBnplIssuers(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      const std::string& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) = 0;

  // Shows the Touch To Fill surface with terms for linking a new BNPL issuer,
  // if possible, returning `true` on success. This function is not implemented
  // on iOS and iOS WebView, and should not be used on those platforms.
  virtual bool ShowTouchToFillBnplTos(BnplTosModel bnpl_tos_model,
                                      base::OnceClosure accept_callback,
                                      base::OnceClosure cancel_callback) = 0;

  // Shows the BNPL error screen, if possible, returning `true` on success.
  // Should be called only on Android if the feature is supported by the
  // platform. `context` will decide what strings are displayed for the title
  // and description.
  virtual bool ShowTouchToFillError(
      const AutofillErrorDialogContext& context) = 0;

  // Hides the Touch To Fill surface for filling payment information if one is
  // currently shown. Should be called only if the feature is supported by the
  // platform.
  virtual void HideTouchToFillPaymentMethod() = 0;

  // Sets the Touch To Fill surface visibility to `visible`. Should be called
  // only if the feature is supported by the platform.
  virtual void SetTouchToFillVisible(bool visible) = 0;

  // Return the `PaymentsDataManager` which is payments-specific version of
  // PersonalDataManager. It has two main responsibilities:
  // - Caching the payments related data stored in `AutofillTable` for
  // synchronous retrieval.
  // - Posting changes to `AutofillTable` via the `AutofillWebDataService`
  //   and updating its state accordingly.
  virtual PaymentsDataManager& GetPaymentsDataManager() = 0;

  // Gets a const version of the PaymentsDataManager.
  const PaymentsDataManager& GetPaymentsDataManager() const;

#if !BUILDFLAG(IS_IOS)
  // Creates the appropriate implementation of InternalAuthenticator. May be
  // null for platforms that don't support this, in which case standard CVC
  // authentication will be used instead.
  virtual std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver) = 0;
#endif

  // Gets or creates a payments autofill mandatory re-auth manager. This will be
  // used to handle payments mandatory re-auth related flows.
  virtual payments::MandatoryReauthManager*
  GetOrCreatePaymentsMandatoryReauthManager() = 0;

  // Gets the payments Save and Fill manager owned by the client. This will be
  // used to handle the Save and Fill dialog.
  virtual payments::SaveAndFillManager* GetSaveAndFillManager() = 0;

  // Gets a const version of payments Save and Fill manager owned by the client.
  const payments::SaveAndFillManager* GetSaveAndFillManager() const;

  // Shows the local `Save and Fill` modal dialog.
  virtual void ShowCreditCardLocalSaveAndFillDialog(
      CardSaveAndFillDialogCallback callback) = 0;

  // Shows the upload `Save and Fill` modal dialog.
  virtual void ShowCreditCardUploadSaveAndFillDialog(
      const LegalMessageLines& legal_message_lines,
      CardSaveAndFillDialogCallback callback) = 0;

  // Shows a pending state dialog with a throbber while the preflight
  // response is being fetched. This pending state is a precursor to either the
  // local or upload Save and Fill dialog. If the preflight call fails, the
  // dialog transitions to the local version. If it succeeds, the dialog
  // transitions to the server version.
  virtual void ShowCreditCardSaveAndFillPendingDialog() = 0;

  // Hides the Save and Fill dialog upon receivng response from the CreateCard
  // server call.
  virtual void HideCreditCardSaveAndFillDialog() = 0;

  // Checks if the browser popup is a tab modal popup.
  virtual bool IsTabModalPopupDeprecated() const = 0;

  // Gets the `BnplStrategy` instance associated with the client. Helps
  // determines the next step in the BNPL flow depending on the platform.
  virtual BnplStrategy* GetBnplStrategy() = 0;

  // Gets the `BnplUiDelegate` instance associated with the client. Handles the
  // UI in the BNPL flow depending on the platform.
  virtual BnplUiDelegate* GetBnplUiDelegate() = 0;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
