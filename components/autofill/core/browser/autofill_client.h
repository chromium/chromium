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
#include "base/types/id_type.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/password_form_classification.h"
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

namespace optimization_guide::proto {
class UserAnnotationsEntry;
}

namespace version_info {
enum class Channel;
}

namespace autofill {

class AddressNormalizer;
class AutocompleteHistoryManager;
class AutofillAblationStudy;
class AutofillComposeDelegate;
class AutofillCrowdsourcingManager;
class AutofillDriverFactory;
class AutofillMlPredictionModelHandler;
class AutofillOptimizationGuide;
class AutofillSuggestionDelegate;
class AutofillPlusAddressDelegate;
class AutofillPredictionImprovementsDelegate;
class AutofillProfile;
class FormDataImporter;
class LogManager;
class PersonalDataManager;
class StrikeDatabase;
struct Suggestion;
enum class WebauthnDialogState;

namespace payments {
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

  // Describes the types of Iph shown by Autofill and anchored to a field.
  enum class IphFeature {
    kManualFallback,
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
    // TODO(crbug.com/340817507): Update this member name since bounds can now
    // refer to the caret bounds and elements gives the idea of HTML elements
    // only.
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

  // Callback to run when the user decides to undo the plus address full form
  // fulling. If the user never undoes the operation, the callback is never
  // triggered.
  using EmailOverrideUndoCallback = base::OnceClosure;

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

  // Returns the AutofillDriverFactory.
  virtual AutofillDriverFactory& GetAutofillDriverFactory() = 0;

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

  // Returns the `AutofillPredictionImprovementsDelegate` instance for
  // the tab of this client. This method can return nullptr if the user does not
  // have the feature available, either because of not being part of the
  // experiment or because of the current platform (prediction improvements are
  // only available in Desktop).
  virtual AutofillPredictionImprovementsDelegate*
  GetAutofillPredictionImprovementsDelegate();

  // Returns the `AutofillPlusAddressDelegate` associated with the profile of
  // the window of this tab.
  virtual AutofillPlusAddressDelegate* GetPlusAddressDelegate();

  // TODO(crbug.com/365494310): Move these methods to a plus-address-specific
  // client class.

  // Orchestrates UI for enterprise plus address creation; no-op
  // except on supported platforms.
  virtual void OfferPlusAddressCreation(const url::Origin& main_frame_origin,
                                        PlusAddressCallback callback);

  enum class PlusAddressErrorDialogType {
    kGenericError,
    // The quota for plus address creation is exhausted (account-wide or
    // site-specific).
    kQuotaExhausted,
    // The network request timed out.
    kTimeout,
  };
  // Shows UI to inform the user about a plus address error (apart from
  // affiliation errors).
  virtual void ShowPlusAddressError(
      PlusAddressErrorDialogType error_dialog_type,
      base::OnceClosure on_accepted);

  // Shows UI to inform the user about a plus address affiliation error.
  virtual void ShowPlusAddressAffiliationError(
      std::u16string affiliated_domain,
      std::u16string affiliated_plus_address,
      base::OnceClosure on_accepted);

  // Gets the preferences associated with the client.
  virtual PrefService* GetPrefs() = 0;
  virtual const PrefService* GetPrefs() const = 0;

  // Gets the sync service associated with the client.
  virtual syncer::SyncService* GetSyncService() = 0;

  // Gets the IdentityManager associated with the client.
  virtual signin::IdentityManager* GetIdentityManager() = 0;
  virtual const signin::IdentityManager* GetIdentityManager() const = 0;

  // Gets the FormDataImporter instance owned by the client.
  virtual FormDataImporter* GetFormDataImporter() = 0;

  // Gets the payments::PaymentsAutofillClient implementation owned by `this`.
  // On platforms where there exists a payments::PaymentsAutofillClient, the
  // instance that is returned is an existing payments::PaymentsAutofillClient
  // that was created upon the AutofillClient implementation's creation. If no
  // payments::PaymentsAutofillClient exists for a given platform, these
  // functions will return nullptr.
  virtual payments::PaymentsAutofillClient* GetPaymentsAutofillClient();
  const payments::PaymentsAutofillClient* GetPaymentsAutofillClient() const;

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

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings(SuggestionType suggestion_type) = 0;

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
  // `is_migration_to_account` differentiates saving `profile` in browser or
  // in user's Google account.
  virtual void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AddressProfileSavePromptCallback callback) = 0;

