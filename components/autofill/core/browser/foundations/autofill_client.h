// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/types/id_type.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/security_state/core/security_state.h"
#include "ui/gfx/geometry/rect_f.h"

class GoogleGroupsManager;
class GURL;
class PrefService;

namespace device_reauth {
class DeviceAuthenticator;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace one_time_tokens {
class OneTimeTokenService;
}  // namespace one_time_tokens

namespace optimization_guide {
class ModelQualityLogsUploaderService;
class RemoteModelExecutor;
}  // namespace optimization_guide

namespace optimization_guide::proto {
class AnnotatedPageContent;
}

namespace plus_addresses::hats {
enum class SurveyType;
}

namespace signin {
class IdentityManager;
}

namespace strike_database {
class StrikeDatabase;
}

namespace syncer {
class SyncService;
}

namespace translate {
class LanguageState;
class TranslateDriver;
}  // namespace translate

namespace ukm {
class UkmRecorder;
}

namespace url {
class Origin;
}

namespace version_info {
enum class Channel;
}

namespace autofill {

class AutofillManager;
class AddressNormalizer;
class AutocompleteHistoryManager;
class AutofillAblationStudy;
class AutofillPlusAddressDelegate;
class AutofillAiManager;
class AutofillAiModelCache;
class AutofillAiModelExecutor;
class AutofillComposeDelegate;
class AutofillCrowdsourcingManager;
class AutofillDriverFactory;
class AutofillOptimizationGuideDecider;
class AutofillProfile;
#if BUILDFLAG(IS_ANDROID)
class AutofillSnackbarControllerImpl;
#endif  // BUILDFLAG(IS_ANDROID)
class AutofillSuggestionDelegate;
enum class AutofillTriggerSource;
class IdentityCredentialDelegate;
class EntityDataManager;
class FastCheckoutClient;
class FieldClassificationModelHandler;
enum class FillingProduct;
class FormDataImporter;
class FormFieldData;
struct FormInteractionsFlowId;
class LogManager;
class OtpFieldDetector;
class OtpPhishGuardDelegate;
struct PasswordFormClassification;
class PasswordManagerDelegate;
class PersonalDataManager;
struct SelectOption;
struct Suggestion;
enum class SuggestionHidingReason;
enum class SuggestionType;
class SingleFieldFillRouter;
class ValuablesDataManager;
class VotesUploader;
class PasswordManagerAutofillHelperDelegate;

namespace autofill_metrics {
class FormInteractionsUkmLogger;
}

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
    kUndefined = 0,
    // No prompt is shown and no decision is needed to proceed with the process.
    kUserNotAsked = 1,
    // The user accepted the save/update/migration flow from the initial prompt.
    kAccepted = 2,
    // The user declined the save/update/migration flow from the initial prompt.
    kDeclined = 3,
    // The user accepted the save/update/migration flow from the edit dialog.
    kEditAccepted = 4,
    // The user declined the save/update/migration flow from the edit dialog.
    kEditDeclined = 5,
    // The user selected to never migrate a `kLocalOrSyncable` profile to the
    // account storage. Currently unused for new profile and update prompts, but
    // is triggered by explicitly declining a migration prompt.
    kNever = 6,
    // The user ignored the prompt.
    kIgnored = 7,
    // The save/update/migration message timed out before the user interacted.
    // This is only relevant on mobile.
    kMessageTimeout = 8,
    // The user swipes away the save/update/migration message. This is only
    // relevant on mobile.
    kMessageDeclined = 9,
    // The prompt is suppressed most likely because there is already another
    // prompt shown on the same tab.
    kAutoDeclined = 10,
    kMaxValue = kAutoDeclined,
  };

