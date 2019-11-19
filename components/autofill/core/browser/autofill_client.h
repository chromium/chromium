// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/security_state/core/security_state.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class RenderFrameHost;
}

namespace gfx {
class RectF;
}

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

namespace ukm {
class UkmRecorder;
}

namespace version_info {
enum class Channel;
}

namespace autofill {

class AddressNormalizer;
class AutocompleteHistoryManager;
class AutofillPopupDelegate;
class AutofillProfile;
class CardUnmaskDelegate;
class CreditCard;
class FormDataImporter;
class FormStructure;
class LogManager;
class MigratableCreditCard;
class PersonalDataManager;
class StrikeDatabase;
struct Suggestion;

namespace payments {
class PaymentsClient;
}

// A client interface that needs to be supplied to the Autofill component by the
// embedder.
//
// Each client instance is associated with a given context within which an
// AutofillManager is used (e.g. a single tab), so when we say "for the client"
// below, we mean "in the execution context the client is associated with" (e.g.
// for the tab the AutofillManager is attached to).
class AutofillClient : public RiskDataLoader {
 public:
  enum PaymentsRpcResult {
    // Empty result. Used for initializing variables and should generally
    // not be returned nor passed as arguments unless explicitly allowed by
    // the API.
    NONE,

    // Request succeeded.
    SUCCESS,

    // Request failed; try again.
    TRY_AGAIN_FAILURE,

    // Request failed; don't try again.
    PERMANENT_FAILURE,

    // Unable to connect to Payments servers. Prompt user to check internet
    // connection.
    NETWORK_ERROR,
  };

  enum SaveCardOfferUserDecision {
    // The user accepted credit card save.
    ACCEPTED,

    // The user explicitly declined credit card save.
    DECLINED,

    // The user ignored the credit card save prompt.
    IGNORED,
  };

  enum UnmaskCardReason {
    // The card is being unmasked for PaymentRequest.
    UNMASK_FOR_PAYMENT_REQUEST,

    // The card is being unmasked for Autofill.
    UNMASK_FOR_AUTOFILL,
  };

  // Authentication methods for card unmasking.
  enum UnmaskAuthMethod {
    UNKNOWN = 0,
    // Require user to unmask via CVC.
    CVC = 1,
    // Suggest use of FIDO authenticator for card unmasking.
    FIDO = 2,
  };

  // Details for card unmasking, such as the suggested method of authentication,
  // along with any information required to facilitate the authentication.
  struct UnmaskDetails {
    UnmaskDetails();
    ~UnmaskDetails();

    // The type of authentication method suggested for card unmask.
    UnmaskAuthMethod unmask_auth_method = UnmaskAuthMethod::UNKNOWN;
    // Set to true if the user should be offered opt-in for FIDO Authentication.
    bool offer_fido_opt_in = false;
    // Public Key Credential Request Options required for authentication.
    // https://www.w3.org/TR/webauthn/#dictdef-publickeycredentialrequestoptions
    base::Value fido_request_options;
    // Set of credit cards ids that are eligible for FIDO Authentication.
    std::set<std::string> fido_eligible_card_ids;
  };

  // Used for explicitly requesting the user to enter/confirm cardholder name,
  // expiration date month and year.
  struct UserProvidedCardDetails {
    base::string16 cardholder_name;
    base::string16 expiration_date_month;
    base::string16 expiration_date_year;
  };

  // Used for options of upload prompt.
  struct SaveCreditCardOptions {
    SaveCreditCardOptions& with_from_dynamic_change_form(bool b) {
      from_dynamic_change_form = b;
      return *this;
    }

    SaveCreditCardOptions& with_has_non_focusable_field(bool b) {
      has_non_focusable_field = b;
      return *this;
    }

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

    bool from_dynamic_change_form = false;
    bool has_non_focusable_field = false;
    bool should_request_name_from_user = false;
    bool should_request_expiration_date_from_user = false;
    bool show_prompt = false;
  };

  // Callback to run after local credit card save is offered. Sends whether the
  // prompt was accepted, declined, or ignored in |user_decision|.
  typedef base::OnceCallback<void(SaveCardOfferUserDecision user_decision)>
      LocalSaveCardPromptCallback;

  // Callback to run after upload credit card save is offered. Sends whether the
  // prompt was accepted, declined, or ignored in |user_decision|, and
  // additional |user_provided_card_details| if applicable.
  typedef base::OnceCallback<void(
      SaveCardOfferUserDecision user_decision,
      const UserProvidedCardDetails& user_provided_card_details)>
      UploadSaveCardPromptCallback;

  typedef base::Callback<void(const CreditCard&)> CreditCardScanCallback;