  // A unique identifier for suggestions UI (i.e. the keyboard accessory on
  // mobile and the popup on Desktop). Calling `ShowAutofillSuggestions`
  // generates a new identifier, but calling `UpdateAutofillSuggestions` does
  // not. Therefore the identifier can be used to decide whether to update or
  // close suggestions UI in asynchronous execution flows. There is at most one
  // suggestion UI showing at a time.
  using SuggestionUiSessionId =
      base::IdTypeU32<struct SuggestionUiSessionIdTag>;

  // Shows Autofill suggestions with the given `values`, `labels`, `icons`, and
  // `identifiers` for the element at `element_bounds`. `delegate` will be
  // notified of suggestion events, e.g., the user accepting a suggestion.
  // Note that suggestions are shown asynchronously on Desktop and Android. As a
  // result, calling `GetSessionIdForCurrentAutofillSuggestions` directly after
  // this method will return not return the same identifier, since the UI is not
  // showing yet.
  // `SuggestionUiSessionId` is only implemented on Chrome for Desktop and
  // Android. On other platforms, the returned identifier is meaningless.
  virtual SuggestionUiSessionId ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) = 0;

  // Notifies the user via a patform specific UI that full form filling for plus
  // addresses has occurred (i.e. the filled email address was overridden by the
  // plus address). The UI provides the user with the option to undo the
  // filling operation back to back to `original_email`, in which case the
  // `email_override_undo_callback` is triggered.
  virtual void ShowPlusAddressEmailOverrideNotification(
      const std::string& original_email,
      EmailOverrideUndoCallback email_override_undo_callback);

  // Update the data list values shown by the Autofill suggestions, if visible.
  virtual void UpdateAutofillDataListValues(
      base::span<const SelectOption> datalist) = 0;

  // Informs the client that the suggestion UI needs to be kept alive. Call
  // before `UpdateAutofillSuggestions` to update the open popup in-place.
  virtual void PinAutofillSuggestions() = 0;

  // Returns the information of the popup on the screen, if there is one that is
  // showing. Note that this implemented only on Desktop.
  virtual std::optional<PopupScreenLocation> GetPopupScreenLocation() const;

  // Returns the identifier of the suggestion UI that is currently showing or
  // `std::nullopt` is there is none.
  virtual std::optional<SuggestionUiSessionId>
  GetSessionIdForCurrentAutofillSuggestions() const;

  // Returns (not elided) suggestions currently held by the UI.
  virtual base::span<const Suggestion> GetAutofillSuggestions() const;

  // Updates the shown Autofill suggestions. `trigger_source` indicates the
  // reason for updating the popup. (However, the password manager makes no
  // distinction).
  virtual void UpdateAutofillSuggestions(
      const std::vector<Suggestion>& suggestions,
      FillingProduct main_filling_product,
      AutofillSuggestionTriggerSource trigger_source);

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
  virtual void ShowAutofillFieldIphForFeature(
      const FormFieldData& field,
      AutofillClient::IphFeature feature);

  // Hides the IPH for the manual fallback feature.
  virtual void HideAutofillFieldIph();

  // Notifies the IPH code that the manual fallback feature was used.
  virtual void NotifyAutofillManualFallbackUsed();

  // Shows a bubble asking whether the user wants to save prediction
  // improvements data.
  virtual void ShowSaveAutofillPredictionImprovementsBubble(
      const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
          to_be_upserted_entries,
      base::OnceCallback<void(bool prompt_was_accepted)>
          prompt_acceptance_callback);

  // Stores test addresses provided by devtools and used to help developers
  // debug their forms with a list of well formatted addresses. Differently from
  // other `AutofillProfile`s/addresses, this list is stored in the client,
  // instead of the `PersonalDataManager`.
  virtual void set_test_addresses(std::vector<AutofillProfile> test_addresses);

  virtual base::span<const AutofillProfile> GetTestAddresses() const;

  // Returns the heuristics predictions for the renderer form to which
  // `field_id` belongs inside the form with `form_id`. The browser form with
  // `form_id` is decomposed into renderer forms prior to running Password
  // Manager heuristics.
  // If the form cannot be found, `PasswordFormClassification::kNoPasswordForm`
  // is returned.
  virtual PasswordFormClassification ClassifyAsPasswordForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId field_id) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_
