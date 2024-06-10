// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/security_state/core/security_state.h"
#include "components/translate/core/browser/language_state.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

class PrefService;

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

#if !BUILDFLAG(IS_IOS)
namespace webauthn {
class InternalAuthenticator;
}
#endif

namespace autofill {

class AddressNormalizer;
class AutocompleteHistoryManager;
class AutofillAblationStudy;
class AutofillComposeDelegate;
class AutofillCrowdsourcingManager;
class AutofillDriver;
class AutofillMlPredictionModelHandler;
class AutofillOfferManager;
class AutofillOptimizationGuide;
class AutofillSuggestionDelegate;
class AutofillPlusAddressDelegate;
class AutofillProfile;
class CreditCard;
enum class CreditCardFetchResult;
class FormDataImporter;
class Iban;
class LogManager;
class MerchantPromoCodeManager;
class PersonalDataManager;
class StrikeDatabase;
struct Suggestion;
class TouchToFillDelegate;
enum class WebauthnDialogState;

namespace payments {
class MandatoryReauthManager;
class PaymentsAutofillClient;
}

// Fills the focused field with the string passed to it.
using PlusAddressCallback = base::OnceCallback<void(const std::string&)>;

// A client interface that needs to be supplied to the Autofill component by the
// embedder.
//
// Each client instance is associated with a given context within which an
// BrowserAutofillManager is used (e.g. a single tab), so when we say "for the
// client" below, we mean "in the execution context the client is associated
// with" (e.g. for the tab the BrowserAutofillManager is attached to).
class AutofillClient {
 public:
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
  };

  // The type of the credit card the Payments RPC fetches.
  enum class PaymentsRpcCardType {
    // Unknown type.
    kUnknown = 0,
    // Server card.
    kServerCard = 1,
    // Virtual card.
    kVirtualCard = 2,
  };

  enum class SaveCardOfferUserDecision {
    // The user accepted credit card save.
    kAccepted,

    // The user explicitly declined credit card save.
    kDeclined,

    // The user ignored the credit card save prompt.
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

  // Represents the user's possible decisions or outcomes in response to a
  // prompt related to address saving, updating, or migrating.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AddressPromptUserDecision {
    kUndefined,
    // No prompt is shown and no decision is needed to proceed with the process.
    kUserNotAsked,
    // The user accepted the save/update/migration flow from the initial prompt.
    kAccepted,
    // The user declined the save/update/migration flow from the initial prompt.
    kDeclined,
    // The user accepted the save/update/migration flow from the edit dialog.
    kEditAccepted,
    // The user declined the save/update/migration flow from the edit dialog.
    kEditDeclined,
    // The user selected to never migrate a `kLocalOrSyncable` profile to the
    // account storage. Currently unused for new profile and update prompts, but
    // is triggered by explicitly declining a migration prompt.
    kNever,
    // The user ignored the prompt.
    kIgnored,
    // The save/update/migration message timed out before the user interacted.
    // This is only relevant on mobile.
    kMessageTimeout,
    // The user swipes away the save/update/migration message. This is only
    // relevant on mobile.
    kMessageDeclined,
    // The prompt is suppressed most likely because there is already another
    // prompt shown on the same tab.
    kAutoDeclined,
    kMaxValue = kAutoDeclined,
  };

  // Used for explicitly requesting the user to enter/confirm cardholder name,
  // expiration date month and year.
  struct UserProvidedCardDetails {
    std::u16string cardholder_name;
    std::u16string expiration_date_month;
    std::u16string expiration_date_year;
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

  // TODO(b/325440757): Remove after the save-update controller splitting is
  // done or remove this TODO if a new option is added.
  // Used for options of save (and update) address profile prompt.
  struct SaveAddressProfilePromptOptions {
    // Whether the prompt suggests migration into the user's account.
    bool is_migration_to_account = false;
  };

  // Required arguments to create a dropdown showing autofill suggestions.
  struct PopupOpenArgs {
    PopupOpenArgs();
    PopupOpenArgs(const gfx::RectF& element_bounds,
                  base::i18n::TextDirection text_direction,
                  std::vector<Suggestion> suggestions,
                  AutofillSuggestionTriggerSource trigger_source,
                  int32_t form_control_ax_id,
                  PopupAnchorType anchor_type);
    PopupOpenArgs(const PopupOpenArgs&);
    PopupOpenArgs(PopupOpenArgs&&);
    PopupOpenArgs& operator=(const PopupOpenArgs&);
    PopupOpenArgs& operator=(PopupOpenArgs&&);
    ~PopupOpenArgs();
    // TODO(b/340817507): Update this member name since bounds can now refer to
    // the caret bounds and elements gives the idea of HTML elements only.
    gfx::RectF element_bounds;
    base::i18n::TextDirection text_direction =
        base::i18n::TextDirection::UNKNOWN_DIRECTION;
    std::vector<Suggestion> suggestions;
    AutofillSuggestionTriggerSource trigger_source =
        AutofillSuggestionTriggerSource::kUnspecified;
    int32_t form_control_ax_id = 0;
    PopupAnchorType anchor_type = PopupAnchorType::kField;
  };

