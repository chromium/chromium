// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace autofill::features {

// LINT.IfChange(autofill_across_iframes_ios)
// Controls whether to flatten and fill cross-iframe forms on iOS.
// TODO(crbug.com/1441921) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframesIos,
             "AutofillAcrossIframesIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_across_iframes_ios)

// Use the heuristic parser to detect unfillable numeric types in field labels
// and grant the heuristic precedence over non-override server predictions.
BASE_FEATURE(kAutofillGivePrecedenceToNumericQuantities,
             "AutofillGivePrecedenceToNumericQuantities",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, address profile save problem will contain a dropdown for
// assigning a nickname to the address profile. Relevant only if the
// AutofillAddressProfileSavePrompt feature is enabled.
BASE_FEATURE(kAutofillAddressProfileSavePromptNicknameSupport,
             "AutofillAddressProfileSavePromptNicknameSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag to control the displaying of an ongoing hats survey that
// measures users perception of Autofill. Differently from other surveys,
// the Autofill user perception survey will not have a specific target
// number of answers where it will be fully stop, instead, it will run
// indefinitely. A target number of full answers exists, but per quarter. The
// goal is to have a go to place to understand how users are perceiving autofill
// across quarters.
BASE_FEATURE(kAutofillAddressUserPerceptionSurvey,
             "AutofillAddressUserPerceptionSurvey",
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

// Crowdsourcing already prefers PHONE_HOME_CITY_AND_NUMBER over
// PHONE_HOME_WHOLE_NUMBER. With this feature, local heuristics do the same.
// TODO(crbug.com/1474308): Clean up after June 1, 2024.
BASE_FEATURE(kAutofillDefaultToCityAndNumber,
             "AutofillDefaultToCityAndNumber",
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

// Kill switch for feature which when:
//   (1) The last interacted element is a field without a containing <form>
//   AND
//   (2) An element that the user has edited has been removed from the page
// changes the submitted form detection logic to use the provisionally saved
// form instead of using any remaining fields on the page (Example: The page
// might still have an <input> search bar).
// TODO(crbug.com/1523655): Remove kill switch.
BASE_FEATURE(kAutofillPreferProvisionalFormWhenFormlessFormIsRemoved,
             "AutofillPreferProvisionalFormWhenFormlessFormIsRemoved",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Relaxes the requirements for offering credit card import.
// TODO(crbug.com/1381477): Clean up when launched.
BASE_FEATURE(kAutofillRelaxCreditCardImport,
             "AutofillRelaxCreditCardImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, autofill will fill <selectlist> elements.
// TODO(crbug.com/1427153) Remove once autofilling <selectlist> is launched.
BASE_FEATURE(kAutofillEnableSelectList,
             "AutofillEnableSelectList",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, autofill displays an IPH informing users about using autofill
// from the context menu. The IPH will be attached to address fields with
// autocomplete="garbage".
// TODO(b/313587343) Remove once manual fallback IPH feature is launched.
BASE_FEATURE(kAutofillEnableManuallFallbackIPH,
             "AutofillEnableManuallFallbackIPH",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Controls if Chrome parses fields at street locations. This field type is
// generally supported in the legacy hierarchy but there is a risk of confusing
// an address line 1 with a street location. We don't have a good strategy for
// that yet. Therefore, this behavior is limited to MX at the moment.
// TODO(crbug.com/1441904) Remove once launched.
BASE_FEATURE(kAutofillEnableParsingOfStreetLocation,
             "AutofillEnableParsingOfStreetLocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if special rationalization rules for mexico are enabled.
BASE_FEATURE(kAutofillEnableRationalizationEngineForMX,
             "AutofillEnableRationalizationEngineForMX",
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

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/1156315): Remove once launched.
BASE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses,
             "AutofillEnableLabelPrecedenceForTurkishAddresses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, trunk prefix-related phone number types are added to the
// supported and matching types of |PhoneNumber|. Local heuristics for these
// types are enabled as well.
BASE_FEATURE(kAutofillEnableSupportForPhoneNumberTrunkTypes,
             "AutofillEnableSupportForPhoneNumberTrunkTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, only non-ad frames are extracted.
// Otherwise, non-ad frames as well as *visible* ad frames are extracted.
// "Extracted" means that FormFieldData::child_frames is populated, which is
// necessary for flattening these forms.
// The forms in those frames are extracted either way.
// TODO(crbug.com/40196220): Remove once launched.
BASE_FEATURE(kAutofillExtractOnlyNonAdFrames,
             "AutofillExtractOnlyNonAdFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_xhr_submission_detection_ios)
// If enabled, XHR form submissions are detected on iOS when the last interacted
// form in a frame is removed. Otherwise only HTTP form submissions are detected
// on iOS.
BASE_FEATURE(kAutofillEnableXHRSubmissionDetectionIOS,
             "AutofillEnableXHRSubmissionDetectionIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_xhr_submission_detection_ios)

// Implements a model that suppresses suggestions after N times the user ignores
// the popup (i.e. doesn't select a suggestion from the popup).
// N depends on the parametrization of the feature.
BASE_FEATURE(kAutofillSuggestionNStrikeModel,
             "AutofillSuggestionNStrikeModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kSuggestionStrikeLimit{
    &kAutofillSuggestionNStrikeModel, "strike-limit", 5};

// Replaces blink::WebFormElementObserver usage in FormTracker by updated logic
// for tracking the disappearance of forms as well as other submission
// triggering events.
BASE_FEATURE(kAutofillReplaceFormElementObserver,
             "AutofillReplaceFormElementObserver",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, FormFieldData::is_visible is a heuristic for actual visibility.
// Otherwise, it's an alias for FormFieldData::is_focusable.
// TODO(crbug.com/324199622) When abandoned, remove FormFieldData::is_visible.
BASE_FEATURE(kAutofillDetectFieldVisibility,
             "AutofillDetectFieldVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, new heuristics are applied for disamibguating multiple possible
// types in a form field. Otherwise, only the already established heuristic for
// disambiguating address and credit card names is used.
BASE_FEATURE(kAutofillDisambiguateContradictingFieldTypes,
             "AutofillDisambiguateContradictingFieldTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, whenever form controls are removed from the DOM, the ChromeClient
// is informed about this. This enables Autofill to trigger a reparsing of
// forms.
BASE_FEATURE(kAutofillDetectRemovedFormControls,
             "AutofillDetectRemovedFormControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Replaces cached web elements in AutofillAgent and FormTracker by their
// renderer ids.
BASE_FEATURE(kAutofillReplaceCachedWebElementsByRendererIds,
             "AutofillReplaceCachedWebElementsByRendererIds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes AutofillProfile::IsSubsetOfForFieldSet stop ignoring street address
// types during comparison, and instead compares them using address rewriter
// normalization.
BASE_FEATURE(kAutofillUseAddressRewriterInProfileSubsetComparison,
             "AutofillUseAddressRewriterInProfileSubsetComparison",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using the newer i18n address model, overriding the legacy one.
// This includes:
// - Using newer i18n address hierarchies.
// - Using newer i18n address format strings.
// - Using newer i18n address parsing rules.
BASE_FEATURE(kAutofillUseI18nAddressModel,
             "AutofillUseI18nAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Brazil, overriding the legacy one.
BASE_FEATURE(kAutofillUseBRAddressModel,
             "AutofillUseBRAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Germany, overriding the legacy one.
BASE_FEATURE(kAutofillUseDEAddressModel,
             "AutofillUseDEAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for India, overriding the legacy one.
BASE_FEATURE(kAutofillUseINAddressModel,
             "AutofillUseINAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Mexico, overriding the legacy one.
BASE_FEATURE(kAutofillUseMXAddressModel,
             "AutofillUseMXAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will issues votes for EMAIL_ADDRESS field types on
// fields where the content matches a valid email format.
BASE_FEATURE(kAutofillUploadVotesForFieldsWithEmail,
             "AutofillUploadVotesForFieldsWithEmail",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Changes Autofill Clear Form into Undo Autofill.
BASE_FEATURE(kAutofillUndo, "AutofillUndo", base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, some local heuristic predictions will take precedence over the
// autocomplete attribute and server predictions, when determining a field's
// overall type.
BASE_FEATURE(kAutofillLocalHeuristicsOverrides,
             "AutofillLocalHeuristicsOverrides",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, all behaviours related to the on-device machine learning
// model for field type predictions will be guarded.
// TODO(crbug.com/1465926): Remove when launched.
BASE_FEATURE(kAutofillModelPredictions,
             "AutofillModelPredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When true, use the machine learning model as the active `HeuristicSource`,
// else use the source provided by `kAutofillParsingPatternActiveSource`.
const base::FeatureParam<bool> kAutofillModelPredictionsAreActive{
    &kAutofillModelPredictions, "model_active", false};

// If enabled, Autofill will first look at field labels and then at field
// attributes when classifying address fields in Mexico.
BASE_FEATURE(kAutofillPreferLabelsInSomeCountries,
             "AutofillPreferLabelsInSomeCountries",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a pre-filled field will only be overwritten if it's not
// classified as meaningfully pre-filled based on server predictions. If also
// flag `kAutofillSkipPreFilledFields` is enabled, a pre-filled field will only
// be overwritten if it's classified as a placeholder.
BASE_FEATURE(kAutofillOverwritePlaceholdersOnly,
             "AutofillOverwritePlaceholdersOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, a pre-filled field will not be filled.
BASE_FEATURE(kAutofillSkipPreFilledFields,
             "AutofillSkipPreFilledFields",
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
// parameter. One of "default", "experimental", "nextgen".
// This parameter is only supported in Chrome-branded builds. Non-Chrome branded
// builds default to the legacy patterns.
// TODO(crbug/1248339): Remove once experiment is finished.
const base::FeatureParam<std::string> kAutofillParsingPatternActiveSource{
    &kAutofillParsingPatternProvider, "prediction_source", "default"};

// Enables detection of language from Translate.
// TODO(crbug/1150895): Cleanup when launched.
BASE_FEATURE(kAutofillPageLanguageDetection,
             "AutofillPageLanguageDetection",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

// If the feature is enabled, then the time when the Autofill popup is
// considered to have been shown is measured only once the UI thread has become
// idle. The intent behind this is to avoid situations in which the OS message
// queue has a backlog and input event timestamps become inaccurate (i.e. event
// timestamps indicate that events are more recent than they should be).
BASE_FEATURE(kAutofillPopupImprovedTimingChecks,
             "AutofillPopupImprovedTimingChecks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, then the timing measurement of when the Autofill
// popup is considered to have been shown only happens at a delay - 500 ms after
// showing the popup. The same protection mechanisms as for
// `kAutofillPopupImprovedTimingChecks` are used, but only after 500 ms have
// passed. The intent is to ensure that events that the user triggered within
// 500 ms of the popup are showing do not arrive delayed on the UI thread of the
// browser process.
// TODO(crbug.com/475902): If this feature proves effective, combine it with
// `kAutofillPopupImprovedTimingChecks`.
BASE_FEATURE(kAutofillPopupImprovedTimingChecksV2,
             "AutofillPopupImprovedTimingChecksV2",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows silent profile updates even when the profile import requirements are
// not met.
BASE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport,
             "AutofillSilentProfileUpdateForInsufficientImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Sends text change events for textarea elements. When this is off, only input
// elements and maybe contenteditable elements send text change events.
BASE_FEATURE(kAutofillTextAreaChangeEvents,
             "AutofillTextAreaChangeEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Sends text change events for contenteditable elements. When this is off,
// only input elements and maybe textarea elements send text change events.
BASE_FEATURE(kAutofillContentEditableChangeEvents,
             "AutofillContentEditableChangeEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, on form submit, observations for every used profile are
// collected into the profile's `token_quality()`.
// TODO(crbug.com/1453650): Remove when launched.
BASE_FEATURE(kAutofillTrackProfileTokenQuality,
             "AutofillTrackProfileTokenQuality",
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

// Whether to favor credit card number that user typed into input field vs
// input field value (which was potentially modified via JavaScript).
BASE_FEATURE(kAutofillUseTypedCreditCardNumber,
             "AutofillUseTypedCreditCardNumber",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Improves the selection of phone country codes by also considering address
// country codes / names.
// See GetStreetAddressForInput() in field_filling_address_util.cc for a details
// description.
// TODO(crbug.com/1395740). Clean up when launched.
BASE_FEATURE(kAutofillEnableFillingPhoneCountryCodesByAddressCountryCodes,
             "AutofillEnableFillingPhoneCountryCodesByAddressCountryCodes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls autofill popup style, if enabled it becomes more prominent,
// i.e. its shadow becomes more emphasized, position is also updated.
// TODO(crbug.com/1354136): Remove once the experiment is over.
BASE_FEATURE(kAutofillMoreProminentPopup,
             "AutofillMoreProminentPopup",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillMoreProminentPopupMaxOffsetToCenterParam{
    &kAutofillMoreProminentPopup, "max_offset_to_center_px", 92};

// Enable the feature by default, and set the enabled percentage as a feature
// param. We are logging information of field types, autofill status and
// forms with a defined sampling rate of 10% on sessions.
// Autofill FormSummary/FieldInfo UKM schema:
// https://docs.google.com/document/d/1ZH0JbL6bES3cD4KqZWsGR6n8I-rhnkx6no6nQOgYq5w/.
BASE_FEATURE(kAutofillLogUKMEventsWithSamplingOnSession,
             "AutofillLogUKMEventsWithSamplingOnSession",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kAutofillLogUKMEventsWithSamplingOnSessionRate{
    &kAutofillLogUKMEventsWithSamplingOnSession, "sampling_rate", 10};

// Autofill is experimenting with an updated set of country specific rules.
// Controls whether we use the current country-specific address import field
// requirements or the updated ones.
BASE_FEATURE(kAutofillUseUpdatedRequiredFieldsForAddressImport,
             "AutofillUseUpdatedRequiredFieldsForAddressImport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether user tap on an element is needed to show autofill
// suggestions. If enabled, this flag would disable android autofill suggestions
// if the focus on an element is Javascript-originated.
// DidReceiveLeftMouseDownOrGestureTapInNode() will show suggestions if the
// focus change occurred as a result of a gesture. See crbug.com/730764 for why
// showing autofill suggestions as a result of JavaScript changing focus is
// enabled on WebView.
// TODO(crbug.com/1496382) Clean up autofill feature flag
// `kAutofillAndroidDisableSuggestionsOnJSFocus`
BASE_FEATURE(kAutofillAndroidDisableSuggestionsOnJSFocus,
             "AutofillAndroidDisableSuggestionsOnJSFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, FormFieldParser::MatchesRegexWithCache tries to avoid
// re-computing whether a regex matches an input string by caching the result.
// The result size is controlled by
// kAutofillEnableCacheForRegexMatchingCacheSizeParam.
BASE_FEATURE(kAutofillEnableCacheForRegexMatching,
             "AutofillEnableCacheForRegexMatching",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kAutofillEnableCacheForRegexMatchingCacheSizeParam{
        &kAutofillEnableCacheForRegexMatching, "cache_size", 300};

#if BUILDFLAG(IS_ANDROID)
// Controls if Chrome Autofill UI surfaces ignore touch events if something is
// fully or partially obscuring the Chrome window.
BASE_FEATURE(kAutofillEnableSecurityTouchEventFilteringAndroid,
             "AutofillEnableSecurityTouchEventFilteringAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the whether the Chrome may provide a virtual view structure for
// Android Autofill.
BASE_FEATURE(kAutofillVirtualViewStructureAndroid,
             "AutofillVirtualViewStructureAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

namespace test {

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
