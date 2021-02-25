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
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace features {

// Controls if Autocomplete suggestions are only shown/stored for meaningful
// field names.
// TODO(crbug.com/1181759): Remove once launched.
const base::Feature kAutocompleteFilterForMeaningfulNames{
    "AutocompleteFilterForMeaningfulNames", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if Autofill sends votes for the new address types.
const base::Feature kAutofillAddressEnhancementVotes{
    "kAutofillAddressEnhancementVotes", base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, a save prompt will be shown to user upon form submission before
// storing any detected address profile.
const base::Feature kAutofillAddressProfileSavePrompt{
    "AutofillAddressProfileSavePrompt", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Controls the use of GET (instead of POST) to fetch cacheable autofill query
// responses.
const base::Feature kAutofillCacheQueryResponses{
    "AutofillCacheQueryResponses", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillCreateDataForTest{
    "AutofillCreateDataForTest", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug/1165780): Remove once shared labels are launched.
const base::Feature kAutofillEnableSupportForParsingWithSharedLabels{
    "AutofillEnableSupportForParsingWithSharedLabels",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for Autofill filling.
const base::Feature kAutofillDisableFilling{"AutofillDisableFilling",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for Autofill address import.
const base::Feature kAutofillDisableAddressImport{
    "AutofillDisableAddressImport", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if Chrome support filling and importing apartment numbers.
// TODO(crbug.com/1153715): Remove once launched.
const base::Feature kAutofillEnableSupportForApartmentNumbers{
    "AutofillEnableSupportForApartmentNumbers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we download server credit cards to the ephemeral
// account-based storage when sync the transport is enabled.
const base::Feature kAutofillEnableAccountWalletStorage {
  "AutofillEnableAccountWalletStorage",
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_IOS)
      // Wallet transport is only currently available on Win/Mac/Linux/Android.
      // (Somehow, swapping this check makes iOS unhappy?)
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

// Controls if Autofill parses ADDRESS_HOME_DEPENDENT_LOCALITY.
// TODO(crbug.com/1157405): Remove once launched.
const base::Feature kAutofillEnableDependentLocalityParsing{
    "AutofillEnableDependentLocalityParsing",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we show "Hide suggestions" item in the suggestions menu.
const base::Feature kAutofillEnableHideSuggestionsUI{
    "AutofillEnableHideSuggestionsUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to save the first number in a form with multiple phone
// numbers instead of aborting the import.
// TODO(crbug.com/1167484) Remove once launched
const base::Feature kAutofillEnableImportWhenMultiplePhoneNumbers{
    "AutofillEnableImportWhenMultiplePhoneNumbers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled and user has single account, a footer indicating user's e-mail
// address and profile picture will appear at the bottom of InfoBars which has
// corresponding account indication footer flags on.
const base::Feature
    kAutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers{
        "AutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers",
        base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled and user is syncing, a footer indicating user's e-mail address
// and profile picture will appear at the bottom of InfoBars which has
// corresponding account indication footer flags on.
const base::Feature kAutofillEnableInfoBarAccountIndicationFooterForSyncUsers{
    "AutofillEnableInfoBarAccountIndicationFooterForSyncUsers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/1156315): Remove once launched.
const base::Feature kAutofillEnableLabelPrecedenceForTurkishAddresses{
    "AutofillEnableLabelPrecedenceForTurkishAddresses",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled and user is signed in, a footer indicating user's e-mail address
// and profile picture will appear at the bottom of corresponding password
// InfoBars.
const base::Feature kAutofillEnablePasswordInfoBarAccountIndicationFooter{
    "AutofillEnablePasswordInfoBarAccountIndicationFooter",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the address profile deduplication logic runs after the browser
// startup, once per chrome version.
const base::Feature kAutofillEnableProfileDeduplication{
    "AutofillEnableProfileDeduplication", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Controls whether honorific prefix is shown and editable in Autofill Settings
// on Android, iOS and Desktop.
// TODO(crbug.com/1141460): Remove once launched.
const base::Feature kAutofillEnableSupportForHonorificPrefixes{
    "AutofillEnableSupportForHonorificPrefixes",
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

// Controls if a server prediction with a prediction source |OVERRIDE| is
// granted precedence over html type attributes.
// TODO(crbug.com/1170384) Remove once launched
const base::Feature kAutofillServerTypeTakesPrecedence{
    "AutofillServerTypeTakesPrecedence", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will use FormRendererIds instead of
// GetIdentifierForRefill() to identify forms during refills.
// TODO(crbug/896689): Remove once experiment is finished.
const base::Feature kAutofillRefillWithRendererIds{
    "AutofillRefillWithRendererIds", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will use FormRendererIds instead of
// unique_name() to create unique section names.
// TODO(crbug/896689): Remove once experiment is finished.
const base::Feature kAutofillNameSectionsWithRendererIds{
    "AutofillNameSectionsWithRendererIds", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill suggestions are displayed in the keyboard accessory
// instead of the regular popup.
const base::Feature kAutofillKeyboardAccessory{
    "AutofillKeyboardAccessory", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will use new logic to strip both prefixes
// and suffixes when setting FormStructure::parseable_name_
extern const base::Feature kAutofillLabelAffixRemoval{
    "AutofillLabelAffixRemoval", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillPruneSuggestions{
    "AutofillPruneSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillMetadataUploads{"AutofillMetadataUploads",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill will load remote patterns via the component updater.
// TODO(crbug/1121990): Remove once launched.
extern const base::Feature kAutofillParsingPatternsFromRemote{
    "AutofillParsingPatternsFromRemote", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables detection of language from Translate.
// TODO(crbug/1150895): Cleanup when launched.
const base::Feature kAutofillParsingPatternsLanguageDetection{
    "AutofillParsingPatternsLanguageDetection",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether negative patterns are used to parse the field type.
// TODO(crbug.com/1132831): Remove once launched.
const base::Feature kAutofillParsingPatternsNegativeMatching{
    "AutofillParsingPatternsNegativeMatching",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether page language is used to match patterns.
// TODO(crbug.com/1134496): Remove once launched.
const base::Feature kAutofillParsingPatternsLanguageDependent{
    "AutofillParsingPatternsLanguageDependent",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If feature is enabled, Autofill will be disabled for mixed forms (forms on
// HTTPS sites that submit over HTTP).
const base::Feature kAutofillPreventMixedFormsFilling{
    "AutofillPreventMixedFormsFilling", base::FEATURE_ENABLED_BY_DEFAULT};

// If the feature is enabled, FormTracker's probable-form-submission detection
// is disabled and replaced with browser-side detection.
// TODO(crbug/1117451): Remove once it works.
const base::Feature kAutofillProbableFormSubmissionInBrowser{
    "AutofillProbableFormSubmissionInBrowser",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillProfileClientValidation{
    "AutofillProfileClientValidation", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Controls whether or not overall prediction are retrieved from the cache.
const base::Feature kAutofillRetrieveOverallPredictionsFromCache{
    "AutofillRetrieveOverallPredictionsFromCache",
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

// Controls whether we require an expiration date or verification field when a
// name field is detected for a credit card, but we aren't confident it's not
// a non-credit card specific name field.
const base::Feature kAutofillStrictContextualCardNameConditions{
    "AutofillStrictContextualCardNameConditions",
    base::FEATURE_DISABLED_BY_DEFAULT};

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
    "AutofillUseAlternativeStateNameMap", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether suggestions' labels use the improved label disambiguation
// format.
const base::Feature kAutofillUseImprovedLabelDisambiguation{
    "AutofillUseImprovedLabelDisambiguation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to use the combined heuristic and the autocomplete section
// implementation for section splitting or not. See https://crbug.com/1076175.
const base::Feature kAutofillUseNewSectioningMethod{
    "AutofillUseNewSectioningMethod", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Controls whether the creation of new address profiles is enabled in settings
// on IOS.
// TODO(crbug/1167105): Remove once it's launched.
const base::Feature kAutofillEnableNewAddressProfileCreationInSettingsOnIOS{
    "AutofillEnableNewAddressProfileCreationInSettingsOnIOS",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// Controls whether Android autofill (WebView and WebLayer) should query the
// Autofill server for the server field type predictions and send them to
// Android autofill service.
const base::Feature kAndroidAutofillQueryServerFieldTypes{
    "AndroidAutofillQueryServerFieldTypes", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the Wallet (GPay) integration requires first-sync-setup to
// be complete.
// TODO(crbug.com/1134564): Clean up after launch.
const base::Feature kWalletRequiresFirstSyncSetupComplete{
    "WalletRequiresFirstSyncSetupComplete", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace autofill