  // Describes the position of the Autofill popup on the screen.
  struct PopupScreenLocation {
    // The bounds of the popup in the screen coordinate system.
    gfx::Rect bounds;
    // Describes the position of the arrow on the popup's border and corresponds
    // to a subset of the available options in `views::BubbleBorder::Arrow`.
    enum class ArrowPosition {
      kTopRight,
      kTopLeft,
      kBottomRight,
      kBottomLeft,
      kLeftTop,
      kRightTop,
      kMax = kRightTop
    };
    ArrowPosition arrow_position;
  };

  // Callback to run after local credit card save or local CVC save is offered.
  // Sends whether the prompt was accepted, declined, or ignored in
  // |user_decision|.
  using LocalSaveCardPromptCallback =
      base::OnceCallback<void(SaveCardOfferUserDecision user_decision)>;

  // Callback to run after upload credit card save or upload CVC save for
  // existing server card is offered. Sends whether the prompt was accepted,
  // declined, or ignored in |user_decision|, and additional
  // |user_provided_card_details| if applicable.
  using UploadSaveCardPromptCallback = base::OnceCallback<void(
      SaveCardOfferUserDecision user_decision,
      const UserProvidedCardDetails& user_provided_card_details)>;

  using CreditCardScanCallback = base::OnceCallback<void(const CreditCard&)>;

  // Callback to run when the user makes a decision on whether to save the
  // profile. If the user edits the Autofill profile and then accepts edits, the
  // edited version of the profile should be passed as the second parameter. No
  // Autofill profile is passed otherwise.
  using AddressProfileSavePromptCallback =
      base::OnceCallback<void(AddressPromptUserDecision,
                              base::optional_ref<const AutofillProfile>)>;

  // The callback accepts the boolean parameter indicating whether the user has
  // accepted the delete dialog. The callback is intended to be called only upon
  // user closing the dialog directly and not when user closes the browser tab.
  using AddressProfileDeleteDialogCallback = base::OnceCallback<void(bool)>;

  virtual ~AutofillClient() = default;

  // Returns the channel for the installation. In branded builds, this will be
  // version_info::Channel::{STABLE,BETA,DEV,CANARY}. In unbranded builds, or
  // in branded builds when the channel cannot be determined, this will be
  // version_info::Channel::UNKNOWN.
  virtual version_info::Channel GetChannel() const;

  // Returns whether the user is currently operating in an incognito context.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the URL loader factory associated with this driver.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns the AutofillCrowdsourcingManager for communication with the
  // Autofill server.
  virtual AutofillCrowdsourcingManager* GetCrowdsourcingManager();

  // Gets the PersonalDataManager instance associated with the original Chrome
  // profile.
  // To distinguish between (non-)incognito mode when deciding to persist data,
  // use the client's `IsOffTheRecord()` function.
  virtual PersonalDataManager* GetPersonalDataManager() = 0;
  const PersonalDataManager* GetPersonalDataManager() const;

  // Gets the AutofillOptimizationGuide instance associated with the client.
  // This function can return nullptr if we are on an unsupported platform, or
  // if the AutofillOptimizationGuide's dependencies are not present.
  virtual AutofillOptimizationGuide* GetAutofillOptimizationGuide() const;

  // Gets the AutofillModelHandler instance for autofill machine learning
  // predictions associated with the client.
  virtual AutofillMlPredictionModelHandler*
  GetAutofillMlPredictionModelHandler();

  // Gets the AutocompleteHistoryManager instance associated with the client.
  virtual AutocompleteHistoryManager* GetAutocompleteHistoryManager() = 0;

  // Returns the `AutofillComposeDelegate` instance for the tab of this client.
  virtual AutofillComposeDelegate* GetComposeDelegate();

  // Returns the `AutofillPlusAddressDelegate` associated with the profile of
  // the window of this tab.
  virtual AutofillPlusAddressDelegate* GetPlusAddressDelegate();

