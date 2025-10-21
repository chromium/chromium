// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/feature_list.h"

namespace autofill::features {

namespace {
constexpr bool IS_AUTOFILL_AI_PLATFORM = BUILDFLAG(IS_CHROMEOS) ||
                                         BUILDFLAG(IS_LINUX) ||
                                         BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN);
}

// LINT.IfChange(autofill_across_iframes_ios)
// Controls whether to flatten and fill cross-iframe forms on iOS.
// TODO(crbug.com/40266699) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframesIos, base::FEATURE_ENABLED_BY_DEFAULT);

// Throttles child frame extraction to a maximum number of child frames that
// can be extracted by applying the following rules: (1) remove the child frames
// from an individual form that busts the limit and (2) stop extracting child
// frames on other forms once the limit is reached across forms.
BASE_FEATURE(kAutofillAcrossIframesIosThrottling,
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_across_iframes_ios)

// Controls whether to trigger form extraction when detecting a form activity on
// a xframe form. Only effective when Autofill is enabled across iframes
// (kAutofillAcrossIframesIos).
BASE_FEATURE(kAutofillAcrossIframesIosTriggerFormExtraction,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag to control displaying of Autofill suggestions on
// unclassified fields based on prefix matching. These suggestions are displayed
// after the user typed a certain number of characters that match some data
// stored in the user's profile.
// TODO(crbug.com/381994105): Cleanup when launched.
BASE_FEATURE(kAutofillAddressSuggestionsOnTyping,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This parameter enables updating the minimum number of characters a user needs
// to type to maybe see an Autofill on typing suggestion.
const base::FeatureParam<int> kAutofillOnTypingMinNumberCharactersToMatch{
    &kAutofillAddressSuggestionsOnTyping, "min_number_characters_to_match", 3};

// This parameter enables updating the maximum number of characters typed until
// Autofill on typing suggestions are no longer displayed.
const base::FeatureParam<int> kAutofillOnTypingMaxNumberCharactersToMatch{
    &kAutofillAddressSuggestionsOnTyping, "max_number_characeters_to_match",
    10};

// This parameter enables updating the required number of characters that need
// to be missing between the typed data and the profile data. This makes sure
// the value offered by the feature is higher, by for example not displaying a
// suggestion to fill "Tomas" when the user typed "Tom", since at this point
// users are more likely to simply finish typing.
const base::FeatureParam<int> kAutofillOnTypingMinMissingCharactersNumber{
    &kAutofillAddressSuggestionsOnTyping, "min_missing_characters_number", 5};

// This parameter enables updating the field types offered in Autofill on typing
// suggestions. Field types are defined as enums, so this parameter should be a
// string of integers separated by comma, such as "34,22,44,11". If the string
// cannot be parsed or some value is out of bound of the field types enum, the
// param is ignored. When this param is an empty string (default value), a
// default list of field types is used.
const base::FeatureParam<std::string> kAutofillOnTypingFieldTypes{
    &kAutofillAddressSuggestionsOnTyping, "field_types", ""};

// Feature flag to controls whether Autofill on typing suggestions will have a
// strike database.
BASE_FEATURE(kAutofillAddressSuggestionsOnTypingHasStrikeDatabase,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag controlling the display of surveys when a user declines the
// save prompt of Autofill address and a user does not have any address stored.
// The goal is to understand the reason and work towards improving acceptance.
BASE_FEATURE(kAutofillAddressUserDeclinedSaveSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag to control the displaying of an ongoing hats survey that
// measures users perception of Autofill. Differently from other surveys,
// the Autofill user perception survey will not have a specific target
// number of answers where it will be fully stop, instead, it will run
// indefinitely. A target number of full answers exists, but per quarter. The
// goal is to have a go to place to understand how users are perceiving autofill
// across quarters.
BASE_FEATURE(kAutofillAddressUserPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, autofill will fill not skip filling fields that had an initial
// value which was modified.
BASE_FEATURE(kAutofillAllowFillingModifiedInitialValues,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled (and if `AutofillAiServerModel` is also enabled), this ignores
// the `may_run_server_model` boolean sent by the Autofill server and, instead,
// queries the server model for every encountered form that is not already
// cached locally.
// Only intended for testing.
BASE_FEATURE(kAutofillAiAlwaysTriggerServerModel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi entities will be deduped on every major milestone.
BASE_FEATURE(kAutofillAiDedupeEntities, base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch. If enabled, the EntityDataManager is created irrespective of
// whether other features are enabled. This is necessary so that cleaning up the
// browsing data also removes data if the user left the study.
BASE_FEATURE(kAutofillAiCreateEntityDataManager,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Kill switch: If enabled, MayPerformAutofillAiAction() also depends on two
// prefs that enable/disable filling and import of identity-related and
// travel-related entities.
// TODO(crbug.com/450060416): Remove after M144 branch point (2025-01-12).
BASE_FEATURE(kAutofillAiIdentityAndTravelPrefs,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, no account-level capabilities are checked to determine whether
// a user is eligible for AutofillAI.
BASE_FEATURE(kAutofillAiIgnoreCapabilityCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after a walletable suggestion is
// displayed and the form submitted. The survey does not require the suggestion
// to be accepted.
BASE_FEATURE(kAutofillAiFillingSurvey, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the save prompt for a walletable
// entity was interacted with.
BASE_FEATURE(kAutofillAiSavePromptSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kAutofillAiSavePromptSurveyAcceptedTriggerId{
        &kAutofillAiSavePromptSurvey,
        "autofill_ai_walletable_entity_save_prompt_survey_accepted_trigger_id",
        ""};
const base::FeatureParam<std::string>
    kAutofillAiSavePromptSurveyDeclinedTriggerId{
        &kAutofillAiSavePromptSurvey,
        "autofill_ai_walletable_entity_save_prompt_survey_declined_trigger_id",
        ""};
// Allows us to control which actions `kAutofillAiIgnoreCapabilityCheck` applies
// to. If `kAutofillAiIgnoreCapabilityCheckOnlyForNonModelActions` is true, then
// MES and MQLS interactions are still constrained by an account-level
// capability check.
const base::FeatureParam<bool>
    kAutofillAiIgnoreCapabilityCheckOnlyForNonModelActions{
        &kAutofillAiIgnoreCapabilityCheck,
        "autofill_ai_ignore_capability_check_only_for_non_model_actions",
        false};

// If enabled, no GeoIp requirements are imposed for AutofillAi.
// Note that this feature can be modified as follows (all assuming that
// `kAutofillAiIgnoreGeoIp` is enabled):
// - If both `kAutofillAiIgnoreGeoIpAllowlist` and
//   `kAutofillAiIgnoreGeoIpBlocklist` are empty, then all geo IPs are
//   permitted.
// - If only `kAutofillAiIgnoreGeoIpBlocklist` is non-empty, then all geo ips
//   but those in `kAutofillAiIgnoreGeoIpBlocklist` are permitted.
// - If `kAutofillAiIgnoreGeoIpAllowlist` is non-empty, then only geo ips in
//   `kAutofillAiIgnoreGeoIpAllowlist` are permitted.
//
// Both the allowlist and the blocklist are expected to consist of
// comma-separated uppercase two-digit country codes (see documentation of
// `GeoIpCountryCode`.)
BASE_FEATURE(kAutofillAiIgnoreGeoIp, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAutofillAiIgnoreGeoIpAllowlist{
    &kAutofillAiIgnoreGeoIp, "autofill_ai_geo_ip_allowlist", ""};

const base::FeatureParam<std::string> kAutofillAiIgnoreGeoIpBlocklist{
    &kAutofillAiIgnoreGeoIp, "autofill_ai_geo_ip_blocklist", ""};

// If enabled, no locale requirements are imposed for AutofillAi.
BASE_FEATURE(kAutofillAiIgnoreLocale, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, no sign-in requirement is imposed for Autofill. Note that if this
// feature is enabled, the value of `kAutofillAiIgnoreCapabilityCheck` is
// irrelevant.
BASE_FEATURE(kAutofillAiIgnoreSignInState, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the existence of address or payments data is not required to show
// the Iph bubble for AutofillAi.
BASE_FEATURE(kAutofillAiIgnoreWhetherUserHasAddressOrPaymentsDataForIph,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi supports known traveler numbers.
BASE_FEATURE(kAutofillAiKnownTravelerNumber, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi supports national id cards.
BASE_FEATURE(kAutofillAiNationalIdCard, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi supports redress number.
BASE_FEATURE(kAutofillAiRedressNumber, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, this makes the autofill classification logic prefer the
// AutofillAi predictions sent via the server response over local heuristic
// predictions.
BASE_FEATURE(kAutofillAiPreferModelResponseOverHeuristics,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the client may trigger the server model for AutofillAI type
// predictions.
BASE_FEATURE(kAutofillAiServerModel,
             IS_AUTOFILL_AI_PLATFORM ? base::FEATURE_ENABLED_BY_DEFAULT
                                     : base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi supports flight reservation entities from Google
// Wallet.
BASE_FEATURE(kAutofillAiWalletFlightReservation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi supports vehicle registration entities from Google
// Wallet.
BASE_FEATURE(kAutofillAiWalletVehicleRegistration,
             base::FEATURE_DISABLED_BY_DEFAULT);

// The maximum duration for which an AutofillAI server model response is kept in
// the local cache. NOTE: It is advisable to choose a value that is at least as
// large as the cache duration for Autofill server responses to limit cases in
// which the model is run multiple times for the same form.
const base::FeatureParam<base::TimeDelta> kAutofillAiServerModelCacheAge{
    &kAutofillAiServerModel, "autofill_ai_model_cache_age", base::Days(7)};

// The maximum size of the AutofillAI server model cache.
const base::FeatureParam<int> kAutofillAiServerModelCacheSize{
    &kAutofillAiServerModel, "autofill_ai_model_cache_size", 100};

// The timeout for running the AutofillAI server model.
const base::FeatureParam<base::TimeDelta>
    kAutofillAiServerModelExecutionTimeout{
        &kAutofillAiServerModel, "autofill_ai_model_execution_timeout",
        base::Seconds(60)};

// Whether AnnotatedPageContent is included in the request to the AutofillAI
// model.
const base::FeatureParam<bool> kAutofillAiServerModelSendPageContent{
    &kAutofillAiServerModel, "autofill_ai_model_send_apc", true};

// Whether the page's full URL is included in the data sent to the model.
const base::FeatureParam<bool> kAutofillAiServerModelSendPageUrl{
    &kAutofillAiServerModel, "autofill_ai_model_send_page_url", false};

// Whether the user may use the locally cached results from the server model
// to provide AutofillAI predictions for filling and importing.
const base::FeatureParam<bool> kAutofillAiServerModelUseCacheResults{
    &kAutofillAiServerModel, "autofill_ai_model_use_cache_results", false};

// If enabled, votes for prefix and suffix lengths of identification number
// fields are uploaded. For example, if there's a passport with number CX1235987
// on file, <input type=text value=CX12> uploads a format string "4".
// TODO(crbug.com/429704303): Clean up when launched.
BASE_FEATURE(kAutofillAiVoteForFormatStringsForAffixes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, votes for the format of flight number fields are uploaded. For
// example, if there is a flight number "LH89" on file, a submitted value of
// "89" on a field with type `FLIGHT_RESERVATION_FLIGHT_NUMBER` uploads "N".
BASE_FEATURE(kAutofillAiVoteForFormatStringsForFlightNumbers,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the second iteration AutofillAI.
BASE_FEATURE(kAutofillAiWithDataSchema,
             IS_AUTOFILL_AI_PLATFORM ? base::FEATURE_ENABLED_BY_DEFAULT
                                     : base::FEATURE_DISABLED_BY_DEFAULT);

// This parameter enables adding an experiment id to requests to the Autofill
// to enable Autofill AI predictions. The experiment id is not used for other
// backends.
const base::FeatureParam<int> kAutofillAiWithDataSchemaServerExperimentId{
    &kAutofillAiWithDataSchema, "autofill_ai_server_experiment_id",
    IS_AUTOFILL_AI_PLATFORM ? 3314871 : 0};

// When enabled, requests and responses of client-triggered Autofill AI model
// runs are uploaded to MQLS.
BASE_FEATURE(kAutofillAiUploadModelRequestAndResponse,
             IS_AUTOFILL_AI_PLATFORM ? base::FEATURE_ENABLED_BY_DEFAULT
                                     : base::FEATURE_DISABLED_BY_DEFAULT);

// Guards the refactoring to allow showing Autofill and Password suggestions in
// the same surface instead of being mutually exclusive.
BASE_FEATURE(kAutofillAndPasswordsInSameSurface,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Same as `kAutofillAddressUserPerceptionSurvey` but for credit card forms.
BASE_FEATURE(kAutofillCreditCardUserPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag controlling the display of surveys when a user does not
// accept an Autofill suggestion. The goal is to understand the reason and work
// towards improving acceptance.
BASE_FEATURE(kAutofillAddressUserDeclinedSuggestionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag controlling the deduplication of GAS addresses. When disabled
// GAS addresses will never be deleted as part of the deduplication flow.
// TODO(crbug.com/357074792): Remove when launched.
BASE_FEATURE(kAutofillDeduplicateAccountAddresses,
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_disallow_more_hyphen_like_labels)
// When enabled, the list of characters a label cannot exclusively consist of
// includes more hyphen-like characters: em-dash, minus sign and fullwidth
// hyphen-minus.
// TODO(crbug.com/440039204): Remove when launched.
BASE_FEATURE(kAutofillDisallowMoreHyphenLikeLabels,
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_disallow_more_hyphen_like_labels)

// Kill switch for Autofill filling.
BASE_FEATURE(kAutofillDisableFilling, base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for Autofill address import.
BASE_FEATURE(kAutofillDisableAddressImport, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a new implementation for address field parsing that is based on
// backtracking.
BASE_FEATURE(kAutofillEnableAddressFieldParserNG,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug.com/40741721): Remove once shared labels are launched.
BASE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control if Autofill supports German transliteration.
// TODO(crbug.com/328968064): Remove when/if launched.
BASE_FEATURE(kAutofillEnableGermanTransliteration,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a couple of improvements to credit card expiration date handling:
// - The autocomplete attribute values are rationalized with format strings
//   like MM/YY from placeholders and labels in mind.
// - more fill follow.
// TODO(crbug.com/40266396): Remove once launched.
BASE_FEATURE(kAutofillEnableExpirationDateImprovements,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to save the first number in a form with multiple phone
// numbers instead of aborting the import.
// TODO(crbug.com/40742746) Remove once launched.
BASE_FEATURE(kAutofillEnableImportWhenMultiplePhoneNumbers,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/40735892): Remove once launched.
BASE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will help users fill in loyalty card details.
// TODO(crbug.com/395831853): Remove once launched.
BASE_FEATURE(kAutofillEnableLoyaltyCardsFilling,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Autofill will display joined email and loyalty card Autofill
// suggestions.
// TODO(crbug.com/416664590): Remove once launched.
BASE_FEATURE(kAutofillEnableEmailOrLoyaltyCardsFilling,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, only non-ad frames are extracted.
// Otherwise, non-ad frames as well as *visible* ad frames are extracted.
// "Extracted" means that FormFieldData::child_frames is populated, which is
// necessary for flattening these forms.
// The forms in those frames are extracted either way.
// TODO(crbug.com/40196220): Remove once launched.
BASE_FEATURE(kAutofillExtractOnlyNonAdFrames,
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_ignore_checkable_elements)
// If enabled, checkboxes and radio buttons aren't extracted anymore.
// TODO(crbug.com/40283901): Remove once launched. Also remove
// - autofill::FormControlType::kInputCheckbox
// - autofill::FormControlType::kInputRadio
BASE_FEATURE(kAutofillIgnoreCheckableElements,
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_ignore_checkable_elements)

// When enabled, address field swapping suggestions will not include a
// suggestion matching the field's current value. This decreases noises in the
// suggestion UI.
// TODO(crbug.com/381531027): Remove when launched.
BASE_FEATURE(kAutofillImproveAddressFieldSwapping,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, new `negative_pattern` regex values will be used
// in order to reduce false positive classifications of city fields.
// TODO(crbug.com/330508437): Clean up when launched.
BASE_FEATURE(kAutofillImproveCityFieldClassification,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, focusing on a credit card number field that was traditionally
// autofilled will yield all credit card suggestions.
// TODO(crbug.com/354175563): Remove when launched.
BASE_FEATURE(kAutofillPaymentsFieldSwapping, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, password manager and autofill bubbles will be shown based on
// the priorities of the bubbles.
// TODO(crbug.com/432429605): Remove when launched.
BASE_FEATURE(kAutofillShowBubblesBasedOnPriorities,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, chrome will support home and work addresses from account.
// TODO: crbug.com/354706653 - Clean up when launched.
BASE_FEATURE(kAutofillEnableSupportForHomeAndWork,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, chrome will support name and email address profile.
// TODO(cbug.com/356845298): Clean up when launched.
BASE_FEATURE(kAutofillEnableSupportForNameAndEmail,
             base::FEATURE_DISABLED_BY_DEFAULT);

// The number of times after which, a never accepted `kAccountNameEmail`
// suggestion will result in the `kAccountNameEmail` profile being deleted.
const base::FeatureParam<int> kAutofillNameAndEmailProfileNotSelectedThreshold{
    &kAutofillEnableSupportForNameAndEmail, "rejection_threshold", 10};

// The pattern used to remove nicknames from the account full name before
// creating the kAccountNameEmail profile.
const base::FeatureParam<std::string> kAutofillNameAndEmailProfileNicknameRegex{
    &kAutofillEnableSupportForNameAndEmail, "nickname_regex",
    R"(\s+\([^)]*\)|\s+\"[^\"]*\")"};

// When enabled, the autofill suggestion labels are more descriptive and
// relevant.
// TODO(crbug.com/380273791): Cleanup when launched.
BASE_FEATURE(kAutofillImprovedLabels, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether main text should also be improved or not.
// TODO(crbug.com/380273791): Clean up when launched.
const base::FeatureParam<bool>
    kAutofillImprovedLabelsParamWithoutMainTextChangesParam{
        &kAutofillImprovedLabels,
        "autofill_improved_labels_without_main_text_changes", false};

// Controls whether differentiating labels should be shown before or after the
// improved labels.
// TODO(crbug.com/380273791): Clean up when launched.
const base::FeatureParam<bool>
    kAutofillImprovedLabelsParamWithDifferentiatingLabelsInFrontParam{
        &kAutofillImprovedLabels,
        "autofill_improved_labels_with_differentiating_labels_in_front", false};

// If enabled, the new suggestion generation logic is used.
// TODO(crbug.com/409962888): Remove once launched.
BASE_FEATURE(kAutofillNewSuggestionGeneration,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, we start forwarding submissions with source
// DOM_MUTATION_AFTER_AUTOFILL, even for non-password forms.
BASE_FEATURE(kAutofillAcceptDomMutationAfterAutofillSubmission,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Removes logic that resets form submission tracking data upon receiving a
// FORM_SUBMISSION or PROBABLE_FORM_SUBMISSION signal. Also, fixes submission
// deduplication so that it ignores submissions that PWM doesn't act upon.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillFixFormTracking, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the MergeMode::kMergeChildrenAndReformatIfNeeded will be added to
// the StreetAddressNode, StreetLocationNode and HouseNumberAndApartmentNode's
// merge mode.
// TODO(crbug.com/447111009): Remove when launched.
BASE_FEATURE(kAutofillUseChildrenAndReformatMergeMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses AutofillAgent::GetSubmittedForm() in HTML submissions.
// See `AutofillAgent::GetSubmittedForm()` for more documentation.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillUseSubmittedFormInHtmlSubmission,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Replaces blink::WebFormElementObserver usage in FormTracker by updated logic
// for tracking the disappearance of forms as well as other submission
// triggering events. See `AutofillAgent::GetSubmittedForm()` for more
// documentation.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillPreferSavedFormAsSubmittedForm,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows the import of an Autofill profile if duplicate fields were present
// with identical field values.
// TODO(crbug.com/395855125): Remove when launched.
BASE_FEATURE(kAutofillRelaxAddressImport, base::FEATURE_DISABLED_BY_DEFAULT);

// Replaces blink::WebFormElementObserver usage in FormTracker by updated logic
// for tracking the disappearance of forms as well as other submission
// triggering events.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillReplaceFormElementObserver,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, new heuristics are applied for disambiguating multiple possible
// types in a form field. Otherwise, only the already established heuristic for
// disambiguating address and credit card names is used.
BASE_FEATURE(kAutofillDisambiguateContradictingFieldTypes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Replaces cached web elements in AutofillAgent and FormTracker by their
// renderer ids.
BASE_FEATURE(kAutofillReplaceCachedWebElementsByRendererIds,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for India, overriding the legacy one.
BASE_FEATURE(kAutofillUseINAddressModel, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Japan, overriding the legacy one.
BASE_FEATURE(kAutofillSupportPhoneticNameForJP,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using custom name model with last name prefixes support.
BASE_FEATURE(kAutofillSupportLastNamePrefix, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables splitting two-part zip codes into two fields while filling and
// importing split zip codes from two adjacent fields.
// TODO(crbug.com/369503318): Clean up when launched.
BASE_FEATURE(kAutofillSupportSplitZipCode, base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch: If true, FormFieldData::IsFocusable will allow returning false
// for fields with role="presentation" html attribute.
// TODO(crbug.com/444754999): Clean up after confirming this is safe after M143
// release.
BASE_FEATURE(kAutofillSupportPresentationRole,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch: If true, AutofillManager::AfterParsingFinishesDeprecated()
// becomes the identity function. That is, it does not delay the callback until
// after parsing has finished.
// TODO(crbug.com/448144129): Clean up after M144 branch point (Dec 1, 2025).
BASE_FEATURE(kAutofillSynchronousAfterParsing,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables extended zip code validation
// and new zip code merging logic.
// TODO(crbug.com/434140055): Clean up when launched.
BASE_FEATURE(kAutofillZipCodeValidationAndMerging,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the form field parser won't try to match other attributes if
// any of the negative patterns matched.
BASE_FEATURE(kAutofillUseNegativePatternForAllAttributes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, all behaviours related to the on-device machine learning
// model for field type predictions will be guarded.
// TODO(crbug.com/40276177): Remove when launched.
BASE_FEATURE(kAutofillModelPredictions, base::FEATURE_DISABLED_BY_DEFAULT);

// When true, use the machine learning model as the active `HeuristicSource`,
// else use the source provided by `kAutofillParsingPatternActiveSource`.
// It is defined with `BASE_FEATURE_PARAM()` to enable caching as the parameter
// is accesses in several getters.
BASE_FEATURE_PARAM(bool,
                   kAutofillModelPredictionsAreActive,
                   &kAutofillModelPredictions,
                   "model_active",
                   false);

// When true, apply small form rules to ML predictions - if there are too few
// fields or too few distinct types, predictions are cleared. There are some
// special cases. See
// `FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields`.
BASE_FEATURE_PARAM(bool,
                   kAutofillModelPredictionsSmallFormRules,
                   &kAutofillModelPredictions,
                   "small_form_rules",
                   false);

// If enabled, a pre-filled field will not be filled.
BASE_FEATURE(kAutofillSkipPreFilledFields, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables detection of language from Translate.
// TODO(crbug.com/40158074): Cleanup when launched.
BASE_FEATURE(kAutofillPageLanguageDetection, base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, before triggering suggestion acceptance, the row
// view checks that a substantial portion of its content was visible for some
// minimum required period.
// TODO(crbug.com/337222641): During cleaning up, in the popup row view remove
// emitting of "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough".
BASE_FEATURE(kAutofillPopupDontAcceptNonVisibleEnoughSuggestion,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/334909042): Remove after cleanup.
// If the feature is enabled, the Autofill popup widget is initialized with
// `Widget::InitParams::z_order` set to `ui::ZOrderLevel::kSecuritySurface`,
// otherwise the `z_order` is not set and defined by the widget type (see
// `Widget::InitParams::EffectiveZOrderLevel()`). This param makes the popup
// display on top of all other windows, which potentially can negatively
// affect their functionality.
BASE_FEATURE(kAutofillPopupZOrderSecuritySurface,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether Autofill may fill across origins.
// In payment forms, the cardholder name field is often on the merchant's origin
// while the credit card number and CVC are in iframes hosted by a payment
// service provider. By enabling the policy-controlled feature "shared-autofill"
// in those iframes, the merchant's website enable Autofill to fill the credit
// card number and CVC fields from the cardholder name field, even though this
// autofill operation crosses origins.
// TODO(crbug.com/1304721): Enable this feature.
BASE_FEATURE(kAutofillSharedAutofill, base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled, the AddressFieldParser does NOT try to parse
// address lines once it has found a street name and house number or other
// combinations of fields that indicate that an address form uses structured
// addresses. This should be the default in all countries with fully supported
// structured addresses. However, if a country is not sufficiently modeled,
// autofill may still do the right thing if it recognizes "Street name, house
// number, address line 2" as a sequence.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillStructuredFieldsDisableAddressLines,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls an ablation study in which autofill for addresses and payment data
// can be suppressed.
BASE_FEATURE(kAutofillEnableAblationStudy, base::FEATURE_DISABLED_BY_DEFAULT);
// The following parameters are only effective if the study is enabled.
// If "enabled_for_addresses" is true this means that the ablation study is
// enabled for addresses meaning that autofill may be disabled on some forms.
const base::FeatureParam<bool> kAutofillAblationStudyEnabledForAddressesParam{
    &kAutofillEnableAblationStudy, "enabled_for_addresses", false};
const base::FeatureParam<bool> kAutofillAblationStudyEnabledForPaymentsParam{
    &kAutofillEnableAblationStudy, "enabled_for_payments", false};
// The ratio of ablation_weight_per_mille / 1000 determines the chance of
// autofill being disabled on a given combination of site * time_window * client
// session. E.g. an ablation_weight_per_mille = 10 means that there is a 1%
// ablation chance.
const base::FeatureParam<int> kAutofillAblationStudyAblationWeightPerMilleParam{
    &kAutofillEnableAblationStudy, "ablation_weight_per_mille", 0};
// If not 0, the kAutofillAblationStudyAblationWeightPerMilleListXParam
// specify the ablation chances for sites that are on the respective list X.
// These parameters are different from
// kAutofillAblationStudyAblationWeightPerMilleParam which applies to all
// domains.
const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList1Param{
        &kAutofillEnableAblationStudy, "ablation_weight_per_mille_param1", 0};
const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList2Param{
        &kAutofillEnableAblationStudy, "ablation_weight_per_mille_param2", 0};
const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList3Param{
        &kAutofillEnableAblationStudy, "ablation_weight_per_mille_param3", 0};
const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList4Param{
        &kAutofillEnableAblationStudy, "ablation_weight_per_mille_param4", 0};
const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList5Param{
        &kAutofillEnableAblationStudy, "ablation_weight_per_mille_param5", 0};
const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList6Param{
        &kAutofillEnableAblationStudy, "ablation_weight_per_mille_param6", 0};
// If true, the ablation study runs as an A/A study (no behavioral changes) but
// clients are assigned to the respective groups.
const base::FeatureParam<bool> kAutofillAblationStudyIsDryRun{
    &kAutofillEnableAblationStudy, "ablation_study_is_dry_run", false};
// Improves the selection of phone country codes by also considering address
// country codes / names.
// See GetStreetAddressForInput() in field_filling_address_util.cc for a details
// description.
// TODO(crbug.com/40249216). Clean up when launched.
BASE_FEATURE(kAutofillEnableFillingPhoneCountryCodesByAddressCountryCodes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls autofill popup style, if enabled it becomes more prominent,
// i.e. its shadow becomes more emphasized, position is also updated.
// TODO(crbug.com/40235454): Remove once the experiment is over.
BASE_FEATURE(kAutofillMoreProminentPopup, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillMoreProminentPopupMaxOffsetToCenterParam{
    &kAutofillMoreProminentPopup, "max_offset_to_center_px", 92};

// TODO(crbug.com/346507576): Remove once the experiment is over.
// When enabled, makes autocomplete label sensitive.
BASE_FEATURE(kAutofillLabelSensitiveAutocomplete,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Migration generation for the autocomplete label-sensitive feature.
// If the migration generation received from the Finch server is greater than
// the stored browser parameter, re-migrate AutocompleteTableLabelSensitive data
// from the old AutocompleteTable.
const base::FeatureParam<int>
    kAutofillLabelSensitiveAutocompleteMigrationGeneration{
        &kAutofillLabelSensitiveAutocomplete,
        "autocomplete_label_sensitive_migration_generation", 0};

// Enable the feature by default, and set the enabled percentage as a feature
// param. We are logging information of field types, autofill status and
// forms with a defined sampling rate of 10% on sessions.
// Autofill FormSummary/FieldInfo UKM schema:
// https://docs.google.com/document/d/1ZH0JbL6bES3cD4KqZWsGR6n8I-rhnkx6no6nQOgYq5w/.
BASE_FEATURE(kAutofillLogUKMEventsWithSamplingOnSession,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillLogUKMEventsWithSamplingOnSessionRate{
    &kAutofillLogUKMEventsWithSamplingOnSession, "sampling_rate", 10};

// Controls whether user tap on an element is needed to show autofill
// suggestions. If enabled, this flag would disable android autofill suggestions
// if the focus on an element is Javascript-originated.
// DidReceiveLeftMouseDownOrGestureTapInNode() will show suggestions if the
// focus change occurred as a result of a gesture. See crbug.com/730764 for why
// showing autofill suggestions as a result of JavaScript changing focus is
// enabled on WebView.
// TODO(crbug.com/40286775) Clean up autofill feature flag
// `kAutofillAndroidDisableSuggestionsOnJSFocus`
BASE_FEATURE(kAutofillAndroidDisableSuggestionsOnJSFocus,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, FormFieldParser::MatchesRegexWithCache tries to avoid
// re-computing whether a regex matches an input string by caching the result.
// The result size is controlled by
// kAutofillEnableCacheForRegexMatchingCacheSizeParam.
BASE_FEATURE(kAutofillEnableCacheForRegexMatching,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kAutofillEnableCacheForRegexMatchingCacheSizeParam{
        &kAutofillEnableCacheForRegexMatching, "cache_size", 1000};

BASE_FEATURE(kAutofillUKMExperimentalFields, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kAutofillUKMExperimentalFieldsBucket0{
    &kAutofillUKMExperimentalFields, "autofill_experimental_regex_bucket0", ""};
const base::FeatureParam<std::string> kAutofillUKMExperimentalFieldsBucket1{
    &kAutofillUKMExperimentalFields, "autofill_experimental_regex_bucket1", ""};
const base::FeatureParam<std::string> kAutofillUKMExperimentalFieldsBucket2{
    &kAutofillUKMExperimentalFields, "autofill_experimental_regex_bucket2", ""};
const base::FeatureParam<std::string> kAutofillUKMExperimentalFieldsBucket3{
    &kAutofillUKMExperimentalFields, "autofill_experimental_regex_bucket3", ""};
const base::FeatureParam<std::string> kAutofillUKMExperimentalFieldsBucket4{
    &kAutofillUKMExperimentalFields, "autofill_experimental_regex_bucket4", ""};

// When enabled, Greek regexes are used for parsing in branded builds.
BASE_FEATURE(kAutofillGreekRegexes, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables uploading fields that were autofilled with fallback types.
// TODO: crbug.com/444147005 - Clean up after this feature is rolled out.
BASE_FEATURE(kAutofillUploadManualFallbackFieldsToServer,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables uploading of more data to the Autofill server to use for computing
// signatures: go/autofill-signatures-more-data.
BASE_FEATURE(kAutofillServerUploadMoreData, base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/435646513) - Clean-up after feature lands at 100% Stable.
// Enables the new experimental server-side signatures for evaluation purposes.
BASE_FEATURE(kAutofillServerExperimentalSignatures,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Replaces the secondary signature with the structural signature for Uploads.
// For Queries still only the secondary (alternative) signature is used.
// TODO(crbug.com/431737839): Clean up when roll out finishes successfully.
BASE_FEATURE(kAutofillUseStructuralSignatureInsteadOfSecondary,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, forms that are only identified through server predictions
// are considered for key and funnel metric logging. Without this feature, due
// to a bug, only forms identified by parsing are considered.
// TODO(crbug.com/436171158): Clean up when launched.
BASE_FEATURE(kAutofillConsiderServerOnlyFormsInKeyMetrics,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the field classification model uses runtime caching to not run
// models on the same inputs multiple times.
// TODO(crbug.com/371933424). Clean up when launched, if not used for Autofill
// experiments.
BASE_FEATURE(kFieldClassificationModelCaching,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the successful first time creation
// flow.
BASE_FEATURE(kPlusAddressAcceptedFirstTimeCreateSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the declined the first plus
// address creation flow.
BASE_FEATURE(kPlusAddressDeclinedFirstTimeCreateSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user fills a plus address
// after triggering autofill manually.
BASE_FEATURE(kPlusAddressFilledPlusAddressViaManualFallbackSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user creates a 3rd+ plus
// address.
BASE_FEATURE(kPlusAddressUserCreatedMultiplePlusAddressesSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user creates a plus address
// triggering the popup via the Chrome context menu on Desktop or via the
// Keyboard Accessory on Android.
BASE_FEATURE(kPlusAddressUserCreatedPlusAddressViaManualFallbackSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user chooses to fill an email
// when a plus address suggestion is also offered in the Autofill popup.
BASE_FEATURE(kPlusAddressUserDidChooseEmailOverPlusAddressSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user chooses to fill a plus
// address when an email suggestion is also offered in the Autofill popup.
BASE_FEATURE(kPlusAddressUserDidChoosePlusAddressOverEmailSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the placeholder is not considered a label fallback on the
// renderer side anymore. Instead, local heuristic will match regexes against
// either the label or the placeholder, depending on how high quality the label
// is. If no matche is found, local heuristics fall back to the other value.
// This feature can be thought of as "lightweight" multi-label support.
// TODO(crbug.com/320965828): Remove when launched.
BASE_FEATURE(kAutofillBetterLocalHeuristicPlaceholderSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the address add/edit editor in the payments request would be
// removed and instead, the address editor from the settings will be used.
// TODO: crbug.com/399071964 - Remove when launched.
BASE_FEATURE(kUseSettingsAddressEditorInPaymentsRequest,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the rewriter uses updated rewrite rules.
// TODO(crbug.com/445863287): Cleanup when launched.
BASE_FEATURE(kAutofillFixRewriterRules, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)

// If enabled, on Android desktop, the Autofill keyboard accessory will have a
// new behavior and design.
// TODO(crbug.com/438125774): Remove when launched.
BASE_FEATURE(kAutofillAndroidDesktopKeyboardAccessoryRevamp,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, on Android desktop, Autofill keyboard accessory will be
// suppressed when there are no autofill suggestions.
BASE_FEATURE(kAutofillAndroidDesktopSuppressAccessoryOnEmpty,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, other apps can open the Autofill Options in Chrome.
BASE_FEATURE(kAutofillDeepLinkAutofillOptions,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls if Chrome Keyboard Accessory on Android displays 2 line chips.
// TODO: crbug.com/385172647 - Clean up after the feature is launched.
BASE_FEATURE(kAutofillEnableKeyboardAccessoryChipRedesign,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome Keyboard Accessory limits the width of the first chip or
// the first 2 chips to display a part of the next one on the screen.
// TODO: crbug.com/385172647 - Clean up after the feature is launched.
BASE_FEATURE(kAutofillEnableKeyboardAccessoryChipWidthAdjustment,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome Autofill UI surfaces ignore touch events if something is
// fully or partially obscuring the Chrome window.
BASE_FEATURE(kAutofillEnableSecurityTouchEventFilteringAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill Services can query whether Chrome provides forms as
// virtual view structures to third party providers.
BASE_FEATURE(kAutofillThirdPartyModeContentProvider,
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

// Defines if the "Your Saved Info" page is eligible to be shown in Chrome
// settings.
BASE_FEATURE(kYourSavedInfoSettingsPage, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, updates the "Autofill and passwords" (or "Passwords and
// autofill") labels and icons to "Your saved info".
BASE_FEATURE(kYourSavedInfoBrandingInSettings,
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace test {

// If enabled, forces the deduplication pipeline to run on every startup,
// bypassing the 'once per milestone' limit.
BASE_FEATURE(kAutofillSkipDeduplicationRequirements,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Testing tool that collects metrics during a run of the captured site tests
// and dumps the collected metrics into a specified output directory.
// For each test, a file named {test-name}.txt is created. It contains all the
// collected metrics in the following format.
// histogram-name-1
// bucket value
// ...
// histogram-name-2
// ...
// The set of metrics can be restricted using
// `kAutofillCapturedSiteTestsMetricsScraperMetricNames`.
// It is helpful in conjunction with `tools/captured_sites/metrics-scraper.py`.
BASE_FEATURE(kAutofillCapturedSiteTestsMetricsScraper,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Name of the directory to write the results into.
const base::FeatureParam<std::string>
    kAutofillCapturedSiteTestsMetricsScraperOutputDir{
        &kAutofillCapturedSiteTestsMetricsScraper, "output_dir", "/tmp/"};
// A regex matching the histogram names that should be dumped. If not specified,
// the metrics of all histograms dumped.
const base::FeatureParam<std::string>
    kAutofillCapturedSiteTestsMetricsScraperHistogramRegex{
        &kAutofillCapturedSiteTestsMetricsScraper, "histogram_regex", ""};

// If enabled, Captured Site Tests will use 'AutofillFlow' utility to trigger
// the autofill action. This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillCapturedSiteTestsUseAutofillFlow,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill will not apply updates to address profiles based on data
// extracted from submitted forms. This feature is mostly for debugging and
// testing purposes and is not supposed to be launched.
BASE_FEATURE(kAutofillDisableProfileUpdates, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill will not apply silent updates to the structure of
// addresses and names. This feature is mostly for debugging and testing
// purposes and is not supposed to be launched.
BASE_FEATURE(kAutofillDisableSilentProfileUpdates,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for disabling suppressing suggestions based on the strike
// database.
BASE_FEATURE(kAutofillDisableSuggestionStrikeDatabase,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables logging the content of chrome://autofill-internals to the terminal.
BASE_FEATURE(kAutofillLogToTerminal, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows passing a set of overrides for Autofill server predictions.
// Example command line to override server predictions manually:
// chrome --enable-features=AutofillOverridePredictions:spec/1_2_4-7_8_9
// This creates two manual overrides that supersede server predictions as
// follows:
// * The server prediction for the field with signature 2 in the form with
//   signature 1 is overridden to be 4 (NAME_MIDDLE).
// * The server prediction for the field with signature 8 in the form with
//   signature 7 is overridden to be 9 (EMAIL_ADDRESS).
//
// See
// components/autofill/core/browser/crowdsourcing/server_prediction_overrides.h
// for more examples and details on how to specify overrides.
BASE_FEATURE(kAutofillOverridePredictions, base::FEATURE_DISABLED_BY_DEFAULT);

// The override specification in string form.
// See `OverrideFormat::kSpec` for details.
const base::FeatureParam<std::string> kAutofillOverridePredictionsSpecification{
    &kAutofillOverridePredictions, "spec", ""};

// The override specification in Base64-encoded JSON.
// See `OverrideFormat::kJson` for details.
const base::FeatureParam<std::string> kAutofillOverridePredictionsJson{
    &kAutofillOverridePredictions, "json", ""};

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
BASE_FEATURE(kAutofillServerCommunication, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing DOM Node ID of elements.
BASE_FEATURE(kShowDomNodeIDs, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
BASE_FEATURE(kAutofillShowTypePredictions, base::FEATURE_DISABLED_BY_DEFAULT);
// This variation controls whether the verbose version of the feature is used.
// In this version more information is attached to the respective DOM element,
// such as aria labels and descriptions and select element options values and
// texts.
const base::FeatureParam<bool> kAutofillShowTypePredictionsVerboseParam{
    &kAutofillShowTypePredictions, "verbose", false};

// This variation controls whether the autofill information of the element
// is shown as 'title' of the form field elements. If this parameter is on,
// the title attribute will be overwritten with autofill information.
// By default this is disabled to avoid data collection corruption.
const base::FeatureParam<bool> kAutofillShowTypePredictionsAsTitleParam{
    &kAutofillShowTypePredictions, "as-title", false};

// If enabled, ensures that the "autofill-information" attribute only contains a
// single FieldType in "overall type: <FieldTypes>". For example,
// "overall type: NAME_FULL, USERNAME" becomes "overall type: NAME_FULL" if the
// feature is enabled.
// TODO(crbug.com/435354393): Migrate the infrastructure to union types and
// remove this feature.
BASE_FEATURE(kAutofillUnionTypesSingleTypeInAutofillInformation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Autofill upload throttling limits uploading a form to the Autofill server
// more than once over a `kAutofillUploadThrottlingPeriodInDays` period.
// This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillUploadThrottling, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace test

}  // namespace autofill::features