  // Represents the user's possible decisions or outcomes in response to a
  // prompt related to AutofillAi saving, updating, or migrating.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AutofillAiBubbleClosedReason {
    // Bubble closed reason not specified.
    kUnknown = 0,
    // The user explicitly accepted the bubble.
    kAccepted = 1,
    // The user explicitly cancelled the bubble.
    kCancelled = 2,
    // The user explicitly closed the bubble (via the close button or the ESC).
    kClosed = 3,
    // The bubble was not interacted with.
    kNotInteracted = 4,
    // The bubble lost focus and was closed.
    kLostFocus = 5,
    kMaxValue = kLostFocus
  };

  // Describes the types of Iph shown by Autofill and anchored to a field.
  enum class IphFeature {
    kAutofillAi,
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
  using EntityImportPromptResultCallback =
      base::OnceCallback<void(AutofillAiBubbleClosedReason close_reason)>;

  // The types of prompts that AutofillAi can show to the user after a form
  // submission. The values are ordered by decreasing priority of being shown
  // vis-a-vis each other.
  enum class AutofillAiImportPromptType {
    kSave = 0,
    kUpdate = 1,
    kMigrate = 2,
    kMaxValue = kMigrate
  };

  // Specifies the type of the address save prompt.
  enum class SaveAddressBubbleType {
    // The standard "Save address" bubble.
    kSave = 0,
    // An altered save bubble, that offers migrating a profile to the Google
    // Account.
    kMigrateToAccount = 1,
    // A bubble offering to merge the `kAccountNameEmail` and
    // `kAccountHome/kAccountName` profiles into a single profile.
    kHomeWorkNameEmailMerge = 2,
    kMaxValue = kHomeWorkNameEmailMerge
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

  virtual base::WeakPtr<AutofillClient> GetWeakPtr() = 0;

  // Returns the app locale, e.g., "en-US".
  virtual const std::string& GetAppLocale() const = 0;

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

  // Returns the VotesUploader.
  virtual VotesUploader& GetVotesUploader() = 0;

  // Returns the AutofillCrowdsourcingManager for communication with the
  // Autofill server.
  virtual AutofillCrowdsourcingManager& GetCrowdsourcingManager() = 0;

  // Returns whether the client has a PersonalDataManager.
  //
  // TODO(crbug.cm/455121491) This is a temporary fix to avoid crashes when
  // AutofillAnnotationsProviderImpl::AddAutofillInformation tries to query
  // autofillable data but deals with an AndroidAutofillClient.
  virtual bool HasPersonalDataManager() const;

  // Gets the PersonalDataManager instance associated with the original Chrome
  // profile.
  // To distinguish between (non-)incognito mode when deciding to persist data,
  // use the client's `IsOffTheRecord()` function.
  virtual PersonalDataManager& GetPersonalDataManager() = 0;
  const PersonalDataManager& GetPersonalDataManager() const;

  // Gets the ValuablesDataManager instance associated with the profile.
  virtual ValuablesDataManager* GetValuablesDataManager() = 0;
  const ValuablesDataManager* GetValuablesDataManager() const;

  // Gets the EntityDataManager instance associated with the client, if there is
  // one.
  virtual EntityDataManager* GetEntityDataManager() = 0;
  const EntityDataManager* GetEntityDataManager() const;

  // Gets the AutofillOptimizationGuideDecider instance associated with the
  // client. This function can return nullptr if we are on an unsupported
  // platform, or if the AutofillOptimizationGuideDecider's dependencies are not
  // present.
  virtual AutofillOptimizationGuideDecider*
  GetAutofillOptimizationGuideDecider() const;

  // Gets the FieldClassificationModelHandler instance for autofill machine
  // learning predictions associated with the client.
  virtual FieldClassificationModelHandler*
  GetAutofillFieldClassificationModelHandler();

  // Gets the FieldClassificationModelHandler instance for password manager
  // machine learning predictions associated with the client.
  virtual FieldClassificationModelHandler*
  GetPasswordManagerFieldClassificationModelHandler();

  // Handles routing single-field form filling requests, such as for
  // Autocomplete and merchant promo codes.
  virtual SingleFieldFillRouter& GetSingleFieldFillRouter() = 0;

  // Gets the AutocompleteHistoryManager instance associated with the client.
  virtual AutocompleteHistoryManager* GetAutocompleteHistoryManager() = 0;

  // Returns the `AutofillComposeDelegate` instance for the tab of this client.
  virtual AutofillComposeDelegate* GetComposeDelegate();

  // Attempts to the annotated page content for the current tab and calls
  // `callback` with the results.
  using GetAiPageContentCallback = base::OnceCallback<void(
      std::optional<optimization_guide::proto::AnnotatedPageContent>)>;
  virtual void GetAiPageContent(GetAiPageContentCallback callback);

  // Returns the `AutofillAiManager` instance for the tab of this client.
  // Returns `nullptr` if, at the time of the AutofillClient's construction, the
  // Autofill AI feature is unsupported.
  virtual AutofillAiManager* GetAutofillAiManager();

  // Returns the per-profile `AutofillAiModelCache`. Returns `nullptr` if the
  // `kAutofillAiServerModel` is not enabled.
  virtual AutofillAiModelCache* GetAutofillAiModelCache();

  // Returns the per-profile `AutofillAiModelExecutor`. Returns `nullptr` if the
  // `kAutofillAiServerModel` is not enabled or the profile is OTR.
  virtual AutofillAiModelExecutor* GetAutofillAiModelExecutor();

  // Returns the per-profile `RemoteModelExecutor`.
  virtual optimization_guide::RemoteModelExecutor* GetRemoteModelExecutor();

  // Returns nullptr if no identity credential conditional request was made
  // before.
  const IdentityCredentialDelegate* GetIdentityCredentialDelegate() const {
    return const_cast<const IdentityCredentialDelegate*>(
        const_cast<AutofillClient*>(this)->GetIdentityCredentialDelegate());
  }

  virtual IdentityCredentialDelegate* GetIdentityCredentialDelegate();

  // Returns the `AutofillPlusAddressDelegate` associated with the profile of
  // the window of this tab.
  virtual AutofillPlusAddressDelegate* GetPlusAddressDelegate();

  // Returns the `PasswordManagerDelegate` responsible to provide
  // password suggestions for the given `field_id`.
  virtual PasswordManagerDelegate* GetPasswordManagerDelegate(
      const FieldGlobalId& field_id);

  // Gets the preferences associated with the client.
  virtual PrefService* GetPrefs() = 0;
  virtual const PrefService* GetPrefs() const = 0;

  // Gets the sync service associated with the client.
  virtual syncer::SyncService* GetSyncService() = 0;
  const syncer::SyncService* GetSyncService() const;

  // Gets the IdentityManager associated with the client.
  virtual signin::IdentityManager* GetIdentityManager() = 0;
  virtual const signin::IdentityManager* GetIdentityManager() const = 0;

  // Gets the `GoogleGroupsManager` associated with the client.
  virtual const GoogleGroupsManager* GetGoogleGroupsManager() const;

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
  virtual strike_database::StrikeDatabase* GetStrikeDatabase() = 0;

  // Gets the UKM service associated with this client (for metrics).
  virtual ukm::UkmRecorder* GetUkmRecorder() = 0;

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

  // Shows the offer-to-save (or update) address profile bubble. If
  // `original_profile` is nullptr, this renders a save prompt. Otherwise, it
  // renders an update prompt where `original_profile` is the address profile
  // that will be updated if the user accepts the update prompt. Runs `callback`
  // once the user makes a decision with respect to the offer-to-save prompt.
  // `save_address_bubble_type` differentiates saving `profile` in browser or
  // in user's Google account.
  virtual void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveAddressBubbleType save_address_bubble_type,
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

  // Triggers a survey to ask the user why they declined saving an address.
  virtual void TriggerDeclinedSaveAddressReasonSurvey();

  // Triggers a survey after the user sees an Autofill AI suggestion and submits
  // a form. The triggering happens only if the uses sees an Autofill AI
  // suggestion, regardless of whether they accepted it or not.
  // `suggestion_accepted` defines whether the suggestion seen by the user was
  // accepted. `entity_type` defines the type of entity used to generate the
  // suggestion.
  virtual void TriggerAutofillAiFillingJourneySurvey(
      bool suggestion_accepted,
      EntityType entity_type,
      const base::flat_set<EntityTypeName>& saved_entities,
      const FieldTypeSet& triggering_field_types);

  // Triggers a survey after the user sees an Autofill AI save prompt.
  virtual void TriggerAutofillAiSavePromptSurvey(
      bool prompt_accepted,
      EntityType entity_type,
      const base::flat_set<EntityTypeName>& saved_entities);

  // Returns whether there is an active actor task for this client's tab (if
  // one exists).
  virtual bool IsActorTaskActive() const;

  // Returns true if either Profile or CreditCard Autofill is enabled.
  virtual bool IsAutofillEnabled() const = 0;

  // Returns true if the value of the AutofillProfileEnabled pref is true and
  // the client supports Autofill.
  virtual bool IsAutofillProfileEnabled() const = 0;

  // Whether the Autocomplete feature of Autofill should be enabled.
  virtual bool IsAutocompleteEnabled() const = 0;

  // Returns whether password management is enabled as per the user preferences.
  virtual bool IsPasswordManagerEnabled() const = 0;

  // Inform the client that the form has been filled.
  virtual void DidFillForm(AutofillTriggerSource trigger_source,
                           bool is_refill) = 0;

  // If the context is secure.
  virtual bool IsContextSecure() const = 0;

  // Returns whether Google Wallet storage is supported.
  virtual bool IsWalletStorageEnabled() const = 0;

  // Returns true if the client supports saving CVCs. This allows specific
  // clients (IosWebView) to opt out of the CVC saving feature.
  virtual bool IsCvcSavingSupported() const;

  // Returns true if all the conditions for enabling the upload of credit card
  // are satisfied.
  virtual bool IsCreditCardUploadEnabled() const;

  // Returns a LogManager instance (for chrome://autofill-internals). Note that
  // the return value may change over the lifetime of an AutofillClient from
  // null to non-null, so callers should not store the result of this function,
  // but call GetCurrentLogManager() again instead.
  // - May return null if logging is disabled (but a non null return value does
  // not guarantee that logging is enabled).
  // - May return null for platforms that don't support this.
  virtual LogManager* GetCurrentLogManager();

  virtual autofill_metrics::FormInteractionsUkmLogger&
  GetFormInteractionsUkmLogger() = 0;

  virtual const AutofillAblationStudy& GetAblationStudy() const;

#if BUILDFLAG(IS_ANDROID)
  // The AutofillSnackbarController is used to show a snackbar notification
  // on Android.
  virtual AutofillSnackbarControllerImpl* GetAutofillSnackbarController();
#endif

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

  // Attaches the IPH for `feature` to the `field`, on
  // platforms that it. If another IPH has been shown for the tab, the IPH is
  // granteed not to be shown. Returns `true` if the IPH bubble is shown after
  // this method call, which includes the case when it was open before the call.
  virtual bool ShowAutofillFieldIphForFeature(
      const FormFieldData& field,
      AutofillClient::IphFeature feature);

  // Hides any open IPH.
  virtual void HideAutofillFieldIph();

  // Notifies the IPH code that `feature` was used.
  virtual void NotifyIphFeatureUsed(AutofillClient::IphFeature feature);

  // Stores test addresses provided by devtools and used to help developers
  // debug their forms with a list of well formatted addresses. Differently from
  // other `AutofillProfile`s/addresses, this list is stored in the client,
  // instead of the `PersonalDataManager`.
  virtual void set_test_addresses(std::vector<AutofillProfile> test_addresses);

  virtual base::span<const AutofillProfile> GetTestAddresses() const
      LIFETIME_BOUND;

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

  // Triggers the HaTS survey of the `survey_type`.
  // TODO: crbug.com/348139343 - Move back for components/plus_addresses.
  virtual void TriggerPlusAddressUserPerceptionSurvey(
      plus_addresses::hats::SurveyType survey_type);

  // Returns the service used in order to log metrics into MQLS.
  virtual optimization_guide::ModelQualityLogsUploaderService*
  GetMqlsUploadService();

  // Shows a bubble asking whether the user wants to save or update Autofill AI
  // data. `old_entity` is present in the update cases. It is used to give users
  // a better understanding of what was updated.
  virtual void ShowEntityImportBubble(
      EntityInstance new_entity,
      std::optional<EntityInstance> old_entity,
      EntityImportPromptResultCallback prompt_closed_callback);

  virtual void ShowEmailVerifiedToast();

  // May return null on platforms where OTPs are not supported.
  virtual OtpFieldDetector* GetOtpFieldDetector();

  // Returns the delegate for OTP phish guard, which can be used to perform
  // security checks before offering an OTP. May return nullptr.
  virtual OtpPhishGuardDelegate* GetOtpPhishGuardDelegate();

  // May return null on platforms where no OneTimeTokenService is supported.
  virtual one_time_tokens::OneTimeTokenService* GetOneTimeTokenService() const;

  // Returns true if the primary main frame's document used the WebOTP API. This
  // exists only for the main frame because only the main frame has the
  // permission to call the WeOTP API.
  virtual bool DocumentUsedWebOTP();

  // Returns the helper for Password Manager integrations.
  virtual PasswordManagerAutofillHelperDelegate*
  GetPasswordManagerAutofillHelper();

  // Returns the AutofillManager instance for the current frame/tab.
  virtual AutofillManager* GetAutofillManagerForPrimaryMainFrame();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_CLIENT_H_