  // Orchestrates UI for enterprise plus address creation; no-op except on
  // supported platforms.
  virtual void OfferPlusAddressCreation(const url::Origin& main_frame_origin,
                                        PlusAddressCallback callback);

  // Gets the MerchantPromoCodeManager instance associated with the
  // client (can be null for unsupported platforms).
  virtual MerchantPromoCodeManager* GetMerchantPromoCodeManager();

  // Gets the preferences associated with the client.
  virtual PrefService* GetPrefs() = 0;
  virtual const PrefService* GetPrefs() const = 0;

  // Gets the sync service associated with the client.
  virtual syncer::SyncService* GetSyncService() = 0;

  // Gets the IdentityManager associated with the client.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Gets the FormDataImporter instance owned by the client.
  virtual FormDataImporter* GetFormDataImporter() = 0;

  // Gets the payments::PaymentsAutofillClient instance owned by the client.
  virtual payments::PaymentsAutofillClient* GetPaymentsAutofillClient();

  // Gets the StrikeDatabase associated with the client. Note: Nullptr may be
  // returned so check before use.
  // TODO(crbug.com/40926442): Make sure all strike database usages check for
  // the nullptr.
  virtual StrikeDatabase* GetStrikeDatabase() = 0;

  // Gets the UKM service associated with this client (for metrics).
  virtual ukm::UkmRecorder* GetUkmRecorder() = 0;

  // Gets the UKM source id associated with this client (for metrics).
  virtual ukm::SourceId GetUkmSourceId() = 0;

  // Gets an AddressNormalizer instance (can be null).
  virtual AddressNormalizer* GetAddressNormalizer() = 0;

  // Gets an AutofillOfferManager instance (can be null for unsupported
  // platforms).
  virtual AutofillOfferManager* GetAutofillOfferManager();

  // Returns the last committed url of the primary main frame.
  virtual const GURL& GetLastCommittedPrimaryMainFrameURL() const = 0;

  // Returns the last committed origin of the primary main frame.
  virtual url::Origin GetLastCommittedPrimaryMainFrameOrigin() const = 0;

  // Gets the security level used for recording histograms for the current
  // context if possible, SECURITY_LEVEL_COUNT otherwise.
  virtual security_state::SecurityLevel GetSecurityLevelForUmaHistograms() = 0;

  // Returns the language state, if available.
  virtual const translate::LanguageState* GetLanguageState() = 0;

  // Returns the translate driver, if available, which is used to observe the
  // page language for language-dependent heuristics.
  virtual translate::TranslateDriver* GetTranslateDriver() = 0;

  // Retrieves the country code of the user from Chrome variation service.
  // If the variation service is not available, return an empty string.
  virtual GeoIpCountryCode GetVariationConfigCountryCode() const;

  // Returns the profile type of the session.
  virtual profile_metrics::BrowserProfileType GetProfileType() const;

  // Gets a FastCheckoutClient instance (can be null for unsupported platforms).
  virtual FastCheckoutClient* GetFastCheckoutClient();

#if !BUILDFLAG(IS_IOS)
  // Creates the appropriate implementation of InternalAuthenticator. May be
  // null for platforms that don't support this, in which case standard CVC
  // authentication will be used instead.
  virtual std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver);
#endif

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings(SuggestionType suggestion_type) = 0;

