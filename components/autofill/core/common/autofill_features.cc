// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "build/chromeos_buildflags.h"

namespace autofill::features {

// LINT.IfChange(autofill_across_iframes_ios)
// Controls whether to flatten and fill cross-iframe forms on iOS.
// TODO(crbug.com/40266699) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframesIos,
             "AutofillAcrossIframesIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_across_iframes_ios)

// Use the heuristic parser to detect unfillable numeric types in field labels
// and grant the heuristic precedence over non-override server predictions.
BASE_FEATURE(kAutofillGivePrecedenceToNumericQuantities,
             "AutofillGivePrecedenceToNumericQuantities",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/40151750): Remove this feature flag after the explicit save
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

// If enabled, Autofill is informed about the caret position while showing a
// popup.
// TODO(crbug.com/339156167): Remove when launched.
BASE_FEATURE(kAutofillCaretExtraction,
             "AutofillCaretExtraction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAgent's left-click handler tries to treat
// contenteditables appropriately.
// This is a kill switch.
// TODO(crbug.com/341695271): Remove when launched.
BASE_FEATURE(kAutofillContentEditableLeftClickFix,
             "AutofillContentEditableLeftClickFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Same as `kAutofillAddressUserPerceptionSurvey` but for credit card forms.
BASE_FEATURE(kAutofillCreditCardUserPerceptionSurvey,
             "AutofillCreditCardUserPerceptionSurvey",
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

// If enabled, Autofill always sets the phone number as parsed by
// i18n::phonenumber.
// TODO(crbug.com/40220393): Cleanup when launched.
BASE_FEATURE(kAutofillPreferParsedPhoneNumber,
             "AutofillPreferParsedPhoneNumber",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the country calling code for nationally formatted phone numbers
// is inferred from the profile's country, if available.
// TODO(crbug.com/40220393): Cleanup when launched.
BASE_FEATURE(kAutofillInferCountryCallingCode,
             "AutofillInferCountryCallingCode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, label inference considers strings entirely made up of  '(', ')'
// and '-' as valid labels.
// TODO(crbug.com/40220393): Cleanup when launched.
BASE_FEATURE(kAutofillConsiderPhoneNumberSeparatorsValidLabels,
             "AutofillConsiderPhoneNumberSeparatorsValidLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Crowdsourcing already prefers PHONE_HOME_CITY_AND_NUMBER over
// PHONE_HOME_WHOLE_NUMBER. With this feature, local heuristics do the same.
// TODO(crbug.com/40279279): Clean up after June 1, 2024.
BASE_FEATURE(kAutofillDefaultToCityAndNumber,
             "AutofillDefaultToCityAndNumber",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, no prefix matching is applied to filter credit card number
// suggestions.
// TODO(crbug.com/338932642): Clean up if launched.
BASE_FEATURE(kAutofillDontPrefixMatchCreditCardNumbersOrCvcs,
             "AutofillDontPrefixMatchCreditCardNumbersOrCvcs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// FormStructure::RetrieveFromCache used to preserve an AutofillField's
// is_autofilled from the cache of previously parsed forms. This makes little
// sense because the renderer sends us the autofill state and has the most
// recent information. Dropping the old behavior should not make any difference
// but to be sure, this is gated by a finch experiment.
// TODO(crbug.com/40871691) Cleanup when launched.
BASE_FEATURE(kAutofillDontPreserveAutofillState,
             "AutofillDontPreserveAutofillState",
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

// When enabled, addresses of every country are considered eligible for account
// address storage.
BASE_FEATURE(kAutofillEnableAccountStorageForIneligibleCountries,
             "AutofillEnableAccountStorageForIneligibleCountries",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a new implementation for address field parsing that is based on
// backtracking.
BASE_FEATURE(kAutofillEnableAddressFieldParserNG,
             "AutofillEnableAddressFieldParserNG",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, autofill displays an IPH informing users about using autofill
// from the context menu. The IPH will be attached to address fields with
// autocomplete="garbage".
// TODO(b/313587343) Remove once manual fallback IPH feature is launched.
BASE_FEATURE(kAutofillEnableManualFallbackIPH,
             "AutofillEnableManualFallbackIPH",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing between streets.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForBetweenStreets,
             "AutofillEnableSupportForBetweenStreets",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome supports filling and importing administrative area
// level 2. A sub-division of a state, e.g. a Municipio in Brazil or Mexico.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForAdminLevel2,
             "AutofillEnableSupportForAdminLevel2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing address overflow fields.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForAddressOverflow,
             "AutofillEnableSupportForAddressOverflow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing address overflow and
// landmark fields.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForAddressOverflowAndLandmark,
             "AutofillEnableSupportForAddressOverflowAndLandmark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing address overflow and
// landmark fields.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForBetweenStreetsOrLandmark,
             "AutofillEnableSupportForBetweenStreetsOrLandmark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing landmarks.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForLandmark,
             "AutofillEnableSupportForLandmark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome parses fields at street locations. This field type is
// generally supported in the legacy hierarchy but there is a risk of confusing
// an address line 1 with a street location. We don't have a good strategy for
// that yet. Therefore, this behavior is limited to MX at the moment.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillEnableParsingOfStreetLocation,
             "AutofillEnableParsingOfStreetLocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if special rationalization rules for mexico are enabled.
BASE_FEATURE(kAutofillEnableRationalizationEngineForMX,
             "AutofillEnableRationalizationEngineForMX",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug.com/40741721): Remove once shared labels are launched.
BASE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels,
             "AutofillEnableSupportForParsingWithSharedLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if heuristic field parsing should be performed on email-only forms.
// TODO(crbug.com/40285735): Remove when/if launched.
BASE_FEATURE(kAutofillEnableEmailHeuristicOnlyAddressForms,
             "AutofillEnableEmailHeuristicOnlyAddressForms",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When true, use autocomplete=email as required signal for email fields on
// email-only forms, else accept a wider range of autocomplete values except for
// `off` and `false`.
const base::FeatureParam<bool> kAutofillEnableEmailHeuristicAutocompleteEmail{
    &kAutofillEnableEmailHeuristicOnlyAddressForms, "autocomplete_email",
    false};

// Controls if Chrome support filling and importing apartment numbers.
// TODO(crbug.com/40734406): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForApartmentNumbers,
             "AutofillEnableSupportForApartmentNumbers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Autofill parses ADDRESS_HOME_DEPENDENT_LOCALITY.
// TODO(crbug.com/40160818): Remove once launched.
BASE_FEATURE(kAutofillEnableDependentLocalityParsing,
             "AutofillEnableDependentLocalityParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a couple of improvements to credit card expiration date handling:
// - The autocomplete attribute values are rationalized with format strings
//   like MM/YY from placeholders and labels in mind.
// - more fill follow.
// TODO(crbug.com/40266396): Remove once launched.
BASE_FEATURE(kAutofillEnableExpirationDateImprovements,
             "AutofillEnableExpirationDateImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to save the first number in a form with multiple phone
// numbers instead of aborting the import.
// TODO(crbug.com/40742746) Remove once launched.
BASE_FEATURE(kAutofillEnableImportWhenMultiplePhoneNumbers,
             "AutofillEnableImportWhenMultiplePhoneNumbers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/40735892): Remove once launched.
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

// When enabled, focusing on an autofilled field that was traditionally filled
// with address data (meaning filled with the value of their classified type)
// will yield field-by-field filling suggestions without prefix matching.
// TODO(b/339543182): Remove when launched.
BASE_FEATURE(kAutofillAddressFieldSwapping,
             "AutofillAddressFieldSwapping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Resets the autofill state of a field when JavaScript modifies its value.
// Also resets the AutofillState of the blink element to kAutofilled if the
// change was only a reformatting (inserting whitespaces and special
// characters).
// This feature should be enabled with
// blink::features::AllowJavaScriptToResetAutofillState.
BASE_FEATURE(kAutofillFixCachingOnJavaScriptChanges,
             "AutofillFixCachingOnJavaScriptChanges",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Changes the semantics of FocusOnFormField() and FocusOnNonFormField() so that
// - FocusOnFormField() is called when the focus moves to another field,
//   including fields owned by form, unowned fields, and contenteditables.
// - FocusOnNonFormField() is called in all remaining cases.
// See crbug.com/337690061 for details.
// This is a kill switch.
// TODO(crbug.com/337690061): Remove when cleaning up
// `kAutofillNewFocusEvents`.
BASE_FEATURE(kAutofillNewFocusEvents,
             "AutofillNewFocusEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Killswitch for not running logic in `AutofillAgent::ApplyFieldsAction` that
// is responsible for updating `AutofillAgent::last_queried_element_`.
BASE_FEATURE(kAutofillDontUpdateLastQueriedElementOnFill,
             "AutofillDontUpdateLastQueriedElementOnFill",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Implements a model that suppresses suggestions after N times the user ignores
// the popup (i.e. doesn't select a suggestion from the popup).
// N depends on the parametrization of the feature.
BASE_FEATURE(kAutofillSuggestionNStrikeModel,
             "AutofillSuggestionNStrikeModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kSuggestionStrikeLimit{
    &kAutofillSuggestionNStrikeModel, "strike-limit", 5};

// Makes disused suggestion suppression logic ignore the first
// `kNumberOfIgnoredSuggestions` suggestions (in frecency order), so that the
// logic never returns an empty list after being passed a non-empty one.
BASE_FEATURE(kAutofillChangeDisusedAddressSuggestionTreatment,
             "AutofillChangeDisusedAddressSuggestionTreatment",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kNumberOfIgnoredSuggestions{
    &kAutofillChangeDisusedAddressSuggestionTreatment, "ignored-suggestions",
    1};

// Unifies the tracking of the last interacted elements between FormTracker and
// AutofillAgent and fixes inconsistencies in this tracking.
BASE_FEATURE(kAutofillUnifyAndFixFormTracking,
             "AutofillUnifyAndFixFormTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, new heuristics are applied for disambiguating multiple possible
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

// Enables using the newer i18n address model, overriding the legacy one.
// This includes:
// - Using newer i18n address hierarchies.
// - Using newer i18n address format strings.
// - Using newer i18n address parsing rules.
BASE_FEATURE(kAutofillUseI18nAddressModel,
             "AutofillUseI18nAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using the a custom address model for Australia, overriding the legacy
// one.
BASE_FEATURE(kAutofillUseAUAddressModel,
             "AutofillUseAUAddressModel",
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

// Enables using a custom address model for Poland, overriding the legacy one.
BASE_FEATURE(kAutofillUsePLAddressModel,
             "AutofillUsePLAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will issues votes for EMAIL_ADDRESS field types on
// fields where the content matches a valid email format.
BASE_FEATURE(kAutofillUploadVotesForFieldsWithEmail,
             "AutofillUploadVotesForFieldsWithEmail",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, some local heuristic predictions will take precedence over the
// autocomplete attribute and server predictions, when determining a field's
// overall type.
BASE_FEATURE(kAutofillLocalHeuristicsOverrides,
             "AutofillLocalHeuristicsOverrides",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, all behaviours related to the on-device machine learning
// model for field type predictions will be guarded.
// TODO(crbug.com/40276177): Remove when launched.
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
// TODO(crbug.com/40146444): Remove once launched.
BASE_FEATURE(kAutofillParsingPatternProvider,
             "AutofillParsingPatternProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The specific pattern set is controlled by the `kAutofillParsingPatternActive`
// parameter. One of "default", "experimental", "nextgen".
// This parameter is only supported in Chrome-branded builds. Non-Chrome branded
// builds default to the legacy patterns.
// TODO(crbug.com/40197215): Remove once experiment is finished.
const base::FeatureParam<std::string> kAutofillParsingPatternActiveSource{
    &kAutofillParsingPatternProvider, "prediction_source", "default"};

// Enables detection of language from Translate.
// TODO(crbug.com/40158074): Cleanup when launched.
BASE_FEATURE(kAutofillPageLanguageDetection,
             "AutofillPageLanguageDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the placeholder is not used as a fallback during label inference.
// Instead, local heuristics treat it as a separate source in addition to the
// label. The placeholder is matched against the same regex as the label.
// Since placeholders are often used as example values, this should allow us to
// extract a more appropriate label instead.
// TODO(crbug.com/40222716): Remove once launched.
BASE_FEATURE(kAutofillAlwaysParsePlaceholders,
             "AutofillAlwaysParsePlaceholders",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, paint checks over individual `PopupCellView`s (to
// verify that a user's cursor has been outside the cell before accepting it)
// are disabled.
BASE_FEATURE(kAutofillPopupDisablePaintChecks,
             "AutofillPopupDisablePaintChecks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, then the timing measurement of when the Autofill
// popup is considered to have been shown only happens at a delay - 500 ms after
// showing the popup. The same protection mechanisms as for
// `kAutofillPopupImprovedTimingChecks` are used, but only after 500 ms have
// passed. The intent is to ensure that events that the user triggered within
// 500 ms of the popup are showing do not arrive delayed on the UI thread of the
// browser process.
BASE_FEATURE(kAutofillPopupImprovedTimingChecksV2,
             "AutofillPopupImprovedTimingChecksV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(b/334909042): Remove after cleanup.
// If the feature is enabled, the Autofill popup widget is initialized with
// `Widget::InitParams::z_order` set to `ui::ZOrderLevel::kSecuritySurface`,
// otherwise the `z_order` is not set and defined by the widget type (see
// `Widget::InitParams::EffectiveZOrderLevel()`). This param makes the popup
// display on top of all other windows, which potentially can negatively
// affect their functionality.
BASE_FEATURE(kAutofillPopupZOrderSecuritySurface,
             "AutofillPopupZOrderSecuritySurface",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillStructuredFieldsDisableAddressLines,
             "AutofillStructuredFieldsDisableAddressLines",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether granular filling will be available in the autofill popup.
// TODO(crbug.com/40274514): Clean up when launched.
BASE_FEATURE(kAutofillGranularFillingAvailable,
             "AutofillGranularFillingAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This variation controls whether more suggestive suggestion labels are shown
// or not.
// The feature variation exists to separate the granular filling feature into
// multiple sub-features. Thus, metrics can be evaluated separately for each
// sub-feature.
// TODO(crbug.com/40274514): Clean up when launched.
const base::FeatureParam<bool>
    kAutofillGranularFillingAvailableWithImprovedLabelsParam{
        &kAutofillGranularFillingAvailable,
        "autofill_granular_filling_with_improved_labels", true};

// This variation controls whether the "Fill everything" button is displayed at
// the top or at the bottom. When at the top, it is displayed regardless of the
// filling mode. When at the bottom, it is displayed in all modes but the full
// form filling mode.
// The feature variation exists to separate the granular filling feature into
// multiple sub-features. Thus, metrics can be evaluated separately for each
// sub-feature.
// TODO(crbug.com/40274514): Clean up when launched.
const base::FeatureParam<bool>
    kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam{
        &kAutofillGranularFillingAvailable,
        "autofill_granular_filling_with_fill_everything_in_the_footer", true};

// This variation controls whether the expand children suggestions control is
// hidden for non-selected/non-expanded suggestions (and the control shows up
// when any part of the suggestion row is selected/hovered).
// This adjustment in behavior is not a part of the originally approved
// functionality and only exists to investigate one of the hypothesises of
// the acceptance rate drop reasons, namely, user's potential confusion due to
// the newly introduced UI pattern.
// TODO(crbug.com/40274514): Clean up when launched.
const base::FeatureParam<bool>
    kAutofillGranularFillingAvailableWithExpandControlVisibleOnSelectionOnly{
        &kAutofillGranularFillingAvailable,
        "autofill_granular_filling_with_expand_control_visible_on_selection_"
        "only",
        false};

// Controls whether field filling through the context menu will be available for
// the unclassified fields.
// TODO(crbug.com/40285811): Clean up when launched.
BASE_FEATURE(kAutofillForUnclassifiedFieldsAvailable,
             "AutofillForUnclassifiedFieldsAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether test address profiles will be present in the Autofill popup.
// TODO(crbug.com/40270486): Clean up when launched.
BASE_FEATURE(kAutofillTestFormWithTestAddresses,
             "AutofillTestFormWithTestAddresses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows silent profile updates even when the profile import requirements are
// not met.
BASE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport,
             "AutofillSilentProfileUpdateForInsufficientImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Sends text change events for textarea elements. When this is off, only input
// elements and maybe contenteditable elements send text change events.
BASE_FEATURE(kAutofillTextAreaChangeEvents,
             "AutofillTextAreaChangeEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Sends text change events for contenteditable elements. When this is off,
// only input elements and maybe textarea elements send text change events.
// Enabled by default for Mac and Windows platforms.
BASE_FEATURE(kAutofillContentEditableChangeEvents,
             "AutofillContentEditableChangeEvents",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Causes Autofill to announce the Compose popup less assertively.
BASE_FEATURE(kComposePopupAnnouncePolitely,
             "ComposePopupAnnouncePolitely",
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

// Improves the selection of phone country codes by also considering address
// country codes / names.
// See GetStreetAddressForInput() in field_filling_address_util.cc for a details
// description.
// TODO(crbug.com/40249216). Clean up when launched.
BASE_FEATURE(kAutofillEnableFillingPhoneCountryCodesByAddressCountryCodes,
             "AutofillEnableFillingPhoneCountryCodesByAddressCountryCodes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls autofill popup style, if enabled it becomes more prominent,
// i.e. its shadow becomes more emphasized, position is also updated.
// TODO(crbug.com/40235454): Remove once the experiment is over.
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
             "AutofillAndroidDisableSuggestionsOnJSFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, FormFieldParser::MatchesRegexWithCache tries to avoid
// re-computing whether a regex matches an input string by caching the result.
// The result size is controlled by
// kAutofillEnableCacheForRegexMatchingCacheSizeParam.
BASE_FEATURE(kAutofillEnableCacheForRegexMatching,
             "AutofillEnableCacheForRegexMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kAutofillEnableCacheForRegexMatchingCacheSizeParam{
        &kAutofillEnableCacheForRegexMatching, "cache_size", 1000};

// When enabled, various deduplication related metrics are logged on startup
// and on import.
// TODO(b/325452461): Remove once rolled out.
BASE_FEATURE(kAutofillLogDeduplicationMetrics,
             "AutofillLogDeduplicationMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, low-quality quasi duplicates of rank one are silently removed
// during the once-per-milestone deduplication routine.
// TODO(b/325450676): Remove when launched.
BASE_FEATURE(kAutofillSilentlyRemoveQuasiDuplicates,
             "AutofillSilentlyRemoveQuasiDuplicates",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Currently, the importing logic offers new profile creation if the observed
// profile is non-mergeable with any existing profile. With this feature, low-
// quality tokens receive special treatment and can bypass this requirement.
// In particular, if the observed profile was autofilled, except for an edit in
// a single type, this qualifies for an update of the autofilled profile, in
// case the edited type has low-quality.
// TODO(b/325451601): Remove when launched.
BASE_FEATURE(kAutofillUpdateLowQualityTokenOnImport,
             "AutofillUpdateLowQualityTokenOnImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, Captured Site Tests will use 'AutofillFlow' utility to trigger
// the autofill action. This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillCapturedSiteTestsUseAutofillFlow,
             "AutofillCapturedSiteTestsUseAutofillFlow",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Autofill upload throttling limits uploading a form to the Autofill server
// more than once over a `kAutofillUploadThrottlingPeriodInDays` period.
// This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillUploadThrottling,
             "AutofillUploadThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace test

}  // namespace autofill::features
