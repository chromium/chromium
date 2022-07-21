// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace autofill::features {

// Controls whether to flatten and fill cross-iframe forms.
// TODO(crbug.com/1187842) Remove once launched.
const base::Feature kAutofillAcrossIframes{"AutofillAcrossIframes",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, a save prompt will be shown to user upon form submission before
// storing any detected address profile.
const base::Feature kAutofillAddressProfileSavePrompt{
    "AutofillAddressProfileSavePrompt", base::FEATURE_ENABLED_BY_DEFAULT};

// This parameter controls if save profile prompts are automatically blocked for
// a given domain after N (default is 3) subsequent declines.
const base::FeatureParam<bool> kAutofillAutoBlockSaveAddressProfilePrompt{
    &kAutofillAddressProfileSavePrompt, "save_profile_prompt_auto_block", true};
// The auto blocking feature is based on a strike model. This parameter defines
// the months before such strikes expire.
const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptExpirationDays{
        &kAutofillAddressProfileSavePrompt,
        "save_profile_prompt_auto_block_strike_expiration_days", 180};
// The number of strikes before the prompt gets blocked.
const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptStrikeLimit{
        &kAutofillAddressProfileSavePrompt,
        "save_profile_prompt_auto_block_strike_limit", 3};

// Same as above but for update bubbles.
const base::FeatureParam<bool> kAutofillAutoBlockUpdateAddressProfilePrompt{
    &kAutofillAddressProfileSavePrompt, "update_profile_prompt_auto_block",
    true};
// Same as above but for update bubbles.
const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptExpirationDays{
        &kAutofillAddressProfileSavePrompt,
        "update_profile_prompt_auto_block_strike_expiration_days", 180};
// Same as above but for update bubbles.
const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptStrikeLimit{
        &kAutofillAddressProfileSavePrompt,
        "update_profile_prompt_auto_block_strike_limit", 3};

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, address data will be verified and autocorrected in the
// save/update prompt before saving an address profile. Relevant only if the
// AutofillAddressProfileSavePrompt feature is enabled.
const base::Feature kAutofillAddressProfileSavePromptAddressVerificationSupport{
    "AutofillAddressProfileSavePromptAddressVerificationSupport",
    base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, address profile save problem will contain a dropdown for
// assigning a nickname to the address profile. Relevant only if the
// AutofillAddressProfileSavePrompt feature is enabled.
const base::Feature kAutofillAddressProfileSavePromptNicknameSupport{
    "AutofillAddressProfileSavePromptNicknameSupport",
    base::FEATURE_DISABLED_BY_DEFAULT};

// By default, AutofillAgent and, if |kAutofillProbableFormSubmissionInBrowser|
// is enabled, also ContentAutofillDriver omit duplicate form submissions, even
// though the form's data may have changed substantially. If enabled, the
// below feature allows duplicate form submissions.
// TODO(crbug/1117451): Remove once the form-submission experiment is over.
const base::Feature kAutofillAllowDuplicateFormSubmissions{
    "AutofillAllowDuplicateFormSubmissions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether autofill activates on non-HTTP(S) pages. Useful for
// automated with data URLS in cases where it's too difficult to use the
// embedded test server. Generally avoid using.
const base::Feature kAutofillAllowNonHttpActivation{
    "AutofillAllowNonHttpActivation", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the country calling code for nationally formatted phone numbers
// is inferred from the profile's country, if available.
// TODO(crbug.com/1311937): Cleanup when launched.
const base::Feature kAutofillInferCountryCallingCode{
    "AutofillInferCountryCallingCode", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, whenever a form without a country field is parsed, the profile's
// country code is complemented with the predicted country code, used to
// determine the address requirements.
// TODO(crbug.com/1297032): Cleanup when launched.
const base::Feature kAutofillComplementCountryCodeOnImport{
    "AutofillComplementCountryCodeOnImport", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, label inference considers strings entirely made up of  '(', ')'
// and '-' as valid labels.
// TODO(crbug.com/1311937): Cleanup when launched.
const base::Feature kAutofillConsiderPhoneNumberSeparatorsValidLabels{
    "AutofillConsiderPhoneNumberSeparatorsValidLabels",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, local heuristics fall back to the fields placeholder attribute.
const base::Feature kAutofillConsiderPlaceholderForParsing{
    "AutofillConsiderPlaceholderForParsing", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the variation country code is used as the phone number's region,
// instead of defaulting to app locale.
// TODO(crbug.com/1295721): Cleanup when launched.
const base::Feature kAutofillConsiderVariationCountryCodeForPhoneNumbers{
    "AutofillConsiderVariationCountryCodeForPhoneNumbers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, three address profiles are created for testing.
const base::Feature kAutofillCreateDataForTest{
    "AutofillCreateDataForTest", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, we try to fill and import from fields based on available
// heuristic or server suggestions even if the autocomplete attribute is not
// specified by the web standard. This does not affect the moments when the UI
// is shown.
// TODO(crbug.com/1295728): Remove the feature when the experiment is completed.
const base::Feature kAutofillFillAndImportFromMoreFields{
    "AutofillFillAndImportFromMoreFields", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, autofill searches for format strings (like "MM/YY", "MM / YY",
// "MM/YYYY") in the label or placeholder of input elements and uses these
// to fill expiration dates.
// TODO(crbug.com/1326244): Cleanup when launched.
const base::Feature kAutofillFillCreditCardAsPerFormatString{
    "AutofillFillCreditCardAsPerFormatString",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for Autofill filling.
const base::Feature kAutofillDisableFilling{"AutofillDisableFilling",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for Autofill address import.
const base::Feature kAutofillDisableAddressImport{
    "AutofillDisableAddressImport", base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for computing heuristics other than the active ones
// (GetActivePatternSource()).
const base::Feature kAutofillDisableShadowHeuristics{
    "AutofillDisableShadowHeuristics", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, autofill will use the new ranking algorithm for card and
// profile autofill suggestions.
const base::Feature kAutofillEnableRankingFormula{
    "AutofillEnableRankingFormula", base::FEATURE_DISABLED_BY_DEFAULT};
// The half life applied to the use count.
const base::FeatureParam<int> kAutofillRankingFormulaUsageHalfLife{
    &kAutofillEnableRankingFormula, "autofill_ranking_formula_usage_half_life",
    20};
// The boost factor applied to ranking virtual cards.
const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoost{
    &kAutofillEnableRankingFormula,
    "autofill_ranking_formula_virtual_card_boost", 5};
// The half life applied to the virtual card boost.
const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoostHalfLife{
    &kAutofillEnableRankingFormula,
    "autofill_ranking_formula_virtual_card_boost_half_life", 15};

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug.com/1165780): Remove once shared labels are launched.
const base::Feature kAutofillEnableSupportForParsingWithSharedLabels{
    "AutofillEnableSupportForParsingWithSharedLabels",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if Chrome support filling and importing apartment numbers.
// TODO(crbug.com/1153715): Remove once launched.
const base::Feature kAutofillEnableSupportForApartmentNumbers{
    "AutofillEnableSupportForApartmentNumbers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we download server credit cards to the ephemeral
// account-based storage when sync the transport is enabled.
const base::Feature kAutofillEnableAccountWalletStorage {
  "AutofillEnableAccountWalletStorage",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Wallet transport is currently unavailable on ChromeOS.
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Controls whether to detect and fill the augmented phone country code field
// when enabled.
// TODO(crbug.com/1150890) Remove once launched
const base::Feature kAutofillEnableAugmentedPhoneCountryCode{
    "AutofillEnableAugmentedPhoneCountryCode",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables parsing for birthdate fields. Filling is not supported and parsing
// is meant to prevent false positive credit card expiration dates.
// TODO(crbug.com/1306654): Remove once launched.
const base::Feature kAutofillEnableBirthdateParsing{
    "AutofillEnableBirthdateParsing", base::FEATURE_DISABLED_BY_DEFAULT};

// This feature guards the logic for Autofills future compatibility launch of
// birthdates. Currently filling is not supported and this effectively
// disables the birthdate merging logic, reads/writes to the AutofillTable and
// reading/writing from the sync proto.
// TODO(crbug.com/1305940):  Remove once launched.
const base::Feature kAutofillEnableCompatibilitySupportForBirthdates{
    "AutofillEnableCompatibilitySupportForBirthdates",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls if Autofill parses ADDRESS_HOME_DEPENDENT_LOCALITY.
// TODO(crbug.com/1157405): Remove once launched.
const base::Feature kAutofillEnableDependentLocalityParsing{
    "AutofillEnableDependentLocalityParsing",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the augmentation layer for setting-inaccessible fields, which allows
// extending Autofill's address format by additional fields.
// TODO(crbug.com/1300548) Remove when launched.
const base::Feature kAutofillEnableExtendedAddressFormats{
    "AutofillEnableExtendedAddressFormats", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to save the first number in a form with multiple phone
// numbers instead of aborting the import.
// TODO(crbug.com/1167484) Remove once launched
const base::Feature kAutofillEnableImportWhenMultiplePhoneNumbers{
    "AutofillEnableImportWhenMultiplePhoneNumbers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, candidate profiles are temporary stored on import, and merged
// with future candidate profiles, to create an importable profile. This makes
// importing from multi-step input flows possible.
const base::Feature kAutofillEnableMultiStepImports{
    "AutofillEnableMultiStepImports", base::FEATURE_DISABLED_BY_DEFAULT};
// When enabled, imported profiles are stored as multi-step candidates too,
// which enables complementing a recently imported profile during later steps of
// a multi-step input flow.
const base::FeatureParam<bool> kAutofillEnableMultiStepImportComplements{
    &kAutofillEnableMultiStepImports, "enable_multistep_complement", false};
// Configures the TTL of multi-step import candidates.
const base::FeatureParam<base::TimeDelta> kAutofillMultiStepImportCandidateTTL{
    &kAutofillEnableMultiStepImports, "multistep_candidate_ttl",
    base::Minutes(30)};

// When enabled, phone number local heuristics match empty labels when looking
// for composite phone number inputs. E.g. Phone number <input><input>.
const base::Feature kAutofillEnableParsingEmptyPhoneNumberLabels{
    "AutofillEnableParsingEmptyPhoneNumberLabels",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/1156315): Remove once launched.
const base::Feature kAutofillEnableLabelPrecedenceForTurkishAddresses{
    "AutofillEnableLabelPrecedenceForTurkishAddresses",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the parsing of a sequence of fields that follows the pattern of Name,
// Surname.
// TODO(crbug.com/1277480): Remove once launched.
const base::Feature kAutofillEnableNameSurenameParsing{
    "AutofillEnableNameSurenameParsing", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the address profile deduplication logic runs after the browser
// startup, once per chrome version.
const base::Feature kAutofillEnableProfileDeduplication{
    "AutofillEnableProfileDeduplication", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if Autofill supports new structure in names.
// TODO(crbug.com/1098943): Remove once launched.
const base::Feature kAutofillEnableSupportForMoreStructureInNames{
    "AutofillEnableSupportForMoreStructureInNames",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls if Autofill supports new structure in addresses.
// TODO(crbug.com/1098943): Remove once launched.
const base::Feature kAutofillEnableSupportForMoreStructureInAddresses{
    "AutofillEnableSupportForMoreStructureInAddresses",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls if Autofill supports merging subset names.
// TODO(crbug.com/1098943): Remove once launched.
const base::Feature kAutofillEnableSupportForMergingSubsetNames{
    "AutofillEnableSupportForMergingSubsetNames",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether honorific prefix is shown and editable in Autofill Settings
// on Android, iOS and Desktop.
// TODO(crbug.com/1141460): Remove once launched.
const base::Feature kAutofillEnableSupportForHonorificPrefixes{
    "AutofillEnableSupportForHonorificPrefixes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, trunk prefix-related phone number types are added to the
// supported and matching types of |PhoneNumber|. Local heuristics for these
// types are enabled as well.
const base::Feature kAutofillEnableSupportForPhoneNumberTrunkTypes{
    "AutofillEnableSupportForPhoneNumberTrunkTypes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill monitors whether JavaScript modifies autofilled
// credit card expiration dates and tries to fix a specific failure scenario.
// After filling an expiration date "05/2023", some websites try to fix the
// formatting and replace it with "05 / 20" instead of "05 / 23". When this
// experiment is enabled, Chrome replaces the "05 / 20" with "05 / 23".
// TODO(crbug.com/1314360): Remove once launched.
const base::Feature kAutofillRefillModifiedCreditCardExpirationDates{
    "AutofillRefillModifiedCreditCardExpirationDates",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables autofill to function within a FencedFrame, and is disabled by
// default.
// TODO(crbug.com/1294378): Remove once launched.
const base::Feature kAutofillEnableWithinFencedFrame{
    "AutofillEnableWithinFencedFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not all datalist shall be extracted into FormFieldData.
// This feature is enabled in both WebView and WebLayer where all datalists
// instead of only the focused one shall be extracted and sent to Android
// autofill service when the autofill session created.
const base::Feature kAutofillExtractAllDatalists{
    "AutofillExtractAllDatalists", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if type-specific popup widths are used.
// TODO(crbug.com/1250729): Remove once launched.
const base::Feature kAutofillTypeSpecificPopupWidth{
    "AutofillTypeSpecificPopupWidth", base::FEATURE_DISABLED_BY_DEFAULT};

// Autofill uses the local heuristic such that address forms are only filled if
// at least 3 fields are fillable according to local heuristics. Unfortunately,
// the criterion for fillability is only that the field type is unknown. So many
// field types that we don't fill (search term, price, ...) count towards that
// counter, effectively reducing the threshold for some forms.
const base::Feature kAutofillFixFillableFieldTypes{
    "AutofillFixFillableFieldTypes", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the Autofill popup ignores second clicks for a certain period
// (kAutofillIgnoreEarlyClicksOnPopupDuration) after the Autofill popup was
// shown. This is to prevent double clicks accidentally accepting suggestions.
// TODO(crbug/1279268): Remove once launched.
const base::Feature kAutofillIgnoreEarlyClicksOnPopup{
    "AutofillIgnoreEarlyClicksOnPopup", base::FEATURE_DISABLED_BY_DEFAULT};

// The duration for which clicks on the just-shown Autofill popup should be
// ignored if AutofillIgnoreEarlyClicksOnPopup is enabled.
// TODO(crbug/1279268): Remove once launched. Consider also removing
// AutofillPopupItemView::mouse_observed_outside_of_item_.
const base::FeatureParam<base::TimeDelta>
    kAutofillIgnoreEarlyClicksOnPopupDuration{
        &kAutofillIgnoreEarlyClicksOnPopup, "duration",
        base::Milliseconds(500)};

// When enabled, HTML autocomplete values that do not map to any known type, but
// look reasonable (e.g. contain "address") are simply ignored. Without the
// feature, Autofill is disabled on such fields.
const base::Feature kAutofillIgnoreUnmappableAutocompleteValues{
    "AutofillIgnoreUnmappableAutocompleteValues",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, only changed values are highlighted in preview mode.
// TODO(crbug/1248585): Remove when launched.
const base::Feature kAutofillHighlightOnlyChangedValuesInPreviewMode{
    "AutofillHighlightOnlyChangedValuesInPreviewMode",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if a server prediction with a prediction source |OVERRIDE| is
// granted precedence over html type attributes.
// TODO(crbug.com/1170384) Remove once launched
const base::Feature kAutofillServerTypeTakesPrecedence{
    "AutofillServerTypeTakesPrecedence", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill suggestions are displayed in the keyboard accessory
// instead of the regular popup.
const base::Feature kAutofillKeyboardAccessory{
    "AutofillKeyboardAccessory", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will use new logic to strip both prefixes
// and suffixes when setting FormStructure::parseable_name_
extern const base::Feature kAutofillLabelAffixRemoval{
    "AutofillLabelAffixRemoval", base::FEATURE_DISABLED_BY_DEFAULT};

// Enabled a suggestion menu that is aligned to the center of the field.
// TODO(crbug/1248339): Remove once experiment is finished.
extern const base::Feature kAutofillCenterAlignedSuggestions{
    "AutofillCenterAlignedSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the maximum pixels the popup is shifted towards the center.
// TODO(crbug/1248339): Remove once experiment is finished.
extern const base::FeatureParam<int>
    kAutofillMaximumPixelsToMoveSuggestionopupToCenter{
        &kAutofillCenterAlignedSuggestions,
        "maximum_pixels_to_move_the_suggestion_popup__towards_the_fields_"
        "center",
        120};

// Controls the width percentage to move the popup towards the center.
// TODO(crbug/1248339): Remove once experiment is finished.
extern const base::FeatureParam<int>
    kAutofillMaxiumWidthPercentageToMoveSuggestionPopupToCenter{
        &kAutofillCenterAlignedSuggestions,
        "width_percentage_to_shift_the_suggestion_popup_towards_the_center_of_"
        "fields",
        50};

// When enabled, Autofill would not override the field values that were either
// filled by Autofill or on page load.
// TODO(crbug/1275649): Remove once experiment is finished.
extern const base::Feature kAutofillPreventOverridingPrefilledValues{
    "AutofillPreventOverridingPrefilledValues",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, use the parsing patterns from a JSON file for heuristics, rather
// than the hardcoded ones from autofill_regex_constants.cc.
// The specific pattern set is controlled by the
// `kAutofillParsingPatternActiveSource` parameter.
//
// This feature is intended to work with kAutofillPageLanguageDetection.
//
// Enabling this feature is also a prerequisite for emitting shadow metrics.
// TODO(crbug/1121990): Remove once launched.
const base::Feature kAutofillParsingPatternProvider{
    "AutofillParsingPatternProvider", base::FEATURE_DISABLED_BY_DEFAULT};

// The specific pattern set is controlled by the `kAutofillParsingPatternActive`
// parameter. One of "legacy", "default", "experimental", "nextgen". All other
// values are equivalent to "default".
// TODO(crbug/1248339): Remove once experiment is finished.
const base::FeatureParam<std::string> kAutofillParsingPatternActiveSource{
    &kAutofillParsingPatternProvider, "prediction_source", "default"};

// Enables detection of language from Translate.
// TODO(crbug/1150895): Cleanup when launched.
const base::Feature kAutofillPageLanguageDetection{
    "AutofillPageLanguageDetection", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, AutofillManager::ParseForm() isn't called synchronously.
// Instead, all incoming events parse the form asynchronously and proceed
// afterwards.
// TODO(crbug.com/1309848) Remove once launched.
const base::Feature kAutofillParseAsync{"AutofillParseAsync",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If the feature is enabled, FormTracker's probable-form-submission detection
// is disabled and replaced with browser-side detection.
// TODO(crbug/1117451): Remove once it works.
const base::Feature kAutofillProbableFormSubmissionInBrowser{
    "AutofillProbableFormSubmissionInBrowser",
    base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1101280): Remove once feature is tested.
const base::Feature kAutofillProfileImportFromUnfocusableFields{
    "AutofillProfileImportFromUnfocusableFields",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If we observe a sequence of fields of (street address, address line 2), these
// get rationalized to (address line 1, address line 2).
// TODO(crbug.com/1326425): Remove once feature is lanuched.
const base::Feature kAutofillRationalizeStreetAddressAndAddressLine{
    "AutofillRationalizeStreetAddressAndAddressLine",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Prevents Autofill from importing setting-inaccessible field types on profile
// import.
// TODO(crbug.com/1299435): Cleanup when launched.
const base::Feature kAutofillRemoveInaccessibleProfileValues{
    "AutofillRemoveInaccessibleProfileValues",
    base::FEATURE_DISABLED_BY_DEFAULT};
// If enabled, setting-inaccessible field types are additionally removed from
// existing profiles on startup. These can exit in profile imported prior to
// AutofillRemoveInaccessibleProfileValues.
const base::FeatureParam<bool>
    kAutofillRemoveInaccessibleProfileValuesOnStartup{
        &kAutofillRemoveInaccessibleProfileValues,
        "remove_inaccessible_fields_on_startup", false};

// If enabled, invalid phone numbers are removed on profile import, rather than
// invalidating the entire profile.
// TODO(crbug.com/1298424): Cleanup when launched.
const base::Feature kAutofillRemoveInvalidPhoneNumberOnImport{
    "AutofillRemoveInvalidPhoneNumberOnImport",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not overall prediction are retrieved from the cache.
const base::Feature kAutofillRetrieveOverallPredictionsFromCache{
    "AutofillRetrieveOverallPredictionsFromCache",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether UPI/VPA values will be saved and filled into payment forms.
const base::Feature kAutofillSaveAndFillVPA{"AutofillSaveAndFillVPA",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables creating a new form section when an unstructured name input
// containing a |NAME_LAST| field is encountered after a structured name input.
const base::Feature kAutofillSectionUponRedundantNameInfo{
    "AutofillSectionUponRedundantNameInfo", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls non-default Autofill API predictions. See crbug.com/1331322.
const base::Feature kAutofillServerBehaviors{"AutofillServerBehaviors",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
// Chrome doesn't need to know the meaning of the value. Chrome only needs to
// forward it to the Autofill API, to let the server know which group the client
// belongs to.
const base::FeatureParam<int> kAutofillServerBehaviorsParam{
    &kAutofillServerBehaviors, "server_prediction_source", 0};

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
const base::Feature kAutofillServerCommunication{
    "AutofillServerCommunication", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether Autofill may fill across origins as part of the
// AutofillAcrossIframes experiment.
// TODO(crbug.com/1304721): Clean up when launched.
const base::Feature kAutofillSharedAutofill{"AutofillSharedAutofill",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
// Relaxes the conditions under which a field is safe to fill.
// See FormForest::GetRendererFormsOfBrowserForm() for details.
const base::FeatureParam<bool> kAutofillSharedAutofillRelaxedParam{
    &kAutofillSharedAutofill, "relax_shared_autofill", false};

// Controls whether Manual fallbacks would be shown in the context menu for
// filling. Used only in Desktop.
// TODO(crbug.com/1326895): Clean up when launched.
const base::Feature kAutofillShowManualFallbackInContextMenu{
    "AutofillShowManualFallbackInContextMenu",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
const base::Feature kAutofillShowTypePredictions{
    "AutofillShowTypePredictions", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows silent profile updates even when the profile import requirements are
// not met.
const base::Feature kAutofillSilentProfileUpdateForInsufficientImport{
    "AutofillSilentProfileUpdateForInsufficientImport",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether inferred label is considered for comparing in
// FormFieldData.SimilarFieldAs.
// TODO(crbug.com/1211834): The experiment seems dead; remove?
const base::Feature kAutofillSkipComparingInferredLabels{
    "AutofillSkipComparingInferredLabels", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill should search prefixes of all words/tokens when
// filtering profiles, or only on prefixes of the whole string.
const base::Feature kAutofillTokenPrefixMatching{
    "AutofillTokenPrefixMatching", base::FEATURE_DISABLED_BY_DEFAULT};

// Autofill upload throttling is used for testing.
const base::Feature kAutofillUploadThrottling{"AutofillUploadThrottling",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to use the AutofillUseAlternativeStateNameMap for filling
// of state selection fields, comparison of profiles and sending state votes to
// the server.
// TODO(crbug.com/1143516): Remove the feature when the experiment is completed.
const base::Feature kAutofillUseAlternativeStateNameMap{
    "AutofillUseAlternativeStateNameMap", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether suggestions' labels use the improved label disambiguation
// format.
const base::Feature kAutofillUseImprovedLabelDisambiguation{
    "AutofillUseImprovedLabelDisambiguation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to use the same icon for the settings section in the popup
// footer.
const base::Feature kAutofillUseConsistentPopupSettingsIcons{
    "AutofillUseConsistentPopupSettingsIcons",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to use the combined heuristic and the autocomplete section
// implementation for section splitting or not. See https://crbug.com/1076175.
const base::Feature kAutofillUseNewSectioningMethod{
    "AutofillUseNewSectioningMethod", base::FEATURE_DISABLED_BY_DEFAULT};

// Introduces various visual improvements of the Autofill suggestion UI that is
// also used for the password manager.
const base::Feature kAutofillVisualImprovementsForSuggestionUi{
    "AutofillVisualImprovementsForSuggestionUi",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls an ablation study in which autofill for addresses and payment data
// can be suppressed.
const base::Feature kAutofillEnableAblationStudy{
    "AutofillEnableAblationStudy", base::FEATURE_DISABLED_BY_DEFAULT};
// The following parameters are only effective if the study is enabled.
const base::FeatureParam<bool> kAutofillAblationStudyEnabledForAddressesParam{
    &kAutofillEnableAblationStudy, "enabled_for_addresses", false};
const base::FeatureParam<bool> kAutofillAblationStudyEnabledForPaymentsParam{
    &kAutofillEnableAblationStudy, "enabled_for_payments", false};
// The ratio of ablation_weight_per_mille / 1000 determines the chance of
// autofill being disabled on a given combination of site * day * browser
// session.
const base::FeatureParam<int> kAutofillAblationStudyAblationWeightPerMilleParam{
    &kAutofillEnableAblationStudy, "ablation_weight_per_mille", 10};

#if BUILDFLAG(IS_ANDROID)
// Controls whether the Autofill manual fallback for Addresses and Payments is
// present on Android.
const base::Feature kAutofillManualFallbackAndroid{
    "AutofillManualFallbackAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to use modernized style for the Autofill dropdown.
const base::Feature kAutofillRefreshStyleAndroid{
    "AutofillRefreshStyleAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the touch to fill surface is shown for credit cards on
// Android.
const base::Feature kAutofillTouchToFillForCreditCardsAndroid{
    "AutofillTouchToFillForCreditCardsAndroid",
    base::FEATURE_DISABLED_BY_DEFAULT};

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const base::Feature kAutofillUseMobileLabelDisambiguation{
    "AutofillUseMobileLabelDisambiguation", base::FEATURE_DISABLED_BY_DEFAULT};
const char kAutofillUseMobileLabelDisambiguationParameterName[] = "variant";
const char kAutofillUseMobileLabelDisambiguationParameterShowAll[] = "show-all";
const char kAutofillUseMobileLabelDisambiguationParameterShowOne[] = "show-one";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
bool IsAutofillManualFallbackEnabled() {
  return base::FeatureList::IsEnabled(kAutofillKeyboardAccessory) &&
         base::FeatureList::IsEnabled(kAutofillManualFallbackAndroid);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill::features
