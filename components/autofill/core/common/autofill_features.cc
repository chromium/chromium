// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/feature_list.h"

namespace autofill::features {

// LINT.IfChange(autofill_across_iframes_ios)
// Controls whether to flatten and fill cross-iframe forms on iOS.
// TODO(crbug.com/40266699) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframesIos,
             "AutofillAcrossIframesIos",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Throttles child frame extraction to a maximum number of child frames that
// can be extracted by applying the following rules: (1) remove the child frames
// from an individual form that busts the limit and (2) stop extracting child
// frames on other forms once the limit is reached across forms.
BASE_FEATURE(kAutofillAcrossIframesIosThrottling,
             "AutofillAcrossIframesIosThrottling",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_across_iframes_ios)

// Controls whether to trigger form extraction when detecting a form activity on
// a xframe form. Only effective when Autofill is enabled across iframes
// (kAutofillAcrossIframesIos).
BASE_FEATURE(kAutofillAcrossIframesIosTriggerFormExtraction,
             "AutofillAcrossIframesIosTriggerFormExtraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to control displaying of Autofill suggestions on
// unclassified fields based on prefix matching. These suggestions are displayed
// after the user typed a certain number of characters that match some data
// stored in the user's profile.
// TODO(crbug.com/381994105): Cleanup when launched.
BASE_FEATURE(kAutofillAddressSuggestionsOnTyping,
             "AutofillAddressSuggestionsOnTyping",
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

// If enabled (and if `AutofillAiServerModel` is also enabled), this ignores
// the `may_run_server_model` boolean sent by the Autofill server and, instead,
// queries the server model for every encountered form that is not already
// cached locally.
// Only intended for testing.
BASE_FEATURE(kAutofillAiAlwaysTriggerServerModel,
             "AutofillAiAlwaysTriggerServerModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch. If enabled, the EntityDataManager is created irrespective of
// whether other features are enabled. This is necessary so that cleaning up the
// browsing data also removes data if the user left the study.
BASE_FEATURE(kAutofillAiCreateEntityDataManager,
             "AutofillAiCreateEntityDataManager",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// If enabled, no GeoIp requirements are imposed for AutfillAi. Intended for
// Dogfood and testing only.
BASE_FEATURE(kAutofillAiIgnoreGeoIp,
             "AutofillAiIgnoreGeoIp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the client may trigger the server model for AutofillAI type
// predictions.
BASE_FEATURE(kAutofillAiServerModel,
             "AutofillAiServerModel",
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
        base::Seconds(10)};

// Whether AnnotatedPageContent is included in the request to the AutofillAI
// model.
const base::FeatureParam<bool> kAutofillAiServerModelSendPageContent{
    &kAutofillAiServerModel, "autofill_ai_model_send_apc", false};

// Whether the page's full URL is included in the data sent to the model.
const base::FeatureParam<bool> kAutofillAiServerModelSendPageUrl{
    &kAutofillAiServerModel, "autofill_ai_model_send_page_url", false};

// Whether the user may use the locally cached results from the server model
// to provide AutofillAI predictions for filling and importing.
const base::FeatureParam<bool> kAutofillAiServerModelUseCacheResults{
    &kAutofillAiServerModel, "autofill_ai_model_use_cache_results", false};

// If enabled, votes for date format strings from individual fields are
// uploaded. For example, <input type=text value=31/12/2025> leads to the format
// strings DD/MM/YYYY and D/M/YYYY.
BASE_FEATURE(kAutofillAiVoteForFormatStringsFromSingleFields,
             "AutofillAiVoteForFormatStringsFromSingleFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, votes for date format strings from multiple fields are uploaded.
// For example, <input type=text value=31> <input type=text value=12> <input
// type=text value=2025> leads to the format strings DD and D, MM and M, YYYY,
// respectively.
BASE_FEATURE(kAutofillAiVoteForFormatStringsFromMultipleFields,
             "AutofillAiVoteForFormatStringsFromMultipleFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the second iteration AutofillAI.
// This feature is independent of `autofill_ai::kAutofillAi`.
BASE_FEATURE(kAutofillAiWithDataSchema,
             "AutofillAiWithDataSchema",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This parameter enables adding an experiment id to requests to the Autofill
// to enable Autofill AI predictions. The experiment id is not used for other
// backends.
const base::FeatureParam<int> kAutofillAiWithDataSchemaServerExperimentId{
    &kAutofillAiWithDataSchema, "autofill_ai_server_experiment_id", 0};

// When enabled, requests and responses of client-triggered Autofill AI model
// runs are uploaded to MQLS.
BASE_FEATURE(kAutofillAiUploadModelRequestAndResponse,
             "AutofillAiUploadModelRequestAndResponse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Guards the refactoring to allow showing Autofill and Password suggestions in
// the same surface instead of being mutually exclusive.
BASE_FEATURE(kAutofillAndPasswordsInSameSurface,
             "AutofillAndPasswordsInSameSurface",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Same as `kAutofillAddressUserPerceptionSurvey` but for credit card forms.
BASE_FEATURE(kAutofillCreditCardUserPerceptionSurvey,
             "AutofillCreditCardUserPerceptionSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag controlling the display of surveys when a user does not
// accept an Autofill suggestion. The goal is to understand the reason and work
// towards improving acceptance.
BASE_FEATURE(kAutofillAddressUserDeclinedSuggestionSurvey,
             "AutofillAddressUserDeclinedSuggestionSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag controlling the deduplication of GAS addresses. When disabled
// GAS addresses will never be deleted as part of the deduplication flow.
// TODO(crbug.com/357074792): Remove when launched.
BASE_FEATURE(kAutofillDeduplicateAccountAddresses,
             "AutofillDeduplicateAccountAddresses",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables a new implementation for address field parsing that is based on
// backtracking.
BASE_FEATURE(kAutofillEnableAddressFieldParserNG,
             "AutofillEnableAddressFieldParserNG",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug.com/40741721): Remove once shared labels are launched.
BASE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels,
             "AutofillEnableSupportForParsingWithSharedLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if heuristic field parsing should be performed on email-only forms
// without an enclosing form tag.
// TODO(crbug.com/40285735): Remove when/if launched.
BASE_FEATURE(kAutofillEnableEmailHeuristicOutsideForms,
             "AutofillEnableEmailHeuristicOutsideForms",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/40735892): Remove once launched.
BASE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses,
             "AutofillEnableLabelPrecedenceForTurkishAddresses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will help users fill in loyalty card details.
// TODO(crbug.com/395831853): Remove once launched.
BASE_FEATURE(kAutofillEnableLoyaltyCardsFilling,
             "AutofillEnableLoyaltyCardsFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will display joined email and loyalty card Autofill
// suggestions.
// TODO(crbug.com/416664590): Remove once launched.
BASE_FEATURE(kAutofillEnableEmailOrLoyaltyCardsFilling,
             "AutofillEnableEmailOrLoyaltyCardsFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, extracts <input type=date>, which may be filled by Autofill AI.
// This is a kill switch.
// TODO(crbug.com/396325496): Clean up after M137 branch (April 28, 2025).
BASE_FEATURE(kAutofillExtractInputDate,
             "AutofillExtractInputDate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, only non-ad frames are extracted.
// Otherwise, non-ad frames as well as *visible* ad frames are extracted.
// "Extracted" means that FormFieldData::child_frames is populated, which is
// necessary for flattening these forms.
// The forms in those frames are extracted either way.
// TODO(crbug.com/40196220): Remove once launched.
BASE_FEATURE(kAutofillExtractOnlyNonAdFrames,
             "AutofillExtractOnlyNonAdFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, address field swapping suggestions will not include a
// suggestion matching the field's current value. This decreases noises in the
// suggestion UI.
// TODO(crbug.com/381531027): Remove when launched.
BASE_FEATURE(kAutofillImproveAddressFieldSwapping,
             "AutofillImproveAddressFieldSwapping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, new `negative_pattern` regex values will be used
// in order to reduce false positive classifications of city fields.
// TODO(crbug.com/330508437): Clean up when launched.
BASE_FEATURE(kAutofillImproveCityFieldClassification,
             "AutofillImproveCityFieldClassification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will try to reuse the result of previous form
// extractions in subsequent functions that needs the form extracted, provided
// we have guarantees that in the meantime the form couldn't have changed.
BASE_FEATURE(kAutofillOptimizeFormExtraction,
             "AutofillOptimizeFormExtraction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, credit cards that are split into different fields are imported.
// TODO: crbug.com/392179445 - Clean up when launched.
BASE_FEATURE(kAutofillFixSplitCreditCardImport,
             "AutofillFixSplitCreditCardImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, empty autofill settings fields will be correctly saved.
// TODO: crbug.com/402020076 - Clean up when confirmed that this is safe.
BASE_FEATURE(kAutofillFixEmptyFieldAndroidSettingsBug,
             "AutofillFixEmptyFieldAndroidSettingsBug",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, focusing on a credit card number field that was traditionally
// autofilled will yield all credit card suggestions.
// TODO(crbug.com/354175563): Remove when launched.
BASE_FEATURE(kAutofillPaymentsFieldSwapping,
             "AutofillPaymentsFieldSwapping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, and upon receiving a signal that a select element has been
// edited by the user, BrowserAutofillManager will record this correction, which
// will affect many correctness metrics.
BASE_FEATURE(kAutofillRecordCorrectionOfSelectElements,
             "AutofillRecordCorrectionOfSelectElements",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, chrome will support home and work addresses from account.
// TODO: crbug.com/354706653 - Clean up when launched.
BASE_FEATURE(kAutofillEnableSupportForHomeAndWork,
             "AutofillEnableSupportForHomeAndWork",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the autofill suggestion labels are more descriptive and
// relevant.
// TODO(crbug.com/380273791): Cleanup when launched.
BASE_FEATURE(kAutofillImprovedLabels,
             "AutofillImprovedLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If enabled, we include a `FormFieldData`'s maxlength in crowdsourcing votes.
// TODO(crbug.com/393995180): Clean up in M137.
BASE_FEATURE(kAutofillIncludeMaxLengthInCrowdsourcing,
             "AutofillIncludeMaxLengthInCrowdsourcing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we include a <select>'s first, second, and last <option> in
// crowdsourcing votes.
// TODO(crbug.com/393999140): Clean up in M137.
BASE_FEATURE(kAutofillIncludeSelectOptionsInCrowdsourcing,
             "AutofillIncludeSelectOptionsInCrowdsourcing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we include a `FormData`'s URL in crowdsourcing votes.
// TODO(crbug.com/385043924): Clean up in M137.
BASE_FEATURE(kAutofillIncludeUrlInCrowdsourcing,
             "AutofillIncludeUrlInCrowdsourcing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the new suggestion generation logic is used.
// TODO(crbug.com/409962888): Remove once launched.
BASE_FEATURE(kAutofillNewSuggestionGeneration,
             "AutofillNewSuggestionGeneration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, we start forwarding submissions with source
// DOM_MUTATION_AFTER_AUTOFILL, even for non-password forms.
BASE_FEATURE(kAutofillAcceptDomMutationAfterAutofillSubmission,
             "AutofillAcceptDomMutationAfterAutofillSubmission",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Removes logic that resets form submission tracking data upon receiving a
// FORM_SUBMISSION or PROBABLE_FORM_SUBMISSION signal. Also, fixes submission
// deduplication so that it ignores submissions that PWM doesn't act upon.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillFixFormTracking,
             "AutofillFixFormTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses AutofillAgent::GetSubmittedForm() in HTML submissions.
// See `AutofillAgent::GetSubmittedForm()` for more documentation.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillUseSubmittedFormInHtmlSubmission,
             "AutofillUseSubmittedFormInHtmlSubmission",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the ordering for rationalization and sectioning is the same for
// server and heuristic predictions.
// TODO(crbug.com/408497919): Remove when launched.
BASE_FEATURE(kAutofillUnifyRationalizationAndSectioningOrder,
             "AutofillUnifyRationalizationAndSectioningOrder",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Replaces blink::WebFormElementObserver usage in FormTracker by updated logic
// for tracking the disappearance of forms as well as other submission
// triggering events. See `AutofillAgent::GetSubmittedForm()` for more
// documentation.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillPreferSavedFormAsSubmittedForm,
             "AutofillPreferSavedFormAsSubmittedForm",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the import of an Autofill profile if duplicate fields were present
// with identical field values.
// TODO(crbug.com/395855125): Remove when launched.
BASE_FEATURE(kAutofillRelaxAddressImport,
             "AutofillRelaxAddressImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Replaces blink::WebFormElementObserver usage in FormTracker by updated logic
// for tracking the disappearance of forms as well as other submission
// triggering events.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillReplaceFormElementObserver,
             "AutofillReplaceFormElementObserver",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, FormFieldData::is_visible is a heuristic for actual visibility.
// Otherwise, it's an alias for FormFieldData::is_focusable.
// TODO(crbug.com/324199622) When abandoned, remove FormFieldData::is_visible.
BASE_FEATURE(kAutofillDetectFieldVisibility,
             "AutofillDetectFieldVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_disallow_slash_dot_labels)
// Kill switch that adds '/' and '.' to the list of characters of which a label
// must not consist exclusively.
// TODO(crbug.com/396325496): Clean up after after M138 branch (May 26, 2025).
BASE_FEATURE(kAutofillDisallowSlashDotLabels,
             "AutofillDisallowSlashDotLabels",
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_disallow_slash_dot_labels)

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

// Enables using a custom address model for France, overriding the legacy one.
// TODO(crbug.com/347859030): Delete after M139.
BASE_FEATURE(kAutofillUseFRAddressModel,
             "AutofillUseFRAddressModel",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using a custom address model for India, overriding the legacy one.
BASE_FEATURE(kAutofillUseINAddressModel,
             "AutofillUseINAddressModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using a custom address model for Japan, overriding the legacy one.
BASE_FEATURE(kAutofillSupportPhoneticNameForJP,
             "AutofillSupportPhoneticNameForJP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using custom name model with last name prefixes support.
BASE_FEATURE(kAutofillSupportLastNamePrefix,
             "AutofillSupportLastNamePrefix",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch that ignores duplicate AskForValuesToFill() which in
// AutofillAgent as to work around the broken focus-event handling.
// TODO(crbug.com/40284788): Clean up after M138 branch (26 May 2025).
BASE_FEATURE(kAutofillThrottleAskForValuesToFill,
             "AutofillThrottleAskForValuesToFill",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using a custom address model for the Netherlands, overriding the
// legacy one.
BASE_FEATURE(kAutofillUseNLAddressModel,
             "AutofillUseNLAddressModel",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the form field parser won't try to match other attributes if
// any of the negative patterns matched.
BASE_FEATURE(kAutofillUseNegativePatternForAllAttributes,
             "AutofillUseNegativePatternForAllAttributes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, all behaviours related to the on-device machine learning
// model for field type predictions will be guarded.
// TODO(crbug.com/40276177): Remove when launched.
BASE_FEATURE(kAutofillModelPredictions,
             "AutofillModelPredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When true, use the machine learning model as the active `HeuristicSource`,
// else use the source provided by `kAutofillParsingPatternActiveSource`.
// It is defined with `BASE_FEATURE_PARAM()` to enable caching as the parameter
// is accesses in several getters.
BASE_FEATURE_PARAM(bool,
                   kAutofillModelPredictionsAreActive,
                   &kAutofillModelPredictions,
                   "model_active",
                   false);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// When enabled, Greek regexes are used for parsing in branded builds.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kAutofillGreekRegexes,
             "AutofillGreekRegexes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the successful first time creation
// flow.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kPlusAddressAcceptedFirstTimeCreateSurvey,
             "PlusAddressAcceptedFirstTimeCreateSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the declined the first plus
// address creation flow.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kPlusAddressDeclinedFirstTimeCreateSurvey,
             "PlusAddressDeclinedFirstTimeCreateSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user fills a plus address
// after triggering autofill manually.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kPlusAddressFilledPlusAddressViaManualFallbackSurvey,
             "PlusAddressFilledPlusAddressViaManualFallbackSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user creates a 3rd+ plus
// address.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kPlusAddressUserCreatedMultiplePlusAddressesSurvey,
             "PlusAddressUserCreatedMultiplePlusAddressesSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user creates a plus address
// triggering the popup via the Chrome context menu on Desktop or via the
// Keyboard Accessory on Android.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kPlusAddressUserCreatedPlusAddressViaManualFallbackSurvey,
             "PlusAddressUserCreatedPlusAddressViaManualFallbackSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user chooses to fill an email
// when a plus address suggestion is also offered in the Autofill popup.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kPlusAddressUserDidChooseEmailOverPlusAddressSurvey,
             "PlusAddressUserDidChooseEmailOverPlusAddressSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user chooses to fill a plus
// address when an email suggestion is also offered in the Autofill popup.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kPlusAddressUserDidChoosePlusAddressOverEmailSurvey,
             "PlusAddressUserDidChoosePlusAddressOverEmailSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the placeholder is not considered a label fallback on the
// renderer side anymore. Instead, local heuristic will match regexes against
// either the label or the placeholder, depending on how high quality the label
// is. If no matche is found, local heuristics fall back to the other value.
// This feature can be thought of as "lightweight" multi-label support.
// TODO(crbug.com/320965828): Remove when launched.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kAutofillBetterLocalHeuristicPlaceholderSupport,
             "AutofillBetterLocalHeuristicPlaceholderSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the address add/edit editor in the payments request would be
// removed and instead, the address editor from the settings will be used.
// TODO: crbug.com/399071964 - Remove when launched.
COMPONENT_EXPORT(AUTOFILL)
BASE_FEATURE(kUseSettingsAddressEditorInPaymentsRequest,
             "UseSettingsAddressEditorInPaymentsRequest",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// If enabled, other apps can open the Autofill Options in Chrome.
BASE_FEATURE(kAutofillDeepLinkAutofillOptions,
             "AutofillDeepLinkAutofillOptions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls if Chrome Autofill UI surfaces ignore touch events if something is
// fully or partially obscuring the Chrome window.
BASE_FEATURE(kAutofillEnableSecurityTouchEventFilteringAndroid,
             "AutofillEnableSecurityTouchEventFilteringAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill Services can query whether Chrome provides forms as
// virtual view structures to third party providers.
BASE_FEATURE(kAutofillThirdPartyModeContentProvider,
             "AutofillThirdPartyModeContentProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the whether the Chrome may provide a virtual view structure for
// Android Autofill.
// TODO: crbug.com/409579377 - Delete after M139.
BASE_FEATURE(kAutofillVirtualViewStructureAndroid,
             "AutofillVirtualViewStructureAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
// See
// components/autofill/core/browser/crowdsourcing/server_prediction_overrides.h
// for more examples and details on how to specify overrides.
BASE_FEATURE(kAutofillOverridePredictions,
             "AutofillOverridePredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kAutofillServerCommunication,
             "AutofillServerCommunication",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing DOM Node ID of elements.
BASE_FEATURE(kShowDomNodeIDs,
             "ShowDomNodeIDs",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// This variation controls whether the autofill information of the element
// is shown as 'title' of the form field elements. If this parameter is on,
// the title attribute will be overwritten with autofill information.
// By default this is disabled to avoid data collection corruption.
const base::FeatureParam<bool> kAutofillShowTypePredictionsAsTitleParam{
    &kAutofillShowTypePredictions, "as-title", false};

// Autofill upload throttling limits uploading a form to the Autofill server
// more than once over a `kAutofillUploadThrottlingPeriodInDays` period.
// This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillUploadThrottling,
             "AutofillUploadThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace test

}  // namespace autofill::features
