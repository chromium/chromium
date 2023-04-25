// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace autofill::features {

// Controls whether to flatten and fill cross-iframe forms.
// TODO(crbug.com/1187842) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframes,
             "AutofillAcrossIframes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, address data will be verified and autocorrected in the
// save/update prompt before saving an address profile. Relevant only if the
// AutofillAddressProfileSavePrompt feature is enabled.
BASE_FEATURE(kAutofillAddressProfileSavePromptAddressVerificationSupport,
             "AutofillAddressProfileSavePromptAddressVerificationSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the heuristic parser to detect unfillable numeric types in field labels
// and grant the heuristic precedence over non-override server predictions.
BASE_FEATURE(kAutofillGivePrecedenceToNumericQuantities,
             "AutofillGivePrecedenceToNumericQuantities",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if `kAccount` profiles are loaded from AutofillTable and
// consequently suggested for filling.
// TODO(crbug.com/1348294): Remove once launched.
BASE_FEATURE(kAutofillAccountProfilesUnionView,
             "AutofillAccountProfilesUnionView",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Account profiles are not considered for regular updates on import, but if
// this parameter is enabled, they are considered for silent updates.
const base::FeatureParam<bool> kAutofillEnableSilentUpdatesForAccountProfiles{
    &kAutofillAccountProfilesUnionView, "enable_silent_updates", true};

// When enabled, creating new kAccount profiles becomes possible for eligible
// users. Moreover, users are prompted to migrate existing kLocalOrSyncable
// profiles to the kAccount storage.
// TODO(crbug.com/1423319): Remove once launched.
BASE_FEATURE(kAutofillAccountProfileStorage,
             "AutofillAccountProfileStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Determines if users located in an unsupported country (based on GeoIP) are
// eligible to write to the account storage.
const base::FeatureParam<bool> kAutofillAccountProfileStorageFromUnsupportedIPs{
    &kAutofillAccountProfileStorage, "allow_writes_from_unsupported_ips", true};

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, address profile save problem will contain a dropdown for
// assigning a nickname to the address profile. Relevant only if the
// AutofillAddressProfileSavePrompt feature is enabled.
BASE_FEATURE(kAutofillAddressProfileSavePromptNicknameSupport,
             "AutofillAddressProfileSavePromptNicknameSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// By default, AutofillAgent and, if |kAutofillProbableFormSubmissionInBrowser|
// is enabled, also ContentAutofillDriver omit duplicate form submissions, even
// though the form's data may have changed substantially. If enabled, the
// below feature allows duplicate form submissions.
// TODO(crbug/1117451): Remove once the form-submission experiment is over.
BASE_FEATURE(kAutofillAllowDuplicateFormSubmissions,
             "AutofillAllowDuplicateFormSubmissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the two most recent address forms and the most recent credit card
// forms, which were submitted on the same origin, are associated with each
// other. The association only happens if at most `kAutofillAssociateFormsTTL`
// time passes between all submissions.
BASE_FEATURE(kAutofillAssociateForms,
             "AutofillAssociateForms",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kAutofillAssociateFormsTTL{
    &kAutofillAssociateForms, "associate_forms_ttl", base::Minutes(5)};

// If enabled, the country calling code for nationally formatted phone numbers
// is inferred from the profile's country, if available.
// TODO(crbug.com/1311937): Cleanup when launched.
BASE_FEATURE(kAutofillInferCountryCallingCode,
             "AutofillInferCountryCallingCode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, label inference considers strings entirely made up of  '(', ')'
// and '-' as valid labels.
// TODO(crbug.com/1311937): Cleanup when launched.
BASE_FEATURE(kAutofillConsiderPhoneNumberSeparatorsValidLabels,
             "AutofillConsiderPhoneNumberSeparatorsValidLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the last blur votes per form signature are sent not the first
// ones.
// TODO(crbug.com/1383502): Cleanup when this has proven on stable.
BASE_FEATURE(kAutofillDelayBlurVotes,
             "AutofillDelayBlurVotes",
             base::FEATURE_ENABLED_BY_DEFAULT);

// FormStructure::RetrieveFromCache used to preserve an AutofillField's
// is_autofilled from the cache of previously parsed forms. This makes little
// sense because the renderer sends us the autofill state and has the most
// recent information. Dropping the old behavior should not make any difference
// but to be sure, this is gated by a finch experiment.
// TODO(crbug.com/1373362) Cleanup when launched.
BASE_FEATURE(kAutofillDontPreserveAutofillState,
             "AutofillDontPreserveAutofillState",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, checking whether a form has disappeared after an Ajax response is
// delayed because subsequent Ajax responses may restore the form. If disabled,
// the check happens right after a successful Ajax response.
BASE_FEATURE(kAutofillDeferSubmissionClassificationAfterAjax,
             "AutofillDeferSubmissionClassificationAfterAjax",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, server/heuristic predictions take precedence over an unrecognized
// autocomplete attribute. Depending on the parameters, these fields are then
// filled or imported from. Independently of any parameters, suggestions are
// suppressed for such fields.
// Predicting a type for a field can influence other fields due to
// rationalization and sectioning. This also affects metrics like
// Autofill.FieldFillingStats, which rely on the types.
// When only the importing part of this feature is enabled, only the importing
// metrics are reliable.
// TODO(crbug.com/1295728): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillFillAndImportFromMoreFields,
             "AutofillFillAndImportFromMoreFields",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kAutofillFillAutocompleteUnrecognized{
    &kAutofillFillAndImportFromMoreFields, "fill_unrecognized_autocomplete",
    false};
const base::FeatureParam<bool> kAutofillImportFromAutocompleteUnrecognized{
    &kAutofillFillAndImportFromMoreFields,
    "import_from_unrecognized_autocomplete", false};

// Kill switch for Autofill filling.
BASE_FEATURE(kAutofillDisableFilling,
             "AutofillDisableFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for Autofill address import.
BASE_FEATURE(kAutofillDisableAddressImport,
             "AutofillDisableAddressImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for computing heuristics other than the active ones
// (GetActivePatternSource()).
BASE_FEATURE(kAutofillDisableShadowHeuristics,
             "AutofillDisableShadowHeuristics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, autofill will use the new ranking algorithm for address profile
// autofill suggestions.
BASE_FEATURE(kAutofillEnableRankingFormulaAddressProfiles,
             "AutofillEnableRankingFormulaAddressProfiles",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The half life applied to the use count of profiles in the ranking formula.
const base::FeatureParam<int>
    kAutofillRankingFormulaAddressProfilesUsageHalfLife{
        &kAutofillEnableRankingFormulaAddressProfiles,
        "autofill_ranking_formula_address_profiles_usage_half_life", 20};

// When enabled, autofill will use the new ranking algorithm for credit card
// autofill suggestions.
BASE_FEATURE(kAutofillEnableRankingFormulaCreditCards,
             "AutofillEnableRankingFormulaCreditCards",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The half life applied to the use count.
const base::FeatureParam<int> kAutofillRankingFormulaCreditCardsUsageHalfLife{
    &kAutofillEnableRankingFormulaCreditCards,
    "autofill_ranking_formula_credit_cards_usage_half_life", 20};

// The boost factor applied to ranking virtual cards.
const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoost{
    &kAutofillEnableRankingFormulaCreditCards,
    "autofill_ranking_formula_virtual_card_boost", 5};
// The half life applied to the virtual card boost.
const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoostHalfLife{
    &kAutofillEnableRankingFormulaCreditCards,
    "autofill_ranking_formula_virtual_card_boost_half_life", 15};

// When enabled, autofill will fill <selectmenu> elements.
// TODO(crbug.com/1427153) Remove once autofilling <selectmenu> is launched.
BASE_FEATURE(kAutofillEnableSelectMenu,
             "AutofillEnableSelectMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug.com/1165780): Remove once shared labels are launched.
BASE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels,
             "AutofillEnableSupportForParsingWithSharedLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing apartment numbers.
// TODO(crbug.com/1153715): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForApartmentNumbers,
             "AutofillEnableSupportForApartmentNumbers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables parsing for birthdate fields. Filling is not supported and parsing
// is meant to prevent false positive credit card expiration dates.
// TODO(crbug.com/1306654): Remove once launched.
BASE_FEATURE(kAutofillEnableBirthdateParsing,
             "AutofillEnableBirthdateParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Autofill parses ADDRESS_HOME_DEPENDENT_LOCALITY.
// TODO(crbug.com/1157405): Remove once launched.
BASE_FEATURE(kAutofillEnableDependentLocalityParsing,
             "AutofillEnableDependentLocalityParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Autofill emits form issues to devtools.
BASE_FEATURE(kAutofillEnableDevtoolsIssues,
             "AutofillEnableDevtoolsIssues",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to save the first number in a form with multiple phone
// numbers instead of aborting the import.
// TODO(crbug.com/1167484) Remove once launched.
BASE_FEATURE(kAutofillEnableImportWhenMultiplePhoneNumbers,
             "AutofillEnableImportWhenMultiplePhoneNumbers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, phone number local heuristics match empty labels when looking
// for composite phone number inputs. E.g. Phone number <input><input>.
BASE_FEATURE(kAutofillEnableParsingEmptyPhoneNumberLabels,
             "AutofillEnableParsingEmptyPhoneNumberLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/1156315): Remove once launched.
BASE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses,
             "AutofillEnableLabelPrecedenceForTurkishAddresses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the address profile deduplication logic runs after the browser
// startup, once per chrome version.
BASE_FEATURE(kAutofillEnableProfileDeduplication,
             "AutofillEnableProfileDeduplication",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls if Autofill supports merging subset names.
// TODO(crbug.com/1098943): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForMergingSubsetNames,
             "AutofillEnableSupportForMergingSubsetNames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether honorific prefix is shown and editable in Autofill Settings
// on Android, iOS and Desktop.
// TODO(crbug.com/1141460): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForHonorificPrefixes,
             "AutofillEnableSupportForHonorificPrefixes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, trunk prefix-related phone number types are added to the
// supported and matching types of |PhoneNumber|. Local heuristics for these
// types are enabled as well.
BASE_FEATURE(kAutofillEnableSupportForPhoneNumberTrunkTypes,
             "AutofillEnableSupportForPhoneNumberTrunkTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables autofill to function within a FencedFrame, and is enabled by
// default as part of FencedFramesAPIChanges blink experiment.
// This flag can be used via Finch to disable Autofill in the
// FencedFramesAPIChanges blink experiment without affecting the other
// features included in the experiment.
// TODO(crbug.com/1294378): Remove once launched.
BASE_FEATURE(kAutofillEnableWithinFencedFrame,
             "AutofillEnableWithinFencedFrame",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether or not all datalist shall be extracted into FormFieldData.
// This feature is enabled in both WebView and WebLayer where all datalists
// instead of only the focused one shall be extracted and sent to Android
// autofill service when the autofill session created.
BASE_FEATURE(kAutofillExtractAllDatalists,
             "AutofillExtractAllDatalists",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support to submit feedback on Autofill. Used only in Desktop.
BASE_FEATURE(kAutofillFeedback,
             "AutofillFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes is_autofilled = true cached only after filling and not previewing.
BASE_FEATURE(kAutofillOnlyCacheIsAutofilledOnFill,
             "AutofillOnlyCacheIsAutofilledOnFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables converging towards the longer or shorter street address in profile
// merging.
BASE_FEATURE(kAutofillConvergeToExtremeLengthStreetAddress,
             "AutofillConvergeToExtremeLengthStreetAddress",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kAutofillConvergeToLonger{
    &kAutofillConvergeToExtremeLengthStreetAddress, "converge_to_longer", true};

BASE_FEATURE(kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete,
             "AutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<PrecedenceOverAutocompleteScope>::Option
    kPrecedenceOverAutocompleteScope[] = {
        {PrecedenceOverAutocompleteScope::kNone, "none"},
        {PrecedenceOverAutocompleteScope::kAddressLine1And2,
         "address_line_1_and_2"},
        {PrecedenceOverAutocompleteScope::kRecognized, "recognized"},
        {PrecedenceOverAutocompleteScope::kSpecified, "specified"}};

const base::FeatureParam<PrecedenceOverAutocompleteScope>
    kAutofillHeuristicPrecedenceScopeOverAutocomplete{
        &kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete,
        "AutofillHeuristicPrecedenceOverAutocompleteScope",
        PrecedenceOverAutocompleteScope::kAddressLine1And2,
        &kPrecedenceOverAutocompleteScope};

const base::FeatureParam<PrecedenceOverAutocompleteScope>
    kAutofillServerPrecedenceScopeOverAutocomplete{
        &kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete,
        "AutofillServerPrecedenceOverAutocompleteScope",
        PrecedenceOverAutocompleteScope::kNone,
        &kPrecedenceOverAutocompleteScope};

// When enabled, HTML autocomplete values that do not map to any known type, but
// look reasonable (e.g. contain "address") are simply ignored. Without the
// feature, Autofill is disabled on such fields.
BASE_FEATURE(kAutofillIgnoreUnmappableAutocompleteValues,
             "AutofillIgnoreUnmappableAutocompleteValues",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, only changed values are highlighted in preview mode.
// TODO(crbug/1248585): Remove when launched.
BASE_FEATURE(kAutofillHighlightOnlyChangedValuesInPreviewMode,
             "AutofillHighlightOnlyChangedValuesInPreviewMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will use new logic to strip both prefixes
// and suffixes when setting FormStructure::parseable_name_
BASE_FEATURE(kAutofillLabelAffixRemoval,
             "AutofillLabelAffixRemoval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill would not override the field values that were either
// filled by Autofill or on page load.
// TODO(crbug/1275649): Remove once experiment is finished.
BASE_FEATURE(kAutofillPreventOverridingPrefilledValues,
             "AutofillPreventOverridingPrefilledValues",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, use the parsing patterns from a JSON file for heuristics, rather
// than the hardcoded ones from autofill_regex_constants.cc.
// The specific pattern set is controlled by the
// `kAutofillParsingPatternActiveSource` parameter.
//
// This feature is intended to work with kAutofillPageLanguageDetection.
//
// Enabling this feature is also a prerequisite for emitting shadow metrics.
// TODO(crbug/1121990): Remove once launched.
BASE_FEATURE(kAutofillParsingPatternProvider,
             "AutofillParsingPatternProvider",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The specific pattern set is controlled by the `kAutofillParsingPatternActive`
// parameter. One of "legacy", "default", "experimental", "nextgen". All other
// values are equivalent to "default".
// TODO(crbug/1248339): Remove once experiment is finished.
const base::FeatureParam<std::string> kAutofillParsingPatternActiveSource{
    &kAutofillParsingPatternProvider, "prediction_source", "default"};

// Enables detection of language from Translate.
// TODO(crbug/1150895): Cleanup when launched.
BASE_FEATURE(kAutofillPageLanguageDetection,
             "AutofillPageLanguageDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillManager::ParseForm() isn't called synchronously.
// Instead, all incoming events parse the form asynchronously and proceed
// afterwards.
// TODO(crbug.com/1309848) Remove once launched.
BASE_FEATURE(kAutofillParseAsync,
             "AutofillParseAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, local heuristics fall back to interpreting the fields' name as an
// autocomplete type.
// TODO(crbug.com/1345879) Remove once launched.
BASE_FEATURE(kAutofillParseNameAsAutocompleteType,
             "AutofillParseNameAsAutocompleteType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the placeholder is not used as a fallback during label inference.
// Instead, local heuristics treat it as a separate source in addition to the
// label. The placeholder is matched against the same regex as the label.
// Since placeholders are often used as example values, this should allow us to
// extract a more appropriate label instead.
// TODO(crbug.com/1317961): Remove once launched.
BASE_FEATURE(kAutofillAlwaysParsePlaceholders,
             "AutofillAlwaysParsePlaceholders",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the same 500ms threshold will be applied for accepting keyboard
// enter strokes that is already applied to mouse and gesture events.
// It will also be applied to tap events on popup menus on Android (but not the
// keyboard accessory, at the screen is outside of the render surface).
// TODO(crbug.com/1418364): Remove once launched.
BASE_FEATURE(kAutofillPopupUseThresholdForKeyboardAndMobileAccept,
             "AutofillPopupUseThresholdForKeyboardAndMobileAccept",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, FormTracker's probable-form-submission detection
// is disabled and replaced with browser-side detection.
// TODO(crbug/1117451): Remove once it works.
BASE_FEATURE(kAutofillProbableFormSubmissionInBrowser,
             "AutofillProbableFormSubmissionInBrowser",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If we observe a sequence of fields of (street address, house number), these
// get rationalized to (street name, house number).
// TODO(crbug.com/1326425): Remove once feature is launched.
BASE_FEATURE(kAutofillRationalizeStreetAddressAndHouseNumber,
             "AutofillRationalizeStreetAddressAndHouseNumber",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Removes setting-inaccessible field types from existing profiles on startup.
// TODO(crbug.com/1300548): Cleanup when launched.
BASE_FEATURE(kAutofillRemoveInaccessibleProfileValuesOnStartup,
             "AutofillRemoveInaccessibleProfileValuesOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Requires a profile to have non-empty full name to import it from a form.
// TODO(crbug.com/1413205): Cleanup when launched.
BASE_FEATURE(kAutofillRequireNameForProfileImport,
             "AutofillRequireNameForProfileImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether UPI/VPA values will be saved and filled into payment forms.
BASE_FEATURE(kAutofillSaveAndFillVPA,
             "AutofillSaveAndFillVPA",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls non-default Autofill API predictions. See crbug.com/1331322.
BASE_FEATURE(kAutofillServerBehaviors,
             "AutofillServerBehaviors",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Chrome doesn't need to know the meaning of the value. Chrome only needs to
// forward it to the Autofill API, to let the server know which group the client
// belongs to.
const base::FeatureParam<int> kAutofillServerBehaviorsParam{
    &kAutofillServerBehaviors, "server_prediction_source", 0};

// Controls whether Autofill may fill across origins as part of the
// AutofillAcrossIframes experiment.
// TODO(crbug.com/1304721): Clean up when launched.
BASE_FEATURE(kAutofillSharedAutofill,
             "AutofillSharedAutofill",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Relaxes the conditions under which a field is safe to fill.
// See FormForest::GetRendererFormsOfBrowserForm() for details.
const base::FeatureParam<bool> kAutofillSharedAutofillRelaxedParam{
    &kAutofillSharedAutofill, "relax_shared_autofill", false};

// Controls whether to offer a delete button for Autocomplete entries in the
// Autofill popup.
BASE_FEATURE(kAutofillShowAutocompleteDeleteButton,
             "AutofillShowAutocompleteDeleteButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Manual fallbacks would be shown in the context menu for
// filling. Used only in Desktop.
// TODO(crbug.com/1326895): Clean up when launched.
BASE_FEATURE(kAutofillShowManualFallbackInContextMenu,
             "AutofillShowManualFallbackInContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows silent profile updates even when the profile import requirements are
// not met.
BASE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport,
             "AutofillSilentProfileUpdateForInsufficientImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether inferred label is considered for comparing in
// FormFieldData.SimilarFieldAs.
// TODO(crbug.com/1211834): The experiment seems dead; remove?
BASE_FEATURE(kAutofillSkipComparingInferredLabels,
             "AutofillSkipComparingInferredLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Stronger conditions for splitting credit card numbers across multiple fields.
// TODO(crbug.com/1419578): Remove when launched.
BASE_FEATURE(kAutofillSplitCreditCardNumbersCautiously,
             "AutofillSplitCreditCardNumbersCautiously",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Autofill should search prefixes of all words/tokens when
// filtering profiles, or only on prefixes of the whole string.
BASE_FEATURE(kAutofillTokenPrefixMatching,
             "AutofillTokenPrefixMatching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the AutofillUseAlternativeStateNameMap for filling
// of state selection fields, comparison of profiles and sending state votes to
// the server.
// TODO(crbug.com/1143516): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillUseAlternativeStateNameMap,
             "AutofillUseAlternativeStateNameMap",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether suggestions' labels use the improved label disambiguation
// format.
BASE_FEATURE(kAutofillUseImprovedLabelDisambiguation,
             "AutofillUseImprovedLabelDisambiguation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the combined heuristic and the autocomplete section
// implementation for section splitting or not. See https://crbug.com/1076175.
BASE_FEATURE(kAutofillUseNewSectioningMethod,
             "AutofillUseNewSectioningMethod",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the newest, parameterized sectioning algorithm.
// Use together with `kAutofillRefillByFormRendererId`.
// TODO(crbug.com/1153539): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillUseParameterizedSectioning,
             "AutofillUseParameterizedSectioning",
             base::FEATURE_DISABLED_BY_DEFAULT);
// In the experiment, we test different combinations of these parameters.
const base::FeatureParam<bool> kAutofillSectioningModeIgnoreAutocomplete{
    &kAutofillUseParameterizedSectioning, "ignore_autocomplete", false};
const base::FeatureParam<bool> kAutofillSectioningModeCreateGaps{
    &kAutofillUseParameterizedSectioning, "create_gaps", false};
const base::FeatureParam<bool> kAutofillSectioningModeExpand{
    &kAutofillUseParameterizedSectioning, "expand_assigned_sections", false};
const base::FeatureParam<bool>
    kAutofillSectioningModeExpandOverUnfocusableFields{
        &kAutofillUseParameterizedSectioning, "expand_over_unfocsuable_fields",
        false};

// Controls whether to use form renderer IDs to find the form which contains the
// field that was last interacted with in
// `AutofillAgent::TriggerRefillIfNeeded()`.
// TODO(crbug.com/1360988): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillRefillByFormRendererId,
             "AutofillRefillByFormRendererId",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls an ablation study in which autofill for addresses and payment data
// can be suppressed.
BASE_FEATURE(kAutofillEnableAblationStudy,
             "AutofillEnableAblationStudy",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

// If enabled, crowdsourcing considers not just the value V but also the human
// readable text HRT of an <option value="V">HRT</option> for voting.
// TODO(crbug.com/1395740). This is a kill switch, remove once the feature has
// settled.
BASE_FEATURE(kAutofillVoteForSelectOptionValues,
             "AutofillVoteForSelectOptionValues",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls autofill popup style, if enabled it becomes more prominent,
// i.e. its shadow becomes more emphasized, position is also updated.
// TODO(crbug.com/1354136): Remove once the experiment is over.
BASE_FEATURE(kAutofillMoreProminentPopup,
             "AutofillMoreProminentPopup",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillMoreProminentPopupMaxOffsetToCenterParam{
    &kAutofillMoreProminentPopup, "max_offset_to_center_px", 92};

// If enabled, we will log information of field types and autofill and forms
// with sample rates according to Autofill FormSummary/FieldInfo UKM schema:
// https://docs.google.com/document/d/1ZH0JbL6bES3cD4KqZWsGR6n8I-rhnkx6no6nQOgYq5w/.
BASE_FEATURE(kAutofillLogUKMEventsWithSampleRate,
             "AutofillLogUKMEventsWithSampleRate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Autofill is experimenting with an updated set of country specific rules.
// Controls whether we use the current country-specific address import field
// requirements or the updated ones.
BASE_FEATURE(kAutofillUseUpdatedRequiredFieldsForAddressImport,
             "AutofillUseUpdatedRequiredFieldsForAddressImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether virtual card suggestions are shown on the touch to fill
// surface for credit cards on Android.
BASE_FEATURE(kAutofillVirtualCardsOnTouchToFillAndroid,
             "AutofillVirtualCardsOnTouchToFillAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, Autofill suggestions are displayed in the keyboard accessory
// instead of the regular popup.
BASE_FEATURE(kAutofillKeyboardAccessory,
             "AutofillKeyboardAccessory",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the Autofill manual fallback for Addresses and Payments is
// present on Android.
BASE_FEATURE(kAutofillManualFallbackAndroid,
             "AutofillManualFallbackAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the touch to fill surface is shown for credit cards on
// Android.
BASE_FEATURE(kAutofillTouchToFillForCreditCardsAndroid,
             "AutofillTouchToFillForCreditCardsAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kAutofillUseMobileLabelDisambiguation,
             "AutofillUseMobileLabelDisambiguation",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

namespace test {

// Controls whether autofill activates on non-HTTP(S) pages. Useful for
// automated tests with data URLS in cases where it's too difficult to use the
// embedded test server. Generally avoid using.
BASE_FEATURE(kAutofillAllowNonHttpActivation,
             "AutofillAllowNonHttpActivation",
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
             "AutofillCapturedSiteTestsMetricsScraper",
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

// If enabled, Autofill will not apply updates to address profiles based on data
// extracted from submitted forms. This feature is mostly for debugging and
// testing purposes and is not supposed to be launched.
BASE_FEATURE(kAutofillDisableProfileUpdates,
             "AutofillDisableProfileUpdates",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill will not apply silent updates to the structure of
// addresses and names. This feature is mostly for debugging and testing
// purposes and is not supposed to be launched.
BASE_FEATURE(kAutofillDisableSilentProfileUpdates,
             "AutofillDisableSilentProfileUpdates",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, any new profiles created from the settings are of type kAccount.
// TODO(crbug.com/1348294): Remove once the migration UI exists.
BASE_FEATURE(kAutofillCreateAccountProfilesFromSettings,
             "AutofillCreateAccountProfilesFromSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables logging the content of chrome://autofill-internals to the terminal.
BASE_FEATURE(kAutofillLogToTerminal,
             "AutofillLogToTerminal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
BASE_FEATURE(kAutofillServerCommunication,
             "AutofillServerCommunication",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
BASE_FEATURE(kAutofillShowTypePredictions,
             "AutofillShowTypePredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Autofill upload throttling is used for testing.
BASE_FEATURE(kAutofillUploadThrottling,
             "AutofillUploadThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace test

}  // namespace autofill::features
