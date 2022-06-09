// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace autofill::features {

// All features in alphabetical order.
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillAcrossIframes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAddressProfileSavePrompt;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAutoBlockSaveAddressProfilePrompt;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptExpirationDays;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptStrikeLimit;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAutoBlockUpdateAddressProfilePrompt;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptExpirationDays;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptStrikeLimit;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature
    kAutofillAddressProfileSavePromptAddressVerificationSupport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAddressProfileSavePromptNicknameSupport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAllowDuplicateFormSubmissions;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAllowNonHttpActivation;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillInferCountryCallingCode;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillComplementCountryCodeOnImport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillConsiderPlaceholderForParsing;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillConsiderVariationCountryCodeForPhoneNumbers;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableWithinFencedFrame;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillCreateDataForTest;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillFillAndImportFromMoreFields;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillFillCreditCardAsPerFormatString;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillDelayPopupControllerDeletion;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillDisableFilling;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillDisableAddressImport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillDisableShadowHeuristics;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableAccountWalletStorage;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableAugmentedPhoneCountryCode;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableCompatibilitySupportForBirthdates;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableDependentLocalityParsing;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableExtendedAddressFormats;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableImportWhenMultiplePhoneNumbers;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableMultiStepImports;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillEnableMultiStepImportComplements;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<base::TimeDelta>
    kAutofillMultiStepImportCandidateTTL;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableParsingEmptyPhoneNumberLabels;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableRankingFormula;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kAutofillRankingFormulaUsageHalfLife;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoost;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillRankingFormulaVirtualCardBoostHalfLife;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForApartmentNumbers;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableLabelPrecedenceForTurkishAddresses;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableNameSurenameParsing;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableProfileDeduplication;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForParsingWithSharedLabels;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForMoreStructureInNames;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForMoreStructureInAddresses;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForMergingSubsetNames;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForHonorificPrefixes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillExtractAllDatalists;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForPhoneNumberTrunkTypes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRefillModifiedCreditCardExpirationDates;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillTypeSpecificPopupWidth;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillFixFillableFieldTypes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillFixServerQueriesIfPasswordManagerIsEnabled;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillIgnoreEarlyClicksOnPopup;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<base::TimeDelta>
    kAutofillIgnoreEarlyClicksOnPopupDuration;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillHighlightOnlyChangedValuesInPreviewMode;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillServerTypeTakesPrecedence;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillKeyboardAccessory;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillLabelAffixRemoval;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillCenterAlignedSuggestions;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillMaximumPixelsToMoveSuggestionopupToCenter;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillMaxiumWidthPercentageToMoveSuggestionPopupToCenter;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillPageLanguageDetection;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillParsingPatternProvider;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillParsingPatternActiveSource;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillPreventOverridingPrefilledValues;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillProbableFormSubmissionInBrowser;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillProfileImportFromUnfocusableFields;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRationalizeStreetAddressAndAddressLine;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRemoveInvalidPhoneNumberOnImport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRemoveInaccessibleProfileValues;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillRemoveInaccessibleProfileValuesOnStartup;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRetrieveOverallPredictionsFromCache;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillSaveAndFillVPA;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillSectionUponRedundantNameInfo;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillServerBehaviors;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kAutofillServerBehaviorsParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillServerCommunication;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillSharedAutofill;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillSharedAutofillRelaxedParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillShowManualFallbackInContextMenu;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillShowTypePredictions;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillSilentProfileUpdateForInsufficientImport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillSkipComparingInferredLabels;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillTokenPrefixMatching;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillUploadThrottling;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseAlternativeStateNameMap;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseImprovedLabelDisambiguation;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseNewSectioningMethod;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseConsistentPopupSettingsIcons;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillVisualImprovementsForSuggestionUi;

COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableAblationStudy;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForAddressesParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForPaymentsParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleParam;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillManualFallbackAndroid;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRefreshStyleAndroid;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillTouchToFillForCreditCardsAndroid;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseMobileLabelDisambiguation;
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterName[];
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterShowOne[];
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterShowAll[];
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_APPLE)
// Returns true if whether the views autofill popup feature is enabled or the
// we're using the views browser.
COMPONENT_EXPORT(AUTOFILL)
bool IsMacViewsAutofillPopupExperimentEnabled();
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
bool IsAutofillManualFallbackEnabled();
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill::features

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
