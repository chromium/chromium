// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace features {

// Controls if Autofill sends votes for the new address types.
const base::Feature kAutofillAddressEnhancementVotes{
    "kAutofillAddressEnhancementVotes", base::FEATURE_DISABLED_BY_DEFAULT};

// By default, AutofillAgent and, if |kAutofillProbableFormSubmissionInBrowser|
// is enabled, also ContentAutofillDriver omit duplicate form submissions, even
// though the form's data may have changed substantially. If enabled, the
// below feature allows duplicate form submissions.
// TODO(crbug/1117451): Remove once the form-submission experiment is over.
const base::Feature kAutofillAllowDuplicateFormSubmissions{
    "AutofillAllowDuplicateFormSubmissions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if a full country name instead of a country code in a field with a
// type derived from HTML_TYPE_COUNTRY_CODE can be used to set the profile
// country.
const base::Feature kAutofillAllowHtmlTypeCountryCodesWithFullNames{
    "AutofillAllowHtmlTypeCountryCodesWithFullNames",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether autofill activates on non-HTTP(S) pages. Useful for
// automated with data URLS in cases where it's too difficult to use the
// embedded test server. Generally avoid using.
const base::Feature kAutofillAllowNonHttpActivation{
    "AutofillAllowNonHttpActivation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillAlwaysFillAddresses{
    "AlwaysFillAddresses", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether negative patterns are used to parse the field type.
// TODO(crbug.com/1132831): Remove once launched.
const base::Feature
    kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics{
        "AutofillApplyNegativePatternsForFieldTypeDetectionHeuristics",
        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the use of GET (instead of POST) to fetch cacheable autofill query
// responses.
const base::Feature kAutofillCacheQueryResponses{
    "AutofillCacheQueryResponses", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillCreateDataForTest{
    "AutofillCreateDataForTest", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we download server credit cards to the ephemeral
// account-based storage when sync the transport is enabled.
const base::Feature kAutofillEnableAccountWalletStorage {
  "AutofillEnableAccountWalletStorage",
#if defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_IOS)
      // Wallet transport is only currently available on Win/Mac/Linux.
      // (Somehow, swapping this check makes iOS unhappy?)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Controls whether to detect and fill the augmented phone country code field
// when enabled.
const base::Feature kAutofillEnableAugmentedPhoneCountryCode{
    "AutofillEnableAugmentedPhoneCountryCode",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we show "Hide suggestions" item in the suggestions menu.
const base::Feature kAutofillEnableHideSuggestionsUI{
    "AutofillEnableHideSuggestionsUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if Autofill supports new structure in names.
// TODO(crbug.com/1098943): Remove once launched.
const base::Feature kAutofillEnableSupportForMoreStructureInNames{
    "AutofillEnableSupportForMoreStructureInNames",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if Autofill supports new structure in addresses.
// TODO(crbug.com/1098943): Remove once launched.
const base::Feature kAutofillEnableSupportForMoreStructureInAddresses{
    "AutofillEnableSupportForMoreStructureInAddresses",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if Autofill supports merging subset names.
// TODO(crbug.com/1098943): Remove once launched.
const base::Feature kAutofillEnableSupportForMergingSubsetNames{
    "AutofillEnableSupportForMergingSubsetNames",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// heuristic field type prediction is run for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForHeuristics{
    "AutofillEnforceMinRequiredFieldsForHeuristics",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// crowd-sourced field type predictions are queried for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForQuery{
    "AutofillEnforceMinRequiredFieldsForQuery",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// field type votes are uploaded to the crowd-sourcing server.
const base::Feature kAutofillEnforceMinRequiredFieldsForUpload{
    "AutofillEnforceMinRequiredFieldsForUpload",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not all datalist shall be extracted into FormFieldData.
// This feature is enabled in both WebView and WebLayer where all datalists
// instead of only the focused one shall be extracted and sent to Android
// autofill service when the autofill session created.
const base::Feature kAutofillExtractAllDatalists{
    "AutofillExtractAllDatalists", base::FEATURE_DISABLED_BY_DEFAULT};

// Autofill uses the local heuristic such that address forms are only filled if
// at least 3 fields are fillable according to local heuristics. Unfortunately,
// the criterion for fillability is only that the field type is unknown. So many
// field types that we don't fill (search term, price, ...) count towards that
// counter, effectively reducing the threshold for some forms.
const base::Feature kAutofillFixFillableFieldTypes{
    "AutofillFixFillableFieldTypes", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, prefilled country and state values are not reset before
// an address profile import.
// TODO(crbug.com/1100231): Remove once fix is tested.
const base::Feature kAutofillImportPrefilledCountryAndStateValues{
    "AutofillImportPrefilledCountryAndStateValues",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, Autofill keeps the initial field values in the |FormStructure|
// cache for all field types.
const base::Feature kAutofillKeepInitialFormValuesInCache{
    "AutofillKeepCachedFormValues", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, Autofill will use FieldRendererIds instead of unique_names
// to align forms in FormStructure::RetrieveFromCache().
const base::Feature kAutofillRetrieveFromCacheWithRendererIds{
    "AutofillRetrieveFromCacheWithRendererIds",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will try to retrieve cached fields by signatures as a
// fallback that is useful if unique renderer ids are unstable.
// TODO(crbug.com/1125624): Remove experiment once trial ended.
const base::Feature kAutofillRetrieveFromCacheWithFieldSignatureAsFallback{
    "AutofillRetrieveFromCacheWithFieldSignatureAsFallback",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, autofill suggestions are displayed in the keyboard accessory
// instead of the regular popup.
const base::Feature kAutofillKeyboardAccessory{
    "AutofillKeyboardAccessory", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, autofill will use new logic to strip both prefixes
// and suffixes when setting FormStructure::parseable_name_
extern const base::Feature kAutofillLabelAffixRemoval{
    "AutofillLabelAffixRemoval", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillPruneSuggestions{
    "AutofillPruneSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillMetadataUploads{"AutofillMetadataUploads",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillOffNoServerData{"AutofillOffNoServerData",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If feature is enabled, autofill will be disabled for mixed forms (forms on
// HTTPS sites that submit over HTTP).
const base::Feature kAutofillPreventMixedFormsFilling{
    "AutofillPreventMixedFormsFilling", base::FEATURE_DISABLED_BY_DEFAULT};

// If the feature is enabled, FormTracker's probable-form-submission detection
// is disabled and replaced with browser-side detection.
// TODO(crbug/1117451): Remove once it works.
const base::Feature kAutofillProbableFormSubmissionInBrowser{
    "AutofillProbableFormSubmissionInBrowser",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillProfileClientValidation{
    "AutofillProfileClientValidation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillProfileImportFromUnifiedSection{
    "AutofillProfileImportFromUnifiedSection",
    base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1101280): Remove once feature is tested.
const base::Feature kAutofillProfileImportFromUnfocusableFields{
    "AutofillProfileImportFromUnfocusableFields",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill uses server-side validation to ensure that fields
// with invalid data are not suggested.
const base::Feature kAutofillProfileServerValidation{
    "AutofillProfileServerValidation", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not a group of fields not enclosed in a form can be
// considered a form. If this is enabled, unowned fields will only constitute
// a form if there are signals to suggest that this might a checkout page.
const base::Feature kAutofillRestrictUnownedFieldsToFormlessCheckout{
    "AutofillRestrictUnownedFieldsToFormlessCheckout",
    base::FEATURE_DISABLED_BY_DEFAULT};

// On Canary and Dev channels only, this feature flag instructs chrome to send
// rich form/field metadata with queries. This will trigger the use of richer
// field-type predictions model on the server, for testing/evaluation of those
// models prior to a client-push.
const base::Feature kAutofillRichMetadataQueries{
    "AutofillRichMetadataQueries", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether UPI/VPA values will be saved and filled into payment forms.
const base::Feature kAutofillSaveAndFillVPA{"AutofillSaveAndFillVPA",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables creating a new form section when an unstructured name input
// containing a |NAME_LAST| field is encountered after a structured name input.
const base::Feature kAutofillSectionUponRedundantNameInfo{
    "AutofillSectionUponRedundantNameInfo", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
const base::Feature kAutofillServerCommunication{
    "AutofillServerCommunication", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
const base::Feature kAutofillShowTypePredictions{
    "AutofillShowTypePredictions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether inferred label is considered for comparing in
// FormFieldData.SimilarFieldAs.
const base::Feature kAutofillSkipComparingInferredLabels{
    "AutofillSkipComparingInferredLabels", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to skip fields whose last seen value differs from the
// initially value.
const base::Feature kAutofillSkipFillingFieldsWithChangedValues{
    "AutofillSkipFillingFieldsWithChangedValues",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill should search prefixes of all words/tokens when
// filtering profiles, or only on prefixes of the whole string.
const base::Feature kAutofillTokenPrefixMatching{
    "AutofillTokenPrefixMatching", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the touch to fill feature for Android.
const base::Feature kAutofillTouchToFill = {"TouchToFillAndroid",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Autofill upload throttling is used for testing.
const base::Feature kAutofillUploadThrottling{"AutofillUploadThrottling",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether suggestions' labels use the improved label disambiguation
// format.
const base::Feature kAutofillUseImprovedLabelDisambiguation{
    "AutofillUseImprovedLabelDisambiguation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to use the combined heuristic and the autocomplete section
// implementation for section splitting or not. See https://crbug.com/1076175.
const base::Feature kAutofillUseNewSectioningMethod{
    "AutofillUseNewSectioningMethod", base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1075604): Remove once launched.
// Controls whether the page language is used as a fall-back locale to translate
// the country name when a profile is imported from a form.
const base::Feature kAutofillUsePageLanguageToTranslateCountryNames{
    "AutofillUsePageLanguageToTranslateCountryNames",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to use the |ParseCityStateCountryZipCode| or not for
// predicting the heuristic type.
// |ParseCityStateCountryZipCode| is intended to prevent the misclassification
// of the country field into |ADDRESS_HOME_STATE| while determining the
// heuristic type. The misclassification happens sometimes because the regular
// expression for |ADDRESS_HOME_STATE| contains the term "region" which is also
// used for country selectors.
const base::Feature kAutofillUseParseCityStateCountryZipCodeInHeuristic{
    "AutofillUseParseCityStateCountryZipCodeInHeuristic",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not autofill utilizes the country code from the Chrome
// variation service. The country code is used for determining the address
// requirements for address profile creation and as source for a default country
// used in a new address profile.
const base::Feature kAutofillUseVariationCountryCode{
    "AutofillUseVariationCountryCode", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Controls whether the Autofill manual fallback for Addresses and Payments is
// present on Android.
const base::Feature kAutofillManualFallbackAndroid{
    "AutofillManualFallbackAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to use modernized style for the Autofill dropdown.
const base::Feature kAutofillRefreshStyleAndroid{
    "AutofillRefreshStyleAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

#endif  // OS_ANDROID

#if defined(OS_ANDROID) || defined(OS_IOS)
const base::Feature kAutofillUseMobileLabelDisambiguation{
    "AutofillUseMobileLabelDisambiguation", base::FEATURE_DISABLED_BY_DEFAULT};
const char kAutofillUseMobileLabelDisambiguationParameterName[] = "variant";
const char kAutofillUseMobileLabelDisambiguationParameterShowAll[] = "show-all";
const char kAutofillUseMobileLabelDisambiguationParameterShowOne[] = "show-one";
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_IOS)
// Controls whether or not autofill uses numeric renderer IDs instead of string
// form and field identifiers in filling logic.
// TODO(crbug/1131038): Remove once it's launched.
const base::Feature kAutofillUseUniqueRendererIDsOnIOS{
    "AutofillUseUniqueRendererIDsOnIOS", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace autofill
