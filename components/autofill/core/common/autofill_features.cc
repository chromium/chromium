// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace autofill::features {

// Controls whether to flatten and fill cross-iframe forms on iOS.
// TODO(crbug.com/1441921) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframesIos,
             "AutofillAcrossIframesIos",
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

// When enabled, creating new kAccount profiles becomes possible for eligible
// users. Moreover, users are prompted to migrate existing kLocalOrSyncable
// profiles to the kAccount storage.
// TODO(crbug.com/1423319): Remove once launched.
BASE_FEATURE(kAutofillAccountProfileStorage,
             "AutofillAccountProfileStorage",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If enabled, contenteditables are extracted and filled.
// TODO(crbug.com/1490372): Cleanup when launched.
BASE_FEATURE(kAutofillContentEditables,
             "AutofillContentEditables",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Crowdsourcing already prefers PHONE_HOME_CITY_AND_NUMBER over
// PHONE_HOME_WHOLE_NUMBER. With this feature, local heuristics do the same.
// TODO(crbug.com/1474308): Clean up when launched.
BASE_FEATURE(kAutofillDefaultToCityAndNumber,
             "AutofillDefaultToCityAndNumber",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
// autocomplete attribute. Suggestions are suppressed for such fields and they
// won't be considered for filling or importing. The fields do however affect
// rationalization and sectioning, and non-(key and quality) metrics.
// When `kAutofillFillAndImportFromMoreFields` is enabled, fields with
// unrecognized autocomplete attribute are considered for import.
// TODO(crbug.com/1446318): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillPredictionsForAutocompleteUnrecognized,
             "AutofillPredictionsForAutocompleteUnrecognized",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kAutofillImportFromAutocompleteUnrecognized{
    &kAutofillPredictionsForAutocompleteUnrecognized,
    "import_from_autocomplete_unrecognized", false};

// When enabled, an entry is added to the context menu of ac=unrecognized fields
// which allows triggering Autofill suggestions. Selecting such a suggestion
// fills all address fields in the field's section, independently of the
// autocomplete attribute.
// TODO(crbug.com/1446318): Remove when launched.
BASE_FEATURE(kAutofillFallbackForAutocompleteUnrecognized,
             "AutofillFallbackForAutocompleteUnrecognized",
             base::FEATURE_DISABLED_BY_DEFAULT);
// If true, the context menu entry is shown for all address fields.
const base::FeatureParam<bool>
    kAutofillFallForAutocompleteUnrecognizedOnAllAddressField{
        &kAutofillFallbackForAutocompleteUnrecognized,
        "show_on_all_address_fields", false};

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

// When enabled, autofill will fill <selectlist> elements.
// TODO(crbug.com/1427153) Remove once autofilling <selectlist> is launched.
BASE_FEATURE(kAutofillEnableSelectList,
             "AutofillEnableSelectList",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing between streets.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForBetweenStreets,
             "AutofillEnableSupportForBetweenStreets",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome supports filling and importing administrative area
// level 2. A sub-division of a state, e.g. a Municipio in Brazil or Mexico.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForAdminLevel2,
             "AutofillEnableSupportForAdminLevel2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing address overflow fields.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForAddressOverflow,
             "AutofillEnableSupportForAddressOverflow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing address overflow and
// landmark fields.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForAddressOverflowAndLandmark,
             "AutofillEnableSupportForAddressOverflowAndLandmark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing address overflow and
// landmark fields.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForBetweenStreetsOrLandmark,
             "AutofillEnableSupportForBetweenStreetsOrLandmark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing landmarks.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForLandmark,
             "AutofillEnableSupportForLandmark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug.com/1165780): Remove once shared labels are launched.
BASE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels,
             "AutofillEnableSupportForParsingWithSharedLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if heuristic field parsing should be performed on email-only forms.
// TODO(crbug.com/1493145): Remove when/if launched.
BASE_FEATURE(kAutofillEnableEmailHeuristicOnlyAddressForms,
             "AutofillEnableEmailHeuristicOnlyAddressForms",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a couple of improvements to credit card expiration date handling:
// - The autocomplete attribute values are rationalized with format strings
//   like MM/YY from placeholders and labels in mind.
// - more fill follow.
// TODO(crbug.com/1441057): Remove once launched.
BASE_FEATURE(kAutofillEnableExpirationDateImprovements,
             "AutofillEnableExpirationDateImprovements",
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

// Controls whether honorific prefix is shown and editable in Autofill Settings
// on Android, iOS and Desktop.
// TODO(crbug.com/1141460): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForHonorificPrefixes,
             "AutofillEnableSupportForHonorificPrefixes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Some countries like BR and MX have address forms with only a zip code.
// If this feature is enabled, those fields may be classified as zip code fields
// for users who are located in BR/MX, even though our typical policy is to
// disable local heuristics for forms with <3 fields.
BASE_FEATURE(kAutofillEnableZipOnlyAddressForms,
             "AutofillEnableZipOnlyAddressForms",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, trunk prefix-related phone number types are added to the
// supported and matching types of |PhoneNumber|. Local heuristics for these
// types are enabled as well.
BASE_FEATURE(kAutofillEnableSupportForPhoneNumberTrunkTypes,
             "AutofillEnableSupportForPhoneNumberTrunkTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Replaces cached web elements in AutofillAgent and FormTracker by their
// renderer ids.
// DONOTSUMBIT: Disable.
BASE_FEATURE(kAutofillReplaceCachedWebElementsByRendererIds,
             "AutofillReplaceCachedWebElementsByRendererIds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes AutofillProfile::IsSubsetOfForFieldSet stop ignoring street address
// types during comparison, and instead compares them using address rewriter
// normalization.
BASE_FEATURE(kAutofillUseAddressRewriterInProfileSubsetComparison,
             "AutofillUseAddressRewriterInProfileSubsetComparison",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using the newer i18n address model, overriding the legacy one.
// This includes:
// - Using newer i18n address format strings.
BASE_FEATURE(kAutofillUseI18nAddressModel,
             "AutofillUseI18nAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Changes Autofill Clear Form into Undo Autofill.
BASE_FEATURE(kAutofillUndo, "AutofillUndo", base::FEATURE_DISABLED_BY_DEFAULT);

// Makes is_autofilled = true cached only after filling and not previewing.
BASE_FEATURE(kAutofillOnlyCacheIsAutofilledOnFill,
             "AutofillOnlyCacheIsAutofilledOnFill",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// When enabled, some local heuristic predictions will take precedence over the
// autocomplete attribute and server predictions, when determining a field's
// overall type.
BASE_FEATURE(kAutofillLocalHeuristicsOverrides,
             "AutofillLocalHeuristicsOverrides",
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

// When enabled, all behaviours related to the on-device machine learning
// model for field type predictions will be guarded.
// TODO(crbug.com/1465926): Remove when launched.
BASE_FEATURE(kAutofillModelPredictions,
             "AutofillModelPredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The dictionary will be used for creating a new `AutofillModelVectorizer`
// for vectorizing the model input. The `AutofillModelExecutor` will use
// that dictionary path to initialize the vectorizer.
// TODO(crbug.com/1465926): Remove once model is replaced with bigger model
// and store dictionary path in the metadata's additional files.
const base::FeatureParam<std::string> kAutofillModelDictionaryFilePath{
    &kAutofillModelPredictions, "dictionary_path", "default"};

// When true, use the machine learning model as the active `HeuristicSource`,
// else use the source provided by `kAutofillParsingPatternActiveSource`.
// TODO(crbug.com/1465926): Remove when launched.
const base::FeatureParam<bool> kAutofillModelPredictionsAreActive{
    &kAutofillModelPredictions, "model_active", false};

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
// See components/autofill/core/browser/server_prediction_overrides.h for more
// examples and details on how to specify overrides.
BASE_FEATURE(kAutofillOverridePredictions,
             "AutofillOverridePredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The override specification in string form.
const base::FeatureParam<std::string> kAutofillOverridePredictionsSpecification{
    &kAutofillOverridePredictions, "spec", "[]"};

// The override specification using alternative_form_signature in string form.
const base::FeatureParam<std::string>
    kAutofillOverridePredictionsForAlternativeFormSignaturesSpecification{
        &kAutofillOverridePredictions, "alternative_signature_spec", "[]"};

// If enabled, Autofill will first look at field labels and then at field
// attributes when classifying address fields in Mexico.
BASE_FEATURE(kAutofillPreferLabelsInSomeCountries,
             "AutofillPreferLabelsInSomeCountries",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If the feature is enabled, paint checks over individual `PopupCellView`s (to
// verify that a user's cursor has been outside the cell before accepting it)
// are disabled.
BASE_FEATURE(kAutofillPopupDisablePaintChecks,
             "AutofillPopupDisablePaintChecks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the autofill popup is hidden when the context menu is open.
BASE_FEATURE(kAutofillPopupDoesNotOverlapWithContextMenu,
             "AutofillPopupDoesNotOverlapWithContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, custom cursors exceeding the (24 dips) dimension
// limit are disallowed for all active tabs in all active windows.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kAutofillPopupMultiWindowCursorSuppression,
             "AutofillPopupMultiWindowCursorSuppression",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the threshold for accepting Autofill popup suggestions
// should take into account latency information of the user event.
BASE_FEATURE(kAutofillPopupUseLatencyInformationForAcceptThreshold,
             "AutofillPopupUseLatencyInformationForAcceptThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, FormTracker's probable-form-submission detection
// is disabled and replaced with browser-side detection.
// TODO(crbug/1117451): Remove once it works.
BASE_FEATURE(kAutofillProbableFormSubmissionInBrowser,
             "AutofillProbableFormSubmissionInBrowser",
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

// Controls non-default Autofill API predictions. See crbug.com/1331322.
BASE_FEATURE(kAutofillServerBehaviors,
             "AutofillServerBehaviors",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Chrome doesn't need to know the meaning of the value. Chrome only needs to
// forward it to the Autofill API, to let the server know which group the client
// belongs to.
const base::FeatureParam<int> kAutofillServerBehaviorsParam{
    &kAutofillServerBehaviors, "server_prediction_source", 0};

// Controls whether Autofill may fill across origins.
// In payment forms, the cardholder name field is often on the merchant's origin
// while the credit card number and CVC are in iframes hosted by a payment
// service provider. By enabling the policy-controlled feature "shared-autofill"
// in those iframes, the merchant's website enable Autofill to fill the credit
// card number and CVC fields from the cardholder name field, even though this
// autofill operation crosses origins.
// TODO(crbug.com/1304721): Enable this feature.
BASE_FEATURE(kAutofillSharedAutofill,
             "AutofillSharedAutofill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled, the AddressFieldParser does NOT try to parse
// address lines once it has found a street name and house number or other
// combinations of fields that indicate that an address form uses structured
// addresses. This should be the default in all countries with fully supported
// structured addresses. However, if a country is not sufficiently modeled,
// autofill may still do the right thing if it recognizes "Street name, house
// number, address line 2" as a sequence.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillStructuredFieldsDisableAddressLines,
             "AutofillStructuredFieldsDisableAddressLines",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to offer a delete button for Autocomplete entries in the
// Autofill popup.
BASE_FEATURE(kAutofillShowAutocompleteDeleteButton,
             "AutofillShowAutocompleteDeleteButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether granular filling will be available in the autofill popup.
// TODO(crbug.com/1459990): Clean up when launched.
BASE_FEATURE(kAutofillGranularFillingAvailable,
             "AutofillGranularFillingAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether field filling through the context menu will be available for
// the unclassified fields.
// TODO(crbug.com/1493361): Clean up when launched.
BASE_FEATURE(kAutofillForUnclassifiedFieldsAvailable,
             "AutofillForUnclassifiedFieldsAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether testing forms using devtools will be available.
// TODO(crbug.com/1459990): Clean up when launched.
BASE_FEATURE(kAutofillTestFormWithDevtools,
             "AutofillTestFormWithDevtools",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows silent profile updates even when the profile import requirements are
// not met.
BASE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport,
             "AutofillSilentProfileUpdateForInsufficientImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, on form submit, observations for every used profile are
// collected into the profile's `token_quality()`.
// TODO(crbug.com/1453650): Remove when launched.
BASE_FEATURE(kAutofillTrackProfileTokenQuality,
             "AutofillTrackProfileTokenQuality",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
// TODO(crbug.com/1153539): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillUseParameterizedSectioning,
             "AutofillUseParameterizedSectioning",
             base::FEATURE_ENABLED_BY_DEFAULT);
// In the experiment, we test different combinations of these parameters.
const base::FeatureParam<bool> kAutofillSectioningModeIgnoreAutocomplete{
    &kAutofillUseParameterizedSectioning, "ignore_autocomplete", false};
const base::FeatureParam<bool> kAutofillSectioningModeCreateGaps{
    &kAutofillUseParameterizedSectioning, "create_gaps", false};
const base::FeatureParam<bool> kAutofillSectioningModeExpand{
    &kAutofillUseParameterizedSectioning, "expand_assigned_sections", false};

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
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, Autofill suggestions are displayed in the keyboard accessory
// instead of the regular popup.
BASE_FEATURE(kAutofillKeyboardAccessory,
             "AutofillKeyboardAccessory_LAUNCHED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the touch to fill surface is shown for credit cards on
// Android.
BASE_FEATURE(kAutofillTouchToFillForCreditCardsAndroid,
             "AutofillTouchToFillForCreditCardsAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the whether the Chrome may provide a virtual view structure for
// Android Autofill.
BASE_FEATURE(kAutofillVirtualViewStructureAndroid,
             "AutofillVirtualViewStructureAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kAutofillUseMobileLabelDisambiguation,
             "AutofillUseMobileLabelDisambiguation",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kAutofillUseMobileLabelDisambiguationParameterName[] = "variant";
const char kAutofillUseMobileLabelDisambiguationParameterShowAll[] = "show-all";
const char kAutofillUseMobileLabelDisambiguationParameterShowOne[] = "show-one";

// When enabled, the keyboard accessory is shown for autocomplete=unrecognized
// fields. Selecting a keyboard accessory suggestion will fill the triggering
// field (independently of the autocomplete attribute) and all
// autocomplete != unrecognized fields in the triggering field's section.
// Note that this only affects address fields, since credit cards already ignore
// autocomplete=unrecognized.
// TODO(crbug.com/1446318): Remove when launched.
BASE_FEATURE(kAutofillSuggestionsForAutocompleteUnrecognizedFieldsOnMobile,
             "AutofillSuggestionsForAutocompleteUnrecognizedFieldsOnMobile",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

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
