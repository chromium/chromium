// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"

namespace autofill::features {

// LINT.IfChange(autofill_across_iframes_ios)
// Controls whether to flatten and fill cross-iframe forms on iOS.
// TODO(crbug.com/40266699) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframesIos,
             "AutofillAcrossIframesIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_across_iframes_ios)

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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Same as `kAutofillAddressUserPerceptionSurvey` but for credit card forms.
BASE_FEATURE(kAutofillCreditCardUserPerceptionSurvey,
             "AutofillCreditCardUserPerceptionSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, no prefix matching is applied to filter credit card number
// suggestions.
// TODO(crbug.com/338932642): Clean up.
BASE_FEATURE(kAutofillDontPrefixMatchCreditCardNumbersOrCvcs,
             "AutofillDontPrefixMatchCreditCardNumbersOrCvcs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for Autofill filling.
BASE_FEATURE(kAutofillDisableFilling,
             "AutofillDisableFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for Autofill address import.
BASE_FEATURE(kAutofillDisableAddressImport,
             "AutofillDisableAddressImport",
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
// TODO(crbug.com/313587343) Remove once manual fallback IPH feature is
// launched.
BASE_FEATURE(kAutofillEnableManualFallbackIPH,
             "AutofillEnableManualFallbackIPH",
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

// Controls if heuristic field parsing should be performed on email-only forms
// without an enclosing form tag. This feature will only be launched once
// `kAutofillEnableEmailHeuristicOnlyAddressForms` rolls out.
// TODO(crbug.com/40285735): Remove when/if launched.
BASE_FEATURE(kAutofillEnableEmailHeuristicOutsideForms,
             "AutofillEnableEmailHeuristicOutsideForms",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When true, use autocomplete=email as required signal for email fields on
// email-only forms, else accept a wider range of autocomplete values except for
// `off` and `false`.
const base::FeatureParam<bool> kAutofillEnableEmailHeuristicAutocompleteEmail{
    &kAutofillEnableEmailHeuristicOnlyAddressForms, "autocomplete_email",
    false};

// Control if Autofill supports German transliteration.
// TODO(crbug.com/328968064): Remove when/if launched.
BASE_FEATURE(kAutofillEnableGermanTransliteration,
             "AutofillEnableGermanTransliteration",
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

// Controls if Autofill executes the parser for the prediction improvements.
// TODO(crbug.com/345170058) Remove once launched.
BASE_FEATURE(kAutofillEnableImprovedPredictionParser,
             "AutofillEnableImprovedPredictionParser",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/40735892): Remove once launched.
BASE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses,
             "AutofillEnableLabelPrecedenceForTurkishAddresses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, focusing on a credit card number field that was traditionally
// autofilled will yield all credit card suggestions.
// TODO(crbug.com/354175563): Remove when launched.
BASE_FEATURE(kAutofillEnablePaymentsFieldSwapping,
             "AutofillEnablePaymentsFieldSwapping",
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

// When enabled, focusing on an autofilled field that was traditionally filled
// with address data (meaning filled with the value of their classified type)
// will yield field-by-field filling suggestions without prefix matching.
// TODO(crbug.com/339543182): Remove when launched.
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

// Fixes the overloaded meaning of FormFieldData::value (current value of a
// field and initial value of a field): if enabled, AutofillField::value() takes
// into accounts its ValueSemantics parameter.
// TODO: crbug.com/40227496 - Clean up when launched.
BASE_FEATURE(kAutofillFixValueSemantics,
             "AutofillFixValueSemantics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillField::value(kInitial) for <select> fields returns the
// initial values. Otherwise, it is identical to AutofillField::value(kCurrent).
// Should only be enabled if kAutofillFixValueSemantics is enabled.
// TODO: crbug.com/40227496 - Clean up when launched.
BASE_FEATURE(kAutofillFixInitialValueOfSelect,
             "AutofillFixInitialValueOfSelect",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillField::value(kCurrent) is not reset for form import.
// Otherwise, AutofillField::value(kCurrent) is reset to the empty string for
// fields that are non-<select>, non-state, non-country and haven't changed
// their value.
// Should only be enabled if kAutofillFixValueSemantics and
// kAutofillFixInitialValueOfSelect is enabled.
// TODO: crbug.com/40227496 - Clean up when launched.
BASE_FEATURE(kAutofillFixCurrentValueInImport,
             "AutofillFixCurrentValueInImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, AutofillManager::GetCachedFormAndField will return the cached
// form if found, even if it doesn't satisfy
// `cached_form->autofill_count() != 0`.
BASE_FEATURE(kAutofillDecoupleAutofillCountFromCache,
             "AutofillDecoupleAutofillCountFromCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Gives precedence to local heuristics if they indicate that a field is an
// EMAIL_ADDRESS field and the server believes that it is a USERNAME or
// SINGLE_USERNAME field.
//
// Imagine that  a web page has a field that admits both email address
// and username, but the server prediction only captures the username
// aspect.
// With this feature disabled, we predict the overall type to be USERNAME. If
// Password Manager has not results, it defaults to Autofill, which, in turn,
// defaults to Autocomplete because it cannot handle the USERNAME prediction.
// With this feature enabled, Password Manager is still given  precedence for
// showing username suggestions if it has any. However, if it does not, Autofill
// can now show email-related suggestions. Only if it does not have any will it
// fall back to Autocomplete.
//
// TODO: crbug.com/360791229 - clean up.
BASE_FEATURE(kAutofillGivePrecedenceToEmailOverUsername,
             "AutofillGivePrecedenceToEmailOverUsername",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes Autofill try to import data from fields annotated with an unrecognized
// autocomplete HTML attribute. The default behavior doesn't allow that.
// TODO(crbug.com/347698797): Cleanup when launched.
BASE_FEATURE(kAutofillImportFromAutocompleteUnrecognized,
             "AutofillImportFromAutocompleteUnrecognized",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes disused suggestion suppression logic ignore the first
// `kNumberOfIgnoredSuggestions` suggestions (in frecency order), so that the
// logic never returns an empty list after being passed a non-empty one.
BASE_FEATURE(kAutofillChangeDisusedAddressSuggestionTreatment,
             "AutofillChangeDisusedAddressSuggestionTreatment",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kNumberOfIgnoredSuggestions{
    &kAutofillChangeDisusedAddressSuggestionTreatment, "ignored-suggestions",
    1};

// If enabled, we start forwarding submissions with source
// DOM_MUTATION_AFTER_AUTOFILL, even for non-password forms.
BASE_FEATURE(kAutofillAcceptDomMutationAfterAutofillSubmission,
             "AutofillAcceptDomMutationAfterAutofillSubmission",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Removes logic that resets form submission tracking data upon receiving a
// submission signal, so that integrators (namely Autofill and PasswordManager)
// can decide what sources to use and what sources to ignore. Also, fixes
// submission deduplication so that it ignores password submissions that PWM
// doesn't act upon.
BASE_FEATURE(kAutofillFixFormTracking,
             "AutofillFixFormTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Unifies the tracking of the last interacted elements between FormTracker and
// AutofillAgent and fixes inconsistencies in this tracking.
BASE_FEATURE(kAutofillUnifyAndFixFormTracking,
             "AutofillUnifyAndFixFormTracking",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables using the a custom address model for Australia, overriding the legacy
// one.
BASE_FEATURE(kAutofillUseAUAddressModel,
             "AutofillUseAUAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Canada, overriding the legacy one.
BASE_FEATURE(kAutofillUseCAAddressModel,
             "AutofillUseCAAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Germany, overriding the legacy one.
BASE_FEATURE(kAutofillUseDEAddressModel,
             "AutofillUseDEAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for France, overriding the legacy one.
BASE_FEATURE(kAutofillUseFRAddressModel,
             "AutofillUseFRAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for India, overriding the legacy one.
BASE_FEATURE(kAutofillUseINAddressModel,
             "AutofillUseINAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Italy, overriding the legacy one.
BASE_FEATURE(kAutofillUseITAddressModel,
             "AutofillUseITAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Japan, overriding the legacy one.
BASE_FEATURE(kAutofillSupportPhoneticNameForJP,
             "AutofillSupportPhoneticNameForJP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Poland, overriding the legacy one.
BASE_FEATURE(kAutofillUsePLAddressModel,
             "AutofillUsePLAddressModel",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables detection of language from Translate.
// TODO(crbug.com/40158074): Cleanup when launched.
BASE_FEATURE(kAutofillPageLanguageDetection,
             "AutofillPageLanguageDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, we check whether a field's label or placeholder has the format
// of a valid email address. If it does, we use that as a signal that the field
// is of type EMAIL_ADDRESS.
// TODO(crbug.com/361560365): Clean up when launched.
BASE_FEATURE(kAutofillParseEmailLabelAndPlaceholder,
             "AutofillParseEmailLabelAndPlaceholder",
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

// If the feature is enabled, before triggering suggestion acceptance, the row
// view checks that a substantial portion of its content was visible for some
// minimum required period.
// TODO(crbug.com/337222641): During cleaning up, in the popup row view remove
// emitting of "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough".
BASE_FEATURE(kAutofillPopupDontAcceptNonVisibleEnoughSuggestion,
             "AutofillPopupDontAcceptNonVisibleEnoughSuggestion",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/334909042): Remove after cleanup.
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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows silent profile updates even when the profile import requirements are
// not met.
BASE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport,
             "AutofillSilentProfileUpdateForInsufficientImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Causes Autofill to announce the Compose popup less assertively.
BASE_FEATURE(kComposePopupAnnouncePolitely,
             "ComposePopupAnnouncePolitely",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls an ablation study in which autofill for addresses and payment data
// can be suppressed.
BASE_FEATURE(kAutofillEnableAblationStudy,
             "AutofillEnableAblationStudy",
             base::FEATURE_DISABLED_BY_DEFAULT);
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
// TODO(crbug.com/325452461): Remove once rolled out.
BASE_FEATURE(kAutofillLogDeduplicationMetrics,
             "AutofillLogDeduplicationMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, various deduplication related metrics are logged on startup
// and on import. Used only if `kAutofillLogDeduplicationMetrics` is enabled.
// TODO(crbug.com/325452461): Remove once rolled out.
BASE_FEATURE(kAutofillLogDeduplicationMetricsFollowup,
             "AutofillLogDeduplicationMetricsFollowup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Currently, the importing logic offers new profile creation if the observed
// profile is non-mergeable with any existing profile. With this feature, low-
// quality tokens receive special treatment and can bypass this requirement.
// In particular, if the observed profile was autofilled, except for an edit in
// a single type, this qualifies for an update of the autofilled profile, in
// case the edited type has low-quality.
// TODO(crbug.com/325451601): Remove when launched.
BASE_FEATURE(kAutofillUpdateLowQualityTokenOnImport,
             "AutofillUpdateLowQualityTokenOnImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillUKMExperimentalFields,
             "AutofillUKMExperimentalFields",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

// When enabled, `AutofillProfile` tracks the second and third last use date of
// each profile (instead of just the last use date).
// TODO(crbug.com/354706653): Remove when launched.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kAutofillTrackMultipleUseDates,
             "AutofillTrackMultipleUseDates",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Greek regexes are used for parsing in branded builds.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kAutofillGreekRegexes,
             "AutofillGreekRegexes",
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

// Kill switch for disabling suppressing suggestions based on the strike
// database.
BASE_FEATURE(kAutofillDisableSuggestionStrikeDatabase,
             "AutofillDisableSuggestionStrikeDatabase",
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
// This variation controls whether the verbose version of the feature is used.
// In this version more information is attached to the respective DOM element,
// such as aria labels and descriptions and select element options values and
// texts.
const base::FeatureParam<bool> kAutofillShowTypePredictionsVerboseParam{
    &kAutofillShowTypePredictions, "verbose", false};

// Autofill upload throttling limits uploading a form to the Autofill server
// more than once over a `kAutofillUploadThrottlingPeriodInDays` period.
// This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillUploadThrottling,
             "AutofillUploadThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace test

}  // namespace autofill::features
