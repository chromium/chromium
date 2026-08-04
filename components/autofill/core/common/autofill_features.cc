// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/feature_list.h"

namespace autofill::features {

// The Wallet private passes integration is only launched outside these
// countries on desktop and android. On iOS, it's only launched in the US.
#define WALLET_UNSUPPORTED_COUNTRIES                                          \
  "ao", "at", "au", "be", "bg", "br", "ca", "ch", "cy", "cz", "de", "dk",     \
      "dz", "ee", "es", "fi", "fr", "gb", "gr", "hr", "hu", "id", "ie", "in", \
      "is", "it", "jp", "kr", "li", "lt", "lu", "lv", "md", "mk", "ml", "mt", \
      "nl", "no", "om", "pl", "pt", "ro", "se", "si", "sk", "th"

// Like DECLARE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION but for the definition.
// Used for certain AutofillAi features, which are launched globally on desktop
// but only in certain countries on mobile.
// Note that even on desktop, the Wallet private passes integration is only
// launched outside of WALLET_UNSUPPORTED_COUNTRIES.
#if BUILDFLAG(IS_ANDROID)
#define DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(feature_name)           \
  BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(feature_name,                         \
                                         base::FEATURE_DISABLED_FOR_COUNTRIES, \
                                         WALLET_UNSUPPORTED_COUNTRIES)
#elif BUILDFLAG(IS_IOS)
#define DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(feature_name) \
  BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(                            \
      feature_name, base::FEATURE_ENABLED_FOR_COUNTRIES, "us")
#else
#define DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(feature_name) \
  BASE_FEATURE(feature_name, base::FEATURE_ENABLED_BY_DEFAULT)
#endif

BASE_FEATURE(kActorFormFillingServiceEnableAddress,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kActorFormFillingServiceEnableCreditCard,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we start forwarding submissions with source
// DOM_MUTATION_AFTER_AUTOFILL, even for non-password forms.
BASE_FEATURE(kAutofillAcceptDomMutationAfterAutofillSubmission,
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_across_iframes_ios)
// Controls whether to flatten and fill cross-iframe forms on iOS.
// TODO(crbug.com/40266699) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframesIos, base::FEATURE_ENABLED_BY_DEFAULT);

// Throttles child frame extraction to a maximum number of child frames that
// can be extracted by applying the following rules: (1) remove the child frames
// from an individual form that busts the limit and (2) stop extracting child
// frames on other forms once the limit is reached across forms.
BASE_FEATURE(kAutofillAcrossIframesIosThrottling,
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_across_iframes_ios)

// Controls whether to trigger form extraction when detecting a form activity on
// a xframe form. Only effective when Autofill is enabled across iframes
// (kAutofillAcrossIframesIos).
BASE_FEATURE(kAutofillAcrossIframesIosTriggerFormExtraction,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, ActorFormFillingServiceImpl will attempt to split requests for a
// form section fill (when relevant) into two sub-fills - one for a "contact
// info" sub-section and one for an "address" sub-section.
BASE_FEATURE(kAutofillActorFormFillingSplitOutContactInfo,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch: If enabled, `ChromeAutofillClient` may enter the actor mode,
// changing how the UI, parsing and the server predictions work. For more
// context see: go/autofill-actor-mode-implementation
BASE_FEATURE(kAutofillActorMode, base::FEATURE_ENABLED_BY_DEFAULT);

// This parameter configures a preamble which is returned in the
// extra_information attribute of the return value of attempt_form_filling. It
// can be used to pass instructions or information to the model.
BASE_FEATURE_PARAM(
    std::string,
    kAutofillActorModeExtraInformationPreamble,
    &kAutofillActorMode,
    "The user chose to fill the following information into the form:");

// Controls whether to rewrite the credit card trigger field to the first
// credit card number field in the same section.
BASE_FEATURE(kAutofillActorRewriteCreditCardTriggerField,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether all import tasks are suppressed when an Actor task is
// active on the tab in question. This also suppresses silent updates and
// saving to Autocomplete.
BASE_FEATURE(kAutofillActorSuppressImport, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag to control displaying of Autofill suggestions on
// unclassified fields based on prefix matching. These suggestions are displayed
// after the user typed a certain number of characters that match some data
// stored in the user's profile.
// TODO(crbug.com/381994105): Cleanup when launched.
BASE_FEATURE(kAutofillAddressSuggestionsOnTyping,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This parameter enables updating the minimum number of characters a user needs
// to type to maybe see an Autofill on typing suggestion.
BASE_FEATURE_PARAM(int,
                   kAutofillOnTypingMinNumberCharactersToMatch,
                   &kAutofillAddressSuggestionsOnTyping,
                   "min_number_characters_to_match",
                   3);

// This parameter enables updating the maximum number of characters typed until
// Autofill on typing suggestions are no longer displayed.
BASE_FEATURE_PARAM(int,
                   kAutofillOnTypingMaxNumberCharactersToMatch,
                   &kAutofillAddressSuggestionsOnTyping,
                   "max_number_characters_to_match",
                   10);

// This parameter enables updating the required number of characters that need
// to be missing between the typed data and the profile data. This makes sure
// the value offered by the feature is higher, by for example not displaying a
// suggestion to fill "Tomas" when the user typed "Tom", since at this point
// users are more likely to simply finish typing.
BASE_FEATURE_PARAM(int,
                   kAutofillOnTypingMinMissingCharactersNumber,
                   &kAutofillAddressSuggestionsOnTyping,
                   "min_missing_characters_number",
                   5);

// This parameter enables updating the field types offered in Autofill on typing
// suggestions. Field types are defined as enums, so this parameter should be a
// string of integers separated by dash, such as "34-22-44-11". If the string
// cannot be parsed or some value is out of bound of the field types enum, the
// param is ignored. When this param is an empty string (default value), a
// default list of field types is used.
BASE_FEATURE_PARAM(std::string,
                   kAutofillOnTypingFieldTypes,
                   &kAutofillAddressSuggestionsOnTyping,
                   "field_types",
                   "");

// This parameter controls whether Autofill on typing suggestions should be
// displayed only on unclassified fields.
BASE_FEATURE_PARAM(bool,
                   kAutofillOnTypingAllowOnlyOnUnclassifiedFields,
                   &kAutofillAddressSuggestionsOnTyping,
                   "allow_only_on_unclassified_fields",
                   false);

// Feature flag to controls whether Autofill on typing suggestions will have a
// strike database.
BASE_FEATURE(kAutofillAddressSuggestionsOnTypingHasStrikeDatabase,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag controlling the display of surveys when a user declines the
// save prompt of Autofill address and a user does not have any address stored.
// The goal is to understand the reason and work towards improving acceptance.
BASE_FEATURE(kAutofillAddressUserDeclinedSaveSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag controlling the display of surveys when a user does not
// accept an Autofill suggestion. The goal is to understand the reason and work
// towards improving acceptance.
BASE_FEATURE(kAutofillAddressUserDeclinedSuggestionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag to control the displaying of an ongoing hats survey that
// measures users perception of Autofill. Differently from other surveys,
// the Autofill user perception survey will not have a specific target
// number of answers where it will be fully stop, instead, it will run
// indefinitely. A target number of full answers exists, but per quarter. The
// goal is to have a go to place to understand how users are perceiving autofill
// across quarters.
BASE_FEATURE(kAutofillAddressUserPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled (and if `AutofillAiServerModel` is also enabled), this ignores
// the `may_run_server_model` boolean sent by the Autofill server and, instead,
// queries the server model for every encountered form that is not already
// cached locally.
// Only intended for testing.
BASE_FEATURE(kAutofillAiAlwaysTriggerServerModel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled Autofill AI becomes available by default and the previous enable
// toggle controls whether online model runs and MQLS logging are allowed.
// TODO(crbug.com/440488776): Remove once clean up happens.
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(kAutofillAiAvailableByDefault);

// If enabled, AutofillAi entities will be deduped on every major milestone.
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(kAutofillAiDedupeEntities);

#if BUILDFLAG(IS_ANDROID)
// When enabled, the entity save/update prompt displays an edit button that
// opens the entity editor on click.
BASE_FEATURE(kAutofillAiEditEntitiesFromSaveUpdatePrompt,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, a HaTS survey is shown after a walletable suggestion is
// displayed and the form submitted. The survey does not require the suggestion
// to be accepted.
BASE_FEATURE(kAutofillAiFillingSurvey, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, no GeoIp requirements are imposed for AutofillAi.
// Note that this feature can be modified as follows (all assuming that
// `kAutofillAiIgnoreGeoIp` is enabled):
// - If both `kAutofillAiIgnoreGeoIpAllowlist` and
//   `kAutofillAiIgnoreGeoIpBlocklist` are empty, then all geo IPs are
//   permitted.
// - If only `kAutofillAiIgnoreGeoIpBlocklist` is non-empty, then all geo ips
//   but those in `kAutofillAiIgnoreGeoIpBlocklist` are permitted.
// - If `kAutofillAiIgnoreGeoIpAllowlist` is non-empty, then only geo ips in
//   `kAutofillAiIgnoreGeoIpAllowlist` are permitted.
//
// Both the allowlist and the blocklist are expected to consist of
// comma-separated uppercase two-digit country codes (see documentation of
// `GeoIpCountryCode`.)
BASE_FEATURE(kAutofillAiIgnoreGeoIp, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kAutofillAiIgnoreGeoIpAllowlist,
                   &kAutofillAiIgnoreGeoIp,
                   "autofill_ai_geo_ip_allowlist",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAutofillAiIgnoreGeoIpBlocklist,
                   &kAutofillAiIgnoreGeoIp,
                   "autofill_ai_geo_ip_blocklist",
                   "");

// If enabled, Autofill AI suggestion width can be limited.
BASE_FEATURE(kAutofillAiLimitSuggestionWidth,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill AI will use a new update prompt on Desktop that shows
// both the previous and the new value of an updated entity attribute.
BASE_FEATURE(kAutofillAiNewUpdatePrompt, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Autofill AI filling suggestion do not have an icon.
BASE_FEATURE(kAutofillAiNoFillingIconsExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill AI Settings Page title uses "Smarter form understanding"
// instead of "Enhanced autofill".
BASE_FEATURE(kAutofillAiOnlineModelToggleNewTitle,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, AutofillAi supports order entities.
BASE_FEATURE(kAutofillAiOrder, base::FEATURE_ENABLED_BY_DEFAULT);

// When inference through the non-PI AutofillAi online model is run (implying
// that AutofillAiUsePrivateAi is disabled) and this flag is enabled, an
// additional inference request through the PI stack is run to compute shadow
// metrics between the two results.
BASE_FEATURE(kAutofillAiPrivateAiShadowMetric,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi requires re-auth when filling/viewing sensitive
// fields. As part of this feature sensitive fields are also obfuscated during
// suggestion generation time.
// TODO(crbug.com/468236932): Remove once feature is launched.
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(kAutofillAiReauthRequired);

// When enabled, a HaTS survey is shown after the save prompt for a walletable
// entity was interacted with.
BASE_FEATURE(kAutofillAiSavePromptSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    std::string,
    kAutofillAiSavePromptSurveyAcceptedTriggerId,
    &kAutofillAiSavePromptSurvey,
    "autofill_ai_walletable_entity_save_prompt_survey_accepted_trigger_id",
    "");
BASE_FEATURE_PARAM(
    std::string,
    kAutofillAiSavePromptSurveyDeclinedTriggerId,
    &kAutofillAiSavePromptSurvey,
    "autofill_ai_walletable_entity_save_prompt_survey_declined_trigger_id",
    "");

// If enabled, the client may trigger the server model for AutofillAI type
// predictions.
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(kAutofillAiServerModel);

// The maximum duration for which an AutofillAI server model response is kept in
// the local cache. NOTE: It is advisable to choose a value that is at least as
// large as the cache duration for Autofill server responses to limit cases in
// which the model is run multiple times for the same form.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutofillAiServerModelCacheAge,
                   &kAutofillAiServerModel,
                   "autofill_ai_model_cache_age",
                   base::Days(7));

// The maximum size of the AutofillAI server model cache.
BASE_FEATURE_PARAM(int,
                   kAutofillAiServerModelCacheSize,
                   &kAutofillAiServerModel,
                   "autofill_ai_model_cache_size",
                   100);

// The timeout for running the AutofillAI server model.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutofillAiServerModelExecutionTimeout,
                   &kAutofillAiServerModel,
                   "autofill_ai_model_execution_timeout",
                   base::Seconds(60));

// Whether AnnotatedPageContent is included in the request to the AutofillAI
// model.
BASE_FEATURE_PARAM(bool,
                   kAutofillAiServerModelSendPageContent,
                   &kAutofillAiServerModel,
                   "autofill_ai_model_send_apc",
                   true);

// Whether the page's full URL is included in the data sent to the model.
BASE_FEATURE_PARAM(bool,
                   kAutofillAiServerModelSendPageUrl,
                   &kAutofillAiServerModel,
                   "autofill_ai_model_send_page_url",
                   false);

// Whether the user may use the locally cached results from the server model
// to provide AutofillAI predictions for filling and importing.
BASE_FEATURE_PARAM(bool,
                   kAutofillAiServerModelUseCacheResults,
                   &kAutofillAiServerModel,
                   "autofill_ai_model_use_cache_results",
                   true);

// If enabled, AutofillAi supports shipment entities.
BASE_FEATURE(kAutofillAiShipment, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// If enabled, the user is notified about a failure to upstream data to Wallet
// via a dialog instead of a snackbar.
BASE_FEATURE(kAutofillAiShowDialogInSettingsWhenUpstreamingFails,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Controls whether a banner is shown in settings when wallet data sharing is
// disabled.
BASE_FEATURE(kAutofillAiShowWalletDisabledBanner,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the client may trigger the server model for AutofillAI type
// predictions using Private AI Compute.
BASE_FEATURE(kAutofillAiUsePrivateAi, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, votes for the format of flight number fields are uploaded. For
// example, if there is a flight number "LH89" on file, a submitted value of
// "89" on a field with type `FLIGHT_RESERVATION_FLIGHT_NUMBER` uploads "N".
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(
    kAutofillAiVoteForFormatStringsForFlightNumbers);

// If enabled, AutofillAi supports flight reservation entities from Google
// Wallet.
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(
    kAutofillAiWalletFlightReservation);

// Enables the 2026 Autofill AI Wallet Pass Branding Updates.
BASE_FEATURE(kAutofillAiWalletPassBranding2026,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi supports private passes entities from Google Wallet.
#if BUILDFLAG(IS_IOS)
BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kAutofillAiWalletPrivatePasses,
                                       base::FEATURE_ENABLED_FOR_COUNTRIES,
                                       "us");
#else
BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kAutofillAiWalletPrivatePasses,
                                       base::FEATURE_DISABLED_FOR_COUNTRIES,
                                       WALLET_UNSUPPORTED_COUNTRIES);
#endif

// When enabled, account location rather than geo-location is used to determine
// the eligiblity to save Wallet private passes.
BASE_FEATURE(kAutofillAiWalletPrivatePassesCapability,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Wallet private pass entries in settings link to their pass
// details page rather than the generic pass overview page.
BASE_FEATURE(kAutofillAiWalletPrivatePassesDeepLink,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, orders and shipments from Google Wallet become available in
// Autofill for filling as read-only AutofillAi entities.
// TODO(crbug.com/542022094): Clean up when launched.
BASE_FEATURE(kAutofillAiWalletShopping, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillAi supports vehicle registration entities from Google
// Wallet.
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(
    kAutofillAiWalletVehicleRegistration);

// Enables the second iteration AutofillAI.
DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION(kAutofillAiWithDataSchema);

// When enabled, autofill will fill not skip filling fields that had an initial
// value which was modified.
BASE_FEATURE(kAutofillAllowFillingModifiedInitialValues,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the ambient autofill experience is enabled in Chrome.
BASE_FEATURE(kAutofillAmbientAutofill, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kAutofillAmbientAutofillEligibleTiers,
                   &kAutofillAmbientAutofill,
                   "ambient_autofill_eligible_tiers",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAutofillAmbientAutofillEnabledDevices,
                   &kAutofillAmbientAutofill,
                   "ambient_autofill_enabled_devices",
                   "");
// The TTL for prefetched (masked/non-SPII) entities and presence signals.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutofillAmbientAutofillPrefetchedEntitiesAndSignalsCacheTTL,
                   &kAutofillAmbientAutofill,
                   "ambient_autofill_prefetched_entities_cache_ttl",
                   base::Minutes(30));
// The TTL for unmasked sensitive PII (SPII) entities.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutofillAmbientAutofillUnmaskedSpiiCacheTTL,
                   &kAutofillAmbientAutofill,
                   "ambient_autofill_unmasked_spii_cache_ttl",
                   base::Minutes(1));

// If enabled, on Android desktop, the Autofill keyboard accessory will have a
// new behavior and design.
// TODO(crbug.com/438125774): Remove when launched.
BASE_FEATURE(kAutofillAndroidDesktopKeyboardAccessoryRevamp,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, on Android desktop, Autofill keyboard accessory will be
// suppressed when there are no autofill suggestions.
BASE_FEATURE(kAutofillAndroidDesktopSuppressAccessoryOnEmpty,
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, on Android, the Autofill keyboard accessory will not be
// displayed attached to the keyboard but will be placed below or above the
// focused field. It works only for large form factor devices like tablets or
// desktops.
// TODO(crbug.com/438125774): Remove when launched.
BASE_FEATURE(kAutofillAndroidKeyboardAccessoryDynamicPositioning,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag for kAutofillAtMemory.
BASE_FEATURE(kAutofillAtMemory, base::FEATURE_DISABLED_BY_DEFAULT);

// The subscription tiers for which AtMemory is eligible. Comma-separated list
// of subscription tier integers. If empty/not defined, no tier restrictions
// are applied.
BASE_FEATURE_PARAM(std::string,
                   kAutofillAtMemoryEligibleTiers,
                   &kAutofillAtMemory,
                   "at_memory_eligible_tiers",
                   "");

// The Android devices for which AtMemory is enabled. Comma-separated list
// of HardwareModelNames. If empty/not defined, no device restrictions are
// applied (i.e. only tier-based restrictions apply).
BASE_FEATURE_PARAM(std::string,
                   kAutofillAtMemoryEnabledDevices,
                   &kAutofillAtMemory,
                   "at_memory_enabled_devices",
                   "");

// The timeout for `PersonalContextService` requests in `AtMemoryQueryService`.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutofillAtMemoryRequestTimeout,
                   &kAutofillAtMemory,
                   base::Seconds(30));

// Controls whether the Autosuggest nudging logic is used. If enabled, user are
// encouraged to use the AtMemory feature.
BASE_FEATURE(kAutofillAtMemoryInactivityNudge,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AtMemory can be triggered with a keyboard shortcut like
// Ctrl+Space.
BASE_FEATURE(kAutofillAtMemoryTriggerShortcut,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether AtMemory uses the strongly-typed AutofillFetchPlan.
BASE_FEATURE(kAutofillAtMemoryTypedFetchPlan,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the placeholder is not considered a label fallback on the
// renderer side anymore. Instead, local heuristic will match regexes against
// either the label or the placeholder, depending on how high quality the label
// is. If no matche is found, local heuristics fall back to the other value.
// This feature can be thought of as "lightweight" multi-label support.
// TODO(crbug.com/320965828): Remove when launched.
BASE_FEATURE(kAutofillBetterLocalHeuristicPlaceholderSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, deduce country of a new address profile based on the phone
// number if not explicitly observed.
BASE_FEATURE(kAutofillComplementCountryUsingPhoneNumber,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Same as `kAutofillAddressUserPerceptionSurvey` but for credit card forms.
BASE_FEATURE(kAutofillCreditCardUserPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, `FormPredictionsTracker` will wait up to 1 second
// for Autofill to finish parsing forms pesent on a given tab before capturing
// APC. For more context see: go/autofill-actor-mode-implementation
// TODO(crbug.com/479794574): Convert to killswitch if no regressions are
// spotted.
BASE_FEATURE(kAutofillDelayApcForPredictions,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for Autofill address import.
BASE_FEATURE(kAutofillDisableAddressImport, base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for Autofill filling.
BASE_FEATURE(kAutofillDisableFilling, base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_disallow_more_hyphen_like_labels)
// When enabled, the list of characters a label cannot exclusively consist of
// includes more hyphen-like characters: em-dash, minus sign and fullwidth
// hyphen-minus.
// TODO(crbug.com/440039204): Remove when launched.
BASE_FEATURE(kAutofillDisallowMoreHyphenLikeLabels,
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_disallow_more_hyphen_like_labels)

// Controls an ablation study in which autofill for addresses and payment data
// can be suppressed.
BASE_FEATURE(kAutofillEnableAblationStudy, base::FEATURE_DISABLED_BY_DEFAULT);
// The following parameters are only effective if the study is enabled.
// If "enabled_for_addresses" is true this means that the ablation study is
// enabled for addresses meaning that autofill may be disabled on some forms.
BASE_FEATURE_PARAM(bool,
                   kAutofillAblationStudyEnabledForAddressesParam,
                   &kAutofillEnableAblationStudy,
                   "enabled_for_addresses",
                   false);
BASE_FEATURE_PARAM(bool,
                   kAutofillAblationStudyEnabledForPaymentsParam,
                   &kAutofillEnableAblationStudy,
                   "enabled_for_payments",
                   false);
// The ratio of ablation_weight_per_mille / 1000 determines the chance of
// autofill being disabled on a given combination of site * time_window * client
// session. E.g. an ablation_weight_per_mille = 10 means that there is a 1%
// ablation chance.
BASE_FEATURE_PARAM(int,
                   kAutofillAblationStudyAblationWeightPerMilleParam,
                   &kAutofillEnableAblationStudy,
                   "ablation_weight_per_mille",
                   0);
// If not 0, the kAutofillAblationStudyAblationWeightPerMilleListXParam
// specify the ablation chances for sites that are on the respective list X.
// These parameters are different from
// kAutofillAblationStudyAblationWeightPerMilleParam which applies to all
// domains.
BASE_FEATURE_PARAM(int,
                   kAutofillAblationStudyAblationWeightPerMilleList1Param,
                   &kAutofillEnableAblationStudy,
                   "ablation_weight_per_mille_param1",
                   0);
BASE_FEATURE_PARAM(int,
                   kAutofillAblationStudyAblationWeightPerMilleList2Param,
                   &kAutofillEnableAblationStudy,
                   "ablation_weight_per_mille_param2",
                   0);
BASE_FEATURE_PARAM(int,
                   kAutofillAblationStudyAblationWeightPerMilleList3Param,
                   &kAutofillEnableAblationStudy,
                   "ablation_weight_per_mille_param3",
                   0);
BASE_FEATURE_PARAM(int,
                   kAutofillAblationStudyAblationWeightPerMilleList4Param,
                   &kAutofillEnableAblationStudy,
                   "ablation_weight_per_mille_param4",
                   0);
BASE_FEATURE_PARAM(int,
                   kAutofillAblationStudyAblationWeightPerMilleList5Param,
                   &kAutofillEnableAblationStudy,
                   "ablation_weight_per_mille_param5",
                   0);
BASE_FEATURE_PARAM(int,
                   kAutofillAblationStudyAblationWeightPerMilleList6Param,
                   &kAutofillEnableAblationStudy,
                   "ablation_weight_per_mille_param6",
                   0);
// If true, the ablation study runs as an A/A study (no behavioral changes) but
// clients are assigned to the respective groups.
BASE_FEATURE_PARAM(bool,
                   kAutofillAblationStudyIsDryRun,
                   &kAutofillEnableAblationStudy,
                   "ablation_study_is_dry_run",
                   false);

// Enables a new implementation for address field parsing that is based on
// backtracking.
BASE_FEATURE(kAutofillEnableAddressFieldParserNG,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will support per domain and per data type enterprise
// policy.
BASE_FEATURE(kAutofillEnableAutofillSettingsEnterprisePolicy,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the deduplication process for Autofill profiles is run on a
// background thread to avoid blocking the UI thread.
// TODO(crbug.com/496889243): Remove when launched.
BASE_FEATURE(kAutofillEnableDeduplicationOnBackgroundThread,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a couple of improvements to credit card expiration date handling:
// - The autocomplete attribute values are rationalized with format strings
//   like MM/YY from placeholders and labels in mind.
// - more fill follow.
// TODO(crbug.com/40266396): Remove once launched.
BASE_FEATURE(kAutofillEnableExpirationDateImprovements,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control if Autofill supports German transliteration.
// TODO(crbug.com/328968064): Remove when/if launched.
BASE_FEATURE(kAutofillEnableGermanTransliteration,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the import of unchanged values for ADDRESS_HOME_COUNTRY and
// ADDRESS_HOME_STATE fields is enabled.
// TODO(crbug.com/40137859): Remove once launched.
BASE_FEATURE(kAutofillEnableImportOfUnchangedValuesForCountryAndState,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to save the first number in a form with multiple phone
// numbers instead of aborting the import.
// TODO(crbug.com/40742746) Remove once launched.
BASE_FEATURE(kAutofillEnableImportWhenMultiplePhoneNumbers,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will help users fill in non-affiliated loyalty cards
// on loyalty card only fields.
BASE_FEATURE(kAutofillEnableNonAffiliatedLoyaltyCardsFilling,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will use heuristics to identify OTP fields.
BASE_FEATURE(kAutofillEnableOneTimeCodeHeuristics,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Controls if Chrome Autofill UI surfaces ignore touch events if something is
// fully or partially obscuring the Chrome window.
BASE_FEATURE(kAutofillEnableSecurityTouchEventFilteringAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Kill switch: If enabled, `AutofillField` may not suppress suggestions on
// field that has autocomplete=unrecognized attribute.
BASE_FEATURE(kAutofillEnableSkippingUnrecognizedAttribute,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the missing merge modes will be re-enabled on nodes. To do that,
// MergeMode::kMergeChildrenAndReformatIfNeeded will be also added to all the
// nodes where required.
// TODO(crbug.com/447111009): Remove when launched.
BASE_FEATURE(kAutofillEnableStreetAddressMergeModes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables extended zip code validation.
// TODO(crbug.com/434140055): Clean up when launched.
BASE_FEATURE(kAutofillExtendZipCodeValidation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, only non-ad frames are extracted.
// Otherwise, non-ad frames as well as *visible* ad frames are extracted.
// "Extracted" means that FormFieldData::child_frames is populated, which is
// necessary for flattening these forms.
// The forms in those frames are extracted either way.
// TODO(crbug.com/40196220): Remove once launched.
BASE_FEATURE(kAutofillExtractOnlyNonAdFrames,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, removes address field values that contain words "select",
// "choose", or "optional" during profile import.
// TODO(crbug.com/485170688): Remove when launched.
BASE_FEATURE(kAutofillFilterPlaceholderValuesOnImport,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Prioritizes ADDRESS_HOME_STREET_ADDRESS over postal code in inferred labels.
// See crbug.com/540151895.
BASE_FEATURE(kAutofillFixLabelGenerationForStreetAddress,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the rewriter uses updated rewrite rules.
// TODO(crbug.com/445863287): Cleanup when launched.
BASE_FEATURE(kAutofillFixRewriterRules, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the rationalization engine will fix misclassifications where
// a field is detected as a COUNTRY when it should be a STATE or vice versa.
// TODO(crbug.com/444180493): Cleanup when launched.
BASE_FEATURE(kAutofillFixStateCountryMisclassification,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Greek regexes are used for parsing in branded builds.
BASE_FEATURE(kAutofillGreekRegexes, base::FEATURE_ENABLED_BY_DEFAULT);

// LINT.IfChange(autofill_ignore_checkable_elements)
// If enabled, checkboxes and radio buttons aren't extracted anymore.
// TODO(crbug.com/40283901): Remove once launched. Also remove
// - autofill::FormControlType::kInputCheckbox
// - autofill::FormControlType::kInputRadio
BASE_FEATURE(kAutofillIgnoreCheckableElements,
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_ignore_checkable_elements)

// If enabled, global rules are applied to rewrite empty string values like
// "null" to an empty string. These rules are applied for all types during
// address normalization.
BASE_FEATURE(kAutofillIntroduceGlobalEmptyValueRewriterRules,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/346507576): Remove once the experiment is over.
// When enabled, makes autocomplete label sensitive.
BASE_FEATURE(kAutofillLabelSensitiveAutocomplete,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Migration generation for the autocomplete label-sensitive feature.
// If the migration generation received from the Finch server is greater than
// the stored browser parameter, re-migrate AutocompleteTableLabelSensitive data
// from the old AutocompleteTable.
BASE_FEATURE_PARAM(int,
                   kAutofillLabelSensitiveAutocompleteMigrationGeneration,
                   &kAutofillLabelSensitiveAutocomplete,
                   "autocomplete_label_sensitive_migration_generation",
                   0);

// When enabled, all behaviours related to the on-device machine learning
// model for field type predictions will be guarded.
// TODO(crbug.com/40276177): Remove when launched.
BASE_FEATURE(kAutofillModelPredictions, base::FEATURE_DISABLED_BY_DEFAULT);

// When true, use the machine learning model as the active `HeuristicSource`,
// else use the source provided by `kAutofillParsingPatternActiveSource`.
// It is defined with `BASE_FEATURE_PARAM()` to enable caching as the parameter
// is accesses in several getters.
BASE_FEATURE_PARAM(bool,
                   kAutofillModelPredictionsAreActive,
                   &kAutofillModelPredictions,
                   "model_active",
                   false);

// When true, apply small form rules to ML predictions - if there are too few
// fields or too few distinct types, predictions are cleared. There are some
// special cases. See
// `FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields`.
BASE_FEATURE_PARAM(bool,
                   kAutofillModelPredictionsSmallFormRules,
                   &kAutofillModelPredictions,
                   "small_form_rules",
                   false);

// When Enabled Autofill server will stop applying small form rule and Chrome
// will take care of this logic.
BASE_FEATURE(kAutofillMoveSmallFormLogicToClient,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the new suggestion generation logic is used.
// TODO(crbug.com/409962888): Remove once launched.
BASE_FEATURE(kAutofillNewSuggestionGeneration,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, `IsNormalizedNameVariantOf()` uses a linear-time greedy
// algorithm instead of an exponential one that generates all name variants.
// TODO(crbug.com/479905438) Remove once launched.
BASE_FEATURE(kAutofillOptimizeIsNormalizedNameVariantOf,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables detection of language from Translate.
// TODO(crbug.com/40158074): Cleanup when launched.
BASE_FEATURE(kAutofillPageLanguageDetection, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, focusing on a credit card number field that was traditionally
// autofilled will yield all credit card suggestions.
// TODO(crbug.com/354175563): Remove when launched.
BASE_FEATURE(kAutofillPaymentsFieldSwapping, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Autofill may fill across origins.
// In payment forms, the cardholder name field is often on the merchant's origin
// while the credit card number and CVC are in iframes hosted by a payment
// service provider. By enabling the policy-controlled feature "autofill" in
// those iframes, the merchant's website enable Autofill to fill the credit card
// number and CVC fields from the cardholder name field, even though this
// autofill operation crosses origins.
// TODO(crbug.com/40178859): Enable this feature.
BASE_FEATURE(kAutofillPolicyControlledFeatureAutofill,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Autofill warns about manual text input in cross-origin
// frames.
// This feature lives in Autofill code because of its close relationship to
// `kAutofillCrossOriginAutofill`.
// TODO(crbug.com/40178859): Enable this feature.
BASE_FEATURE(kAutofillPolicyControlledFeatureManualText,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, Autofill popups perform additional check to
// detect if they are obscured by top-level HTML form popups (e.g color picker).
// If so, Autofill Popup won't be shown.
// TODO(crbug.com/417052041): Remove when launched.
BASE_FEATURE(kAutofillPopupCheckHtmlFormPopupOverlap,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If the feature is enabled, before triggering suggestion acceptance, the row
// view checks that a substantial portion of its content was visible for some
// minimum required period.
// TODO(crbug.com/337222641): During cleaning up, in the popup row view remove
// emitting of "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough".
BASE_FEATURE(kAutofillPopupDontAcceptNonVisibleEnoughSuggestion,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch. When enabled, fields populated by standard Autofill or Autofill
// AI products are not saved to the Autocomplete database at form submission.
// TODO(crbug.com/533411686): Remove in M154.
BASE_FEATURE(kAutofillPreventAutofillFromSavingToAutocomplete,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Replaces blink::WebFormElementObserver usage in FormTracker by updated logic
// for tracking the disappearance of forms as well as other submission
// triggering events.
// TODO(crbug.com/40281981): Remove when launched.
BASE_FEATURE(kAutofillReplaceFormElementObserver,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Restricts OTP fields detection and fetching to forms that are in a frame
// with the same TLD+1 as the main frame.
BASE_FEATURE(kAutofillRestrictOtpToSameTldPlusOne,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/435646513) - Clean-up after feature lands at 100% Stable.
// Enables the new experimental server-side signatures for evaluation purposes.
BASE_FEATURE(kAutofillServerExperimentalSignatures,
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/470949499) - Clean-up after feature lands at 100% Stable.
// Enables querying the server for predictions before the form has been parsed
// locally.
BASE_FEATURE(kAutofillServerQueryPredictionsEarly,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables uploading of more data to the Autofill server to use for computing
// signatures: go/autofill-signatures-more-data.
BASE_FEATURE(kAutofillServerUploadMoreData, base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch: If enabled, the focus check in AutofillPopupControllerImpl and
// AutofillKeyboardAccessoryControllerImpl is simplified.
// TODO(crbug.com/530190112): Clean up after September 1, 2026.
BASE_FEATURE(kAutofillSimplifyFocusCheck, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, upload votes for sms otp.
// TODO(crbug.com/453999673): Clean up when launched.
BASE_FEATURE(kAutofillSmsOtpCrowdsourcing, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, fetch sms otp from gmscore and upload votes for sms otp.
// TODO(crbug.com/453999673): Clean up when launched.
BASE_FEATURE(kAutofillSmsOtpCrowdsourcingFetchFromGmscore,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If this feature is enabled, the AddressFieldParser does NOT try to parse
// address lines once it has found a street name and house number or other
// combinations of fields that indicate that an address form uses structured
// addresses. This should be the default in all countries with fully supported
// structured addresses. However, if a country is not sufficiently modeled,
// autofill may still do the right thing if it recognizes "Street name, house
// number, address line 2" as a sequence.
// TODO(crbug.com/40266693) Remove once launched.
BASE_FEATURE(kAutofillStructuredFieldsDisableAddressLines,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables parsing of fields that combine postal code and city in France,
// e.g. a single field containing "75008 Paris".
// TODO(crbug.com/465119085): Clean up when launched.
BASE_FEATURE(kAutofillSupportCombinedZipAndCityFR,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables splitting two-part zip codes into two fields while filling and
// importing split zip codes from two adjacent fields.
// TODO(crbug.com/369503318): Clean up when launched.
BASE_FEATURE(kAutofillSupportSplitZipCode, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, standalone zip code fields are classified by local heuristics
// globally, instead of just a handful of countries.
BASE_FEATURE(kAutofillSupportStandaloneZipCodeGlobally,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Mitigates side-channel brute-force probing of autofill data by rate-limiting
// AskForValuesToFill() invocations per RenderFrame via a token bucket.
BASE_FEATURE(kAutofillThrottleBruteForceProbing,
             base::FEATURE_ENABLED_BY_DEFAULT);

// The burst budget of AskForValuesToFill() calls permitted per RenderFrame.
BASE_FEATURE_PARAM(int,
                   kAutofillThrottleBruteForceProbingMaxTokens,
                   &kAutofillThrottleBruteForceProbing,
                   15);

// The rate at which AskForValuesToFill() token budget replenishes.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAutofillThrottleBruteForceProbingReplenishRate,
                   &kAutofillThrottleBruteForceProbing,
                   base::Milliseconds(750));

// Enables tracking of user edits to <select> fields that were not autofilled.
BASE_FEATURE(kAutofillTrackSelectFieldEdits, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillUKMExperimentalFields, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kAutofillUKMExperimentalFieldsBucket0,
                   &kAutofillUKMExperimentalFields,
                   "autofill_experimental_regex_bucket0",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAutofillUKMExperimentalFieldsBucket1,
                   &kAutofillUKMExperimentalFields,
                   "autofill_experimental_regex_bucket1",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAutofillUKMExperimentalFieldsBucket2,
                   &kAutofillUKMExperimentalFields,
                   "autofill_experimental_regex_bucket2",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAutofillUKMExperimentalFieldsBucket3,
                   &kAutofillUKMExperimentalFields,
                   "autofill_experimental_regex_bucket3",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAutofillUKMExperimentalFieldsBucket4,
                   &kAutofillUKMExperimentalFields,
                   "autofill_experimental_regex_bucket4",
                   "");

// Enables using a custom address model for India, overriding the legacy one.
BASE_FEATURE(kAutofillUseINAddressModel, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the form field parser won't try to match other attributes if
// any of the negative patterns matched.
BASE_FEATURE(kAutofillUseNegativePatternForAllAttributes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch: If enabled, use the original suggestion payload. If disabled,
// use a default GUID for payloads without a GUID [iOS specific].
// TODO(crbug.com/525996248): Remove after M154 branchpoint if there are no
// issues.
BASE_FEATURE(kAutofillUseOriginalPayloadIos, base::FEATURE_ENABLED_BY_DEFAULT);

// Replaces the secondary signature with the structural signature for Uploads.
// For Queries still only the secondary (alternative) signature is used.
// TODO(crbug.com/431737839): Clean up when roll out finishes successfully.
BASE_FEATURE(kAutofillUseStructuralSignatureInsteadOfSecondary,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the field classification model uses runtime caching to not run
// models on the same inputs multiple times.
// TODO(crbug.com/371933424). Clean up when launched, if not used for Autofill
// experiments.
BASE_FEATURE(kFieldClassificationModelCaching,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kGlicActorAutofill, base::FEATURE_DISABLED_BY_DEFAULT);

// The amount of time to wait for a fill to happen if no credit card fetch is
// ongoing.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kGlicActorAutofillFillingTimeout,
                   &kGlicActorAutofill,
                   "glic-actor-autofill-filling-timeout",
                   base::Seconds(2));

// The maximum amount of time to wait for a fill to happen (including credit
// card fetches)
BASE_FEATURE_PARAM(base::TimeDelta,
                   kGlicActorAutofillMaximumTimeout,
                   &kGlicActorAutofill,
                   "glic-actor-autofill-maximum-timeout",
                   base::Minutes(1));

// When enabled, a HaTS survey is shown after the user visited "Contact info"
// settings page.
BASE_FEATURE(kManageContactInfoPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user visited "Identity docs"
// settings page.
BASE_FEATURE(kManageIdentityDocsPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user visited Password Manager
// management surface.
BASE_FEATURE(kManagePasswordsPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user visited "Payments"
// settings page.
BASE_FEATURE(kManagePaymentsPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user visited "Travel"
// settings page.
BASE_FEATURE(kManageTravelPerceptionSurvey, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a HaTS survey is shown after the user visited "Your saved info"
// settings page.
BASE_FEATURE(kManageYourSavedInfoPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the AuthenticatorSelectionDialogBridge will reset the native
// pointer when the dialog is dismissed.
BASE_FEATURE(kResetNativePointerInCreditCardAuthDialog,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, an entry point to AtMemory will be shown at the bottom of the
// Autocomplete dialogs.
BASE_FEATURE(kShowAutocompleteAtMemoryButton,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch: if enabled, suggestions are shown for fields with unrecognized
// autocomplete attribute if they are already autofilled.
BASE_FEATURE(kShowSugesstionsOnAlreadyAutofilledUnrecognized,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, "Manage information" menu item for enhanced autofill will
// redirect user either to "/travel" or "/identityDocs" pages instead of
// "/yourSavedInfo" always.
BASE_FEATURE(kSuggestionManageButtonSplitForEnhancedAutofill,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the address add/edit editor in the payments request would be
// removed and instead, the address editor from the settings will be used.
// TODO: crbug.com/399071964 - Remove when launched.
BASE_FEATURE(kUseSettingsAddressEditorInPaymentsRequest,
             base::FEATURE_DISABLED_BY_DEFAULT);

#undef WALLET_SUPPORTED_COUNTRIES
#undef DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION

}  // namespace autofill::features