  // Gets or creates a payments autofill mandatory re-auth manager. This will be
  // used to handle payments mandatory re-auth related flows.
  virtual payments::MandatoryReauthManager*
  GetOrCreatePaymentsMandatoryReauthManager();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Display the expiration date fix flow prompt with the |card| details
  // and run the |callback| if the card should be uploaded to payments with
  // updated expiration date from the user.
  virtual void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Runs |callback| once the user makes a decision with respect to the
  // offer-to-save prompt. This includes both the save local card prompt and the
  // save CVC for a local card prompt. On desktop, shows the offer-to-save
  // bubble if |options.show_prompt| is true; otherwise only shows the omnibox
  // icon. On mobile, shows the offer-to-save infobar if |options.show_prompt|
  // is true; otherwise does not offer to save at all.
  virtual void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      AutofillClient::SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback);

  // Runs |callback| once the user makes a decision with respect to the
  // offer-to-save prompt. This includes both the save server card prompt and
  // the save CVC for a server card prompt. Displays the contents of
  // |legal_message_lines| to the user. Displays a cardholder name textfield in
  // the bubble if |options.should_request_name_from_user| is true. Displays a
  // pair of expiration date dropdowns in the bubble if
  // |should_request_expiration_date_from_user| is true. On desktop, shows the
  // offer-to-save bubble if |options.show_prompt| is true;
  // otherwise only shows the omnibox icon. On mobile, shows the offer-to-save
  // infobar if |options.show_prompt| is true; otherwise does
  // not offer to save at all.
  // TODO (crbug.com/1462821): Make |legal_message_lines| optional, as CVC
  // upload has no legal message.
  virtual void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback);

  // Show an edit address profile dialog, giving the user an option to alter
  // autofill profile data. `on_user_decision_callback` is used to react to the
  // user decision of either saving changes or not.
  virtual void ShowEditAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) = 0;

  // Show a delete address profile dialog asking if users want to proceed with
  // deletion.
  virtual void ShowDeleteAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) = 0;

  // Shows the offer-to-save (or update) address profile bubble. If
  // `original_profile` is nullptr, this renders a save prompt. Otherwise, it
  // renders an update prompt where `original_profile` is the address profile
  // that will be updated if the user accepts the update prompt. Runs `callback`
  // once the user makes a decision with respect to the offer-to-save prompt.
  // `options` carries extra configuration options for the prompt.
  virtual void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) = 0;

  // Returns true if both the platform and the device support scanning credit
  // cards. Should be called before ScanCreditCard().
  virtual bool HasCreditCardScanFeature() const = 0;

  // Shows the user interface for scanning a credit card. Invokes the |callback|
  // when a credit card is scanned successfully. Should be called only if
  // HasCreditCardScanFeature() returns true.
  virtual void ScanCreditCard(CreditCardScanCallback callback) = 0;

  // Shows the Touch To Fill surface for filling credit card information, if
  // possible, and returns |true| on success. |delegate| will be notified of
  // events. `card_acceptabilies` is a boolean list denoting if the virtual
  // card in `cards_to_suggest` is acceptable on the merchant's platform.
  // Should be called only if the feature is supported by the platform.
  virtual bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest,
      const std::vector<bool>& card_acceptabilies) = 0;

  // Shows the Touch To Fill surface for filling IBAN information, if
  // possible, returning `true` on success. `delegate` will be notified of
  // events. This function is not implemented on iOS and iOS WebView, and
  // should not be used on those platforms.
  virtual bool ShowTouchToFillIban(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::Iban> ibans_to_suggest);

  // Hides the Touch To Fill surface for filling credit card information
  // if one is currently shown. Should be called only if the feature is
  // supported by the platform.
  virtual void HideTouchToFillCreditCard() = 0;

  // Shows Autofill suggestions with the given `values`, `labels`, `icons`, and
  // `identifiers` for the element at `element_bounds`. `delegate` will be
  // notified of suggestion events, e.g., the user accepting a suggestion.
  // The suggestions are shown asynchronously on Desktop and Android.
  virtual void ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) = 0;

  // Update the data list values shown by the Autofill suggestions, if visible.
  virtual void UpdateAutofillDataListValues(
      base::span<const SelectOption> datalist) = 0;

  // Informs the client that the suggestion UI needs to be kept alive. Call
  // before |UpdatePopup| to update the open popup in-place.
  virtual void PinAutofillSuggestions() = 0;

  // Returns the information of the popup on the screen, if there is one that is
  // showing. Note that this implemented only on Desktop.
  virtual std::optional<PopupScreenLocation> GetPopupScreenLocation() const;

  // Returns (not elided) suggestions currently held by the UI.
  virtual base::span<const Suggestion> GetAutofillSuggestions() const;

  // Updates the popup contents with the newly given suggestions.
  // `trigger_source` indicates the reason for updating the popup. (However, the
  // password manager makes no distinction).
  virtual void UpdatePopup(const std::vector<Suggestion>& suggestions,
                           FillingProduct main_filling_product,
                           AutofillSuggestionTriggerSource trigger_source) = 0;

  // Hides the Autofill suggestions UI if it is currently showing.
  virtual void HideAutofillSuggestions(SuggestionHidingReason reason) = 0;

  // Maybe triggers a hats survey that measures the user's perception of
  // Autofill. When triggering happens, the survey dialog will be displayed with
  // a 5s delay. Note:  This survey should be triggered after form submissions.
  // `field_filling_stats_data` contains a key-value string representation of
  // `autofill_metrics::FormGroupFillingStats`. See
  // chrome/browser/ui/hats/survey_config.cc for details on what values should
  // be present.
  // `filling_product` defines whether an address or payments survey will be
  // displayed.
  virtual void TriggerUserPerceptionOfAutofillSurvey(
      FillingProduct filling_product,
      const std::map<std::string, std::string>& field_filling_stats_data);

  // Whether the Autocomplete feature of Autofill should be enabled.
  virtual bool IsAutocompleteEnabled() const = 0;

  // Returns whether password management is enabled as per the user preferences.
  virtual bool IsPasswordManagerEnabled() = 0;

  // Inform the client that the form has been filled.
  virtual void DidFillOrPreviewForm(mojom::ActionPersistence action_persistence,
                                    AutofillTriggerSource trigger_source,
                                    bool is_refill) = 0;

  // Inform the client that the field has been filled.
  virtual void DidFillOrPreviewField(
      const std::u16string& autofilled_value,
      const std::u16string& profile_full_name) = 0;

  // If the context is secure.
  virtual bool IsContextSecure() const = 0;

  // Returns a LogManager instance. May be null for platforms that don't support
  // this.
  virtual LogManager* GetLogManager() const;

  virtual const AutofillAblationStudy& GetAblationStudy() const;