  // Callback to run if user presses the Save button in the migration dialog.
  // Will pass a vector of GUIDs of cards that the user selected to upload to
  // LocalCardMigrationManager.
  typedef base::OnceCallback<void(const std::vector<std::string>&)>
      LocalCardMigrationCallback;

  // Callback to run if the user presses the trash can button in the
  // action-required dialog. Will pass to LocalCardMigrationManager a
  // string of GUID of the card that the user selected to delete from local
  // storage.
  typedef base::RepeatingCallback<void(const std::string&)>
      MigrationDeleteCardCallback;

  // Callback to run if the OK button or the cancel button in the
  // WebauthnOfferDialog is clicked. Will pass to CreditCardFIDOAuthenticator a
  // bool indicating if offer was accepted or declined.
  typedef base::RepeatingCallback<void(bool)> WebauthnOfferDialogCallback;

  ~AutofillClient() override {}

  // Returns the channel for the installation. In branded builds, this will be
  // version_info::Channel::{STABLE,BETA,DEV,CANARY}. In unbranded builds, or
  // in branded builds when the channel cannot be determined, this will be
  // version_info::Channel::UNKNOWN.
  virtual version_info::Channel GetChannel() const;

  // Gets the PersonalDataManager instance associated with the client.
  virtual PersonalDataManager* GetPersonalDataManager() = 0;

  // Gets the AutocompleteHistoryManager instance associate with the client.
  virtual AutocompleteHistoryManager* GetAutocompleteHistoryManager() = 0;

  // Gets the preferences associated with the client.
  virtual PrefService* GetPrefs() = 0;

  // Gets the sync service associated with the client.
  virtual syncer::SyncService* GetSyncService() = 0;

  // Gets the IdentityManager associated with the client.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Gets the FormDataImporter instance owned by the client.
  virtual FormDataImporter* GetFormDataImporter() = 0;

  // Gets the payments::PaymentsClient instance owned by the client.
  virtual payments::PaymentsClient* GetPaymentsClient() = 0;

  // Gets the StrikeDatabase associated with the client.
  virtual StrikeDatabase* GetStrikeDatabase() = 0;

  // Gets the UKM service associated with this client (for metrics).
  virtual ukm::UkmRecorder* GetUkmRecorder() = 0;

  // Gets the UKM source id associated with this client (for metrics).
  virtual ukm::SourceId GetUkmSourceId() = 0;

  // Gets an AddressNormalizer instance (can be null).
  virtual AddressNormalizer* GetAddressNormalizer() = 0;

  // Gets the security level used for recording histograms for the current
  // context if possible, SECURITY_LEVEL_COUNT otherwise.
  virtual security_state::SecurityLevel GetSecurityLevelForUmaHistograms() = 0;

  // Returns the current best guess as to the page's display language.
  virtual std::string GetPageLanguage() const;

  // Causes the Autofill settings UI to be shown. If |show_credit_card_settings|
  // is true, will show the credit card specific subpage.
  virtual void ShowAutofillSettings(bool show_credit_card_settings) = 0;

  // A user has attempted to use a masked card. Prompt them for further
  // information to proceed.
  virtual void ShowUnmaskPrompt(const CreditCard& card,
                                UnmaskCardReason reason,
                                base::WeakPtr<CardUnmaskDelegate> delegate) = 0;
  virtual void OnUnmaskVerificationResult(PaymentsRpcResult result) = 0;

  // Runs |show_migration_dialog_closure| if the user accepts the card migration
  // offer. This causes the card migration dialog to be shown.
  virtual void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) = 0;

  // Shows a dialog with the given |legal_message_lines| and the |user_email|.
  // Runs |start_migrating_cards_callback| if the user would like the selected
  // cards in the |migratable_credit_cards| to be uploaded to cloud.
  virtual void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) = 0;

  // Will show a dialog containing a error message if |has_server_error|
  // is true, or the migration results for cards in
  // |migratable_credit_cards| otherwise. If migration succeeds the dialog will
  // contain a |tip_message|. |migratable_credit_cards| will be used when
  // constructing the dialog. The dialog is invoked when the migration process
  // is finished. Runs |delete_local_card_callback| if the user chose to delete
  // one invalid card from local storage.
  virtual void ShowLocalCardMigrationResults(
      const bool has_server_error,
      const base::string16& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback) = 0;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Will show a dialog indicating the card verification is in progress. It is
  // shown after verification starts only if the WebAuthn is enabled.
  // Implemented only on desktop.
  virtual void ShowVerifyPendingDialog(
      base::OnceClosure cancel_card_verification_callback) = 0;

  // Close the verify pending dialog once the card verificiation is completed or
  // verification falls back to CVC.
  virtual void CloseVerifyPendingDialog() = 0;