#if BUILDFLAG(IS_IOS)
  // Checks whether `field_id` is the last field that for which
  // AutofillAgent::queryAutofillForForm() was called. See crbug.com/1097015.
  virtual bool IsLastQueriedField(FieldGlobalId field_id) = 0;
#endif

  // Whether we can add more information to the contents of suggestions text due
  // to the use of a large keyboard accessory view. See b/40942168.
  virtual bool ShouldFormatForLargeKeyboardAccessory() const;

  // Navigates to |url| in a new tab. |url| links to the promo code offer
  // details page for the offers in a promo code suggestions popup. Every offer
  // in a promo code suggestions popup links to the same offer details page.
  virtual void OpenPromoCodeOfferDetailsURL(const GURL& url);

  // Updates and returns the current form interactions flow id. This is used as
  // an approximation for keeping track of the number of user interactions with
  // related forms for logging. Example implementation: the flow id is set to a
  // GUID on the first call. That same GUID will be returned for consecutive
  // calls in the next 20 minutes. Afterwards a new GUID is set and the pattern
  // repeated.
  virtual FormInteractionsFlowId GetCurrentFormInteractionsFlowId() = 0;

  // Returns a pointer to a DeviceAuthenticator. Might be nullptr if the given
  // platform is not supported.
  virtual std::unique_ptr<device_reauth::DeviceAuthenticator>
  GetDeviceAuthenticator();

  // Attaches the IPH for the manual fallback feature to the `field`, on
  // platforms that support manual fallback.
  virtual void ShowAutofillFieldIphForManualFallbackFeature(
      const FormFieldData& field);

  // Hides the IPH for the manual fallback feature.
  virtual void HideAutofillFieldIphForManualFallbackFeature();

  // Notifies the IPH code that the manual fallback feature was used.
  virtual void NotifyAutofillManualFallbackUsed();

  // Stores test addresses provided by devtools and used to help developers
  // debug their forms with a list of well formatted addresses. Differently from
  // other `AutofillProfile`s/addresses, this list is stored in the client,
  // instead of the `PersonalDataManager`.
  virtual void set_test_addresses(std::vector<AutofillProfile> test_addresses);

  virtual base::span<const AutofillProfile> GetTestAddresses() const;

  // `PasswordFormType` describes the different outcomes of Password Manager's
  // form parsing heuristics (see `FormDataParser`). Note that these are all
  // predictions and may be inaccurate.
  enum class PasswordFormType {
    // The form is not password-related.
    kNoPasswordForm = 0,
    // The form is a predicted to be a login form, i.e. it has a username and a
    // password field.
    kLoginForm = 1,
    // The form is predicted to be a signup form, i.e. it has a username field
    // and a new password field.
    kSignupForm = 2,
    // The form is predicted to be a change password form, i.e. it has a current
    // password field and a new password field.
    kChangePasswordForm = 3,
    // The form is predicted to be a reset password form, i.e. it has a new
    // password field.
    kResetPasswordForm = 4,
    // The form is predicted to be the username form of a username-first flow,
    // i.e. there is only a username field.
    kSingleUsernameForm = 5
  };
  // Returns the heuristics predictions for the renderer form to which
  // `field_id` belongs inside the form with `form_id`. The browser form with
  // `form_id` is decomposed into renderer forms prior to running Password
  // Manager heuristics.
  // If the form cannot be found, `kNoPasswordForm` is returned.
  virtual PasswordFormType ClassifyAsPasswordForm(AutofillManager& manager,
                                                  FormGlobalId form_id,
                                                  FieldGlobalId field_id) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_