#endif

  // Will show a dialog offering the option to use device's platform
  // authenticator in the future instead of CVC to verify the card being
  // unmasked. Runs |callback| if the OK button or the cancel button in the
  // dialog is clicked. This is only implemented on desktop.
  virtual void ShowWebauthnOfferDialog(
      WebauthnOfferDialogCallback callback) = 0;

  // Will close the WebAuthn offer dialog. Returns true if dialog was visible
  // and has been closed. Implemented only on desktop.
  virtual bool CloseWebauthnOfferDialog();

  // Will update the WebAuthn offer dialog content to the error state.
  // Implemented only on desktop.
  virtual void UpdateWebauthnOfferDialogWithError() {}

  // Runs |callback| if the |profile| should be imported as personal data.
  virtual void ConfirmSaveAutofillProfile(const AutofillProfile& profile,
                                          base::OnceClosure callback) = 0;

  // Runs |callback| once the user makes a decision with respect to the
  // offer-to-save prompt. On desktop, shows the offer-to-save bubble if
  // |options.show_prompt| is true; otherwise only shows the
  // omnibox icon. On mobile, shows the offer-to-save infobar if
  // |options.show_prompt| is true; otherwise does not offer to
  // save at all.
  virtual void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      AutofillClient::SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) = 0;

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Display the cardholder name fix flow prompt and run the |callback| if
  // the card should be uploaded to payments with updated name from the user.
  virtual void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const base::string16&)> callback) = 0;

  // Display the expiration date fix flow prompt with the |card| details
  // and run the |callback| if the card should be uploaded to payments with
  // updated expiration date from the user.
  virtual void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const base::string16&, const base::string16&)>
          callback) = 0;
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  // Runs |callback| once the user makes a decision with respect to the
  // offer-to-save prompt. Displays the contents of |legal_message_lines|
  // to the user. Displays a cardholder name textfield in the bubble if
  // |options.should_request_name_from_user| is true. Displays
  // a pair of expiration date dropdowns in the bubble if
  // |should_request_expiration_date_from_user| is true. On desktop, shows the
  // offer-to-save bubble if |options.show_prompt| is true;
  // otherwise only shows the omnibox icon. On mobile, shows the offer-to-save
  // infobar if |options.show_prompt| is true; otherwise does
  // not offer to save at all.
  virtual void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) = 0;

  // Called after credit card upload is finished. Will show upload result to
  // users. |card_saved| indicates if the card is successfully saved.
  virtual void CreditCardUploadCompleted(bool card_saved) = 0;

  // Will show an infobar to get user consent for Credit Card assistive filling.
  // Will run |callback| on success.
  virtual void ConfirmCreditCardFillAssist(const CreditCard& card,
                                           base::OnceClosure callback) = 0;

  // Returns true if both the platform and the device support scanning credit
  // cards. Should be called before ScanCreditCard().
  virtual bool HasCreditCardScanFeature() = 0;

  // Shows the user interface for scanning a credit card. Invokes the |callback|
  // when a credit card is scanned successfully. Should be called only if
  // HasCreditCardScanFeature() returns true.
  virtual void ScanCreditCard(const CreditCardScanCallback& callback) = 0;

  // Shows an Autofill popup with the given |values|, |labels|, |icons|, and
  // |identifiers| for the element at |element_bounds|. |delegate| will be
  // notified of popup events.
  virtual void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<Suggestion>& suggestions,
      bool autoselect_first_suggestion,
      PopupType popup_type,
      base::WeakPtr<AutofillPopupDelegate> delegate) = 0;

  // Update the data list values shown by the Autofill popup, if visible.
  virtual void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) = 0;

  // Hide the Autofill popup if one is currently showing.
  virtual void HideAutofillPopup() = 0;

  // Whether the Autocomplete feature of Autofill should be enabled.
  virtual bool IsAutocompleteEnabled() = 0;

  // Pass the form structures to the password manager to choose correct username
  // and to the password generation manager to detect account creation forms.
  virtual void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) = 0;

  // Inform the client that the field has been filled.
  virtual void DidFillOrPreviewField(
      const base::string16& autofilled_value,
      const base::string16& profile_full_name) = 0;

  // If the context is secure.
  virtual bool IsContextSecure() = 0;

  // Whether it is appropriate to show a signin promo for this user.
  virtual bool ShouldShowSigninPromo() = 0;

  // Whether server side cards are supported by the client. If false, only
  // local cards will be shown.
  virtual bool AreServerCardsSupported() = 0;

  // Handles simple actions for the autofill popups.
  virtual void ExecuteCommand(int id) = 0;

  // Returns a LogManager instance. May be null for platforms that don't support
  // this.
  virtual LogManager* GetLogManager() const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_
