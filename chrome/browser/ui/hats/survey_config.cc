// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "survey_config.h"
#include "base/feature_list.h"
#include "base/features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/common/chrome_features.h"
#include "components/performance_manager/public/features.h"         // nogncheck
#include "components/permissions/constants.h"                       // nogncheck
#include "components/permissions/features.h"                        // nogncheck
#include "components/permissions/permission_hats_trigger_helper.h"  // nogncheck
#include "components/safe_browsing/core/common/features.h"          // nogncheck
#include "components/safe_browsing/core/common/safebrowsing_constants.h"  // nogncheck
#endif  // #if !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
constexpr char kHatsSurveyTriggerAutofillAddress[] = "autofill-address";
constexpr char kHatsSurveyTriggerAutofillCard[] = "autofill-card";
constexpr char kHatsSurveyTriggerAutofillPassword[] = "autofill-password";
constexpr char kHatsSurveyTriggerM1AdPrivacyPage[] = "m1-ad-privacy-page";
constexpr char kHatsSurveyTriggerM1TopicsSubpage[] = "m1-topics-subpage";
constexpr char kHatsSurveyTriggerM1FledgeSubpage[] = "m1-fledge-subpage";
constexpr char kHatsSurveyTriggerM1AdMeasurementSubpage[] =
    "m1-ad-measurement-subpage";
constexpr char kHatsSurveyTriggerNtpModules[] = "ntp-modules";
constexpr char kHatsSurveyTriggerNtpPhotosModuleOptOut[] =
    "ntp-photos-module-opt-out";
constexpr char kHatsSurveyTriggerPerformanceControlsPerformance[] =
    "performance-general";
constexpr char kHatsSurveyTriggerPerformanceControlsBatteryPerformance[] =
    "performance-battery";
constexpr char kHatsSurveyTriggerPerformanceControlsHighEfficiencyOptOut[] =
    "performance-high-efficiency-opt-out";
constexpr char kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut[] =
    "performance-battery-saver-opt-out";
// The permission prompt trigger permits configuring multiple triggers
// simultaneously. Each trigger increments a counter at the end -->
// "permission-prompt0", "permission-prompt1", ...
constexpr char kHatsSurveyTriggerPermissionsPrompt[] = "permissions-prompt";
constexpr char kHatsSurveyTriggerPrivacyGuide[] = "privacy-guide";
constexpr char kHatsSurveyTriggerPrivacySandbox[] = "privacy-sandbox";
constexpr char kHatsSurveyTriggerRedWarning[] = "red-warning";
constexpr char kHatsSurveyTriggerSettings[] = "settings";
constexpr char kHatsSurveyTriggerSettingsPrivacy[] = "settings-privacy";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox3ConsentAccept[] =
    "ts-ps3-consent-accept";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox3ConsentDecline[] =
    "ts-ps3-consent-decline";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeDismiss[] =
    "ts-ps3-notice-dismiss";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeOk[] =
    "ts-ps3-notice-ok";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeSettings[] =
    "ts-ps3-notice-settings";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeLearnMore[] =
    "ts-ps3-notice-learn-more";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentAccept[] =
    "ts-ps4-consent-accept";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentDecline[] =
    "ts-ps4-consent-decline";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeOk[] =
    "ts-ps4-notice-ok";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeSettings[] =
    "ts-ps4-notice-settings";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySettings[] =
    "ts-privacy-settings";
constexpr char kHatsSurveyTriggerTrustSafetyTrustedSurface[] =
    "ts-trusted-surface";
constexpr char kHatsSurveyTriggerTrustSafetyTransactions[] = "ts-transactions";
constexpr char kHatsSurveyTriggerWhatsNew[] = "whats-new";
constexpr char kHatsSurveyTriggerTrustSafetyV2BrowsingData[] =
    "ts-v2-browsing-data";
constexpr char kHatsSurveyTriggerTrustSafetyV2ControlGroup[] =
    "ts-v2-control-group";
constexpr char kHatsSurveyTriggerTrustSafetyV2PasswordCheck[] =
    "ts-v2-password-check";
constexpr char kHatsSurveyTriggerTrustSafetyV2SafetyCheck[] =
    "ts-v2-safety-check";
constexpr char kHatsSurveyTriggerTrustSafetyV2TrustedSurface[] =
    "ts-v2-trusted-surface";
constexpr char kHatsSurveyTriggerTrustSafetyV2PrivacyGuide[] =
    "ts-v2-privacy-guide";
constexpr char kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentAccept[] =
    "ts-v2-ps4-consent-accept";
constexpr char kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentDecline[] =
    "ts-v2-ps4-consent-decline";
constexpr char kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeOk[] =
    "ts-v2-ps4-notice-ok";
constexpr char kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeSettings[] =
    "ts-v2-ps4-notice-settings";
constexpr char kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial[] =
    "ts-v2-safe-browsing-interstitial";
#endif  // #if !BUILDFLAG(IS_ANDROID)

constexpr char kHatsSurveyTriggerTesting[] = "testing";
constexpr char kHatsNextSurveyTriggerIDTesting[] =
    "HLpeYy5Av0ugnJ3q1cK0XzzA8UHv";

namespace {

constexpr char kHatsSurveyProbability[] = "probability";
constexpr char kHatsSurveyEnSiteID[] = "en_site_id";
constexpr double kHatsSurveyProbabilityDefault = 0;

// Survey configs must always be hardcoded here, so that they require review
// from HaTS owners. Do not move this method out of the anonymous namespace or
// change its signature to work around this.
std::vector<hats::SurveyConfig> GetAllSurveyConfigs() {
  std::vector<hats::SurveyConfig> survey_configs;

  // Always add the default survey.
  hats::SurveyConfig default_survey;
  default_survey.enabled = true;
  default_survey.probability = 1.0f;
  default_survey.trigger = kHatsSurveyTriggerTesting;
  default_survey.trigger_id = kHatsNextSurveyTriggerIDTesting;
  default_survey.product_specific_bits_data_fields = {"Test Field 1",
                                                      "Test Field 2"};
  default_survey.product_specific_string_data_fields = {"Test Field 3"};
  survey_configs.emplace_back(default_survey);

#if !BUILDFLAG(IS_ANDROID)
  // Dev tools surveys.
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesCOEP,
                              "devtools-issues-coep",
                              "1DbEs89FS0ugnJ3q1cK0Nx6T99yT");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesMixedContent,
                              "devtools-issues-mixed-content",
                              "BhCYpUmyf0ugnJ3q1cK0VtxCftzo");
  survey_configs.emplace_back(
      &features::
          kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite,
      "devtools-issues-cookies-samesite", "w9JqqpmEr0ugnJ3q1cK0NezVg4iK");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesHeavyAd,
                              "devtools-issues-heavy-ad",
                              "bAeiT5J4P0ugnJ3q1cK0Ra6jg7s8");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesCSP,
                              "devtools-issues-csp",
                              "c9fjDmwjb0ugnJ3q1cK0USeAJJ9C");

  // Settings surveys.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopSettings,
      kHatsSurveyTriggerSettings);
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopSettingsPrivacy,
      kHatsSurveyTriggerSettingsPrivacy,
      /*presupplied_trigger_id=*/absl::nullopt,
      std::vector<std::string>{"3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopPrivacyGuide,
      kHatsSurveyTriggerPrivacyGuide);
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopPrivacySandbox,
      kHatsSurveyTriggerPrivacySandbox,
      /*presupplied_trigger_id=*/absl::nullopt,
      std::vector<std::string>{"3P cookies blocked",
                               "Privacy Sandbox enabled"});

  const auto ad_privacy_product_specific_bits_data =
      std::vector<std::string>{"3P cookies blocked", "Topics enabled",
                               "Fledge enabled", "Ad Measurement enabled"};
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopM1AdPrivacyPage,
      kHatsSurveyTriggerM1AdPrivacyPage,
      /*presupplied_trigger_id=*/absl::nullopt,
      ad_privacy_product_specific_bits_data);
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopM1TopicsSubpage,
      kHatsSurveyTriggerM1TopicsSubpage,
      /*presupplied_trigger_id=*/absl::nullopt,
      ad_privacy_product_specific_bits_data);
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopM1FledgeSubpage,
      kHatsSurveyTriggerM1FledgeSubpage,
      /*presupplied_trigger_id=*/absl::nullopt,
      ad_privacy_product_specific_bits_data);
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopM1AdMeasurementSubpage,
      kHatsSurveyTriggerM1AdMeasurementSubpage,
      /*presupplied_trigger_id=*/absl::nullopt,
      ad_privacy_product_specific_bits_data);

  // NTP modules survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopNtpModules,
      kHatsSurveyTriggerNtpModules);
  // NTP Photos module opt-out survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForNtpPhotosOptOut,
      kHatsSurveyTriggerNtpPhotosModuleOptOut);

  // Trust & Safety Sentiment surveys.
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySettings,
      features::kTrustSafetySentimentSurveyPrivacySettingsTriggerId.Get(),
      std::vector<std::string>{"Non default setting", "Ran safety check"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyTrustedSurface,
      features::kTrustSafetySentimentSurveyTrustedSurfaceTriggerId.Get(),
      std::vector<std::string>{"Interacted with Page Info"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyTransactions,
      features::kTrustSafetySentimentSurveyTransactionsTriggerId.Get(),
      std::vector<std::string>{"Saved password"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox3ConsentAccept,
      features::kTrustSafetySentimentSurveyPrivacySandbox3ConsentAcceptTriggerId
          .Get(),
      std::vector<std::string>{"Stable channel", "3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox3ConsentDecline,
      features::
          kTrustSafetySentimentSurveyPrivacySandbox3ConsentDeclineTriggerId
              .Get(),
      std::vector<std::string>{"Stable channel", "3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeDismiss,
      features::kTrustSafetySentimentSurveyPrivacySandbox3NoticeDismissTriggerId
          .Get(),
      std::vector<std::string>{"Stable channel", "3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeOk,
      features::kTrustSafetySentimentSurveyPrivacySandbox3NoticeOkTriggerId
          .Get(),
      std::vector<std::string>{"Stable channel", "3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeSettings,
      features::
          kTrustSafetySentimentSurveyPrivacySandbox3NoticeSettingsTriggerId
              .Get(),
      std::vector<std::string>{"Stable channel", "3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeLearnMore,
      features::
          kTrustSafetySentimentSurveyPrivacySandbox3NoticeLearnMoreTriggerId
              .Get(),
      std::vector<std::string>{"Stable channel", "3P cookies blocked",
                               "Privacy Sandbox enabled"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentAccept,
      features::kTrustSafetySentimentSurveyPrivacySandbox4ConsentAcceptTriggerId
          .Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentDecline,
      features::
          kTrustSafetySentimentSurveyPrivacySandbox4ConsentDeclineTriggerId
              .Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeOk,
      features::kTrustSafetySentimentSurveyPrivacySandbox4NoticeOkTriggerId
          .Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeSettings,
      features::
          kTrustSafetySentimentSurveyPrivacySandbox4NoticeSettingsTriggerId
              .Get());

  // Trust & Safety Sentiment surveys - Version 2.
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2BrowsingData,
      features::kTrustSafetySentimentSurveyV2BrowsingDataTriggerId.Get(),
      std::vector<std::string>{"Deleted history", "Deleted downloads",
                               "Deleted autofill form data"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2ControlGroup,
      features::kTrustSafetySentimentSurveyV2ControlGroupTriggerId.Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PasswordCheck,
      features::kTrustSafetySentimentSurveyV2PasswordCheckTriggerId.Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2SafetyCheck,
      features::kTrustSafetySentimentSurveyV2SafetyCheckTriggerId.Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2TrustedSurface,
      features::kTrustSafetySentimentSurveyV2TrustedSurfaceTriggerId.Get(),
      std::vector<std::string>{"Interacted with Page Info"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PrivacyGuide,
      features::kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId.Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentAccept,
      features::
          kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentAcceptTriggerId
              .Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentDecline,
      features::
          kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentDeclineTriggerId
              .Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeOk,
      features::kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeOkTriggerId
          .Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeSettings,
      features::
          kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeSettingsTriggerId
              .Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial,
      features::kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialTriggerId
          .Get());

  // Autofill surveys.
  survey_configs.emplace_back(&features::kAutofillAddressSurvey,
                              kHatsSurveyTriggerAutofillAddress);
  survey_configs.emplace_back(&features::kAutofillCardSurvey,
                              kHatsSurveyTriggerAutofillCard);
  survey_configs.emplace_back(&features::kAutofillPasswordSurvey,
                              kHatsSurveyTriggerAutofillPassword);

  // What's New survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopWhatsNew,
      kHatsSurveyTriggerWhatsNew);

  // Permissions surveys.
  for (auto& trigger_id_pair : permissions::PermissionHatsTriggerHelper::
           GetPermissionPromptTriggerIdPairs(
               kHatsSurveyTriggerPermissionsPrompt)) {
    // trigger_id_pair has structure <trigger_name, trigger_id>. trigger_name is
    // a unique name used by the HaTS service integration, and trigger_id is an
    // ID that specifies a survey in the Listnr backend.
    survey_configs.emplace_back(
        &permissions::features::kPermissionsPromptSurvey, trigger_id_pair.first,
        trigger_id_pair.second,
        std::vector<std::string>{
            permissions::kPermissionsPromptSurveyHadGestureKey},
        std::vector<std::string>{
            permissions::kPermissionsPromptSurveyPromptDispositionKey,
            permissions::kPermissionsPromptSurveyPromptDispositionReasonKey,
            permissions::kPermissionsPromptSurveyActionKey,
            permissions::kPermissionsPromptSurveyRequestTypeKey,
            permissions::kPermissionsPromptSurveyReleaseChannelKey,
            permissions::kPermissionsPromptSurveyDisplayTimeKey,
            permissions::kPermissionPromptSurveyOneTimePromptsDecidedBucketKey,
            permissions::kPermissionPromptSurveyUrlKey});
  }

  // Performance Controls surveys.
  survey_configs.emplace_back(
      &performance_manager::features::kPerformanceControlsPerformanceSurvey,
      kHatsSurveyTriggerPerformanceControlsPerformance,
      /*presupplied_trigger_id=*/absl::nullopt,
      std::vector<std::string>{"high_efficiency_mode"},
      std::vector<std::string>{"battery_saver_mode"});
  survey_configs.emplace_back(
      &performance_manager::features::
          kPerformanceControlsBatteryPerformanceSurvey,
      kHatsSurveyTriggerPerformanceControlsBatteryPerformance,
      /*presupplied_trigger_id=*/absl::nullopt,
      std::vector<std::string>{"high_efficiency_mode"},
      std::vector<std::string>{"battery_saver_mode"});
  survey_configs.emplace_back(
      &performance_manager::features::
          kPerformanceControlsHighEfficiencyOptOutSurvey,
      kHatsSurveyTriggerPerformanceControlsHighEfficiencyOptOut);
  survey_configs.emplace_back(
      &performance_manager::features::
          kPerformanceControlsBatterySaverOptOutSurvey,
      kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut);

  // Red Warning surveys.
  survey_configs.emplace_back(
      &safe_browsing::kRedWarningSurvey, kHatsSurveyTriggerRedWarning,
      safe_browsing::kRedWarningSurveyTriggerId.Get(),
      std::vector<std::string>{},
      std::vector<std::string>{
          safe_browsing::kFlaggedUrl, safe_browsing::kMainFrameUrl,
          safe_browsing::kReferrerUrl, safe_browsing::kUserActivityWithUrls});

#endif  // #if !BUILDFLAG(IS_ANDROID)

  return survey_configs;
}

}  // namespace

namespace hats {

SurveyConfig::SurveyConfig() = default;
SurveyConfig::SurveyConfig(const SurveyConfig&) = default;
SurveyConfig::~SurveyConfig() = default;

SurveyConfig::SurveyConfig(
    const base::Feature* feature,
    const std::string& trigger,
    const absl::optional<std::string>& presupplied_trigger_id,
    const std::vector<std::string>& product_specific_bits_data_fields,
    const std::vector<std::string>& product_specific_string_data_fields)
    : trigger(trigger),
      product_specific_bits_data_fields(product_specific_bits_data_fields),
      product_specific_string_data_fields(product_specific_string_data_fields) {
  enabled = base::FeatureList::IsEnabled(*feature);
  if (!enabled) {
    return;
  }

  probability = base::FeatureParam<double>(feature, kHatsSurveyProbability,
                                           kHatsSurveyProbabilityDefault)
                    .Get();

  // The trigger_id may be provided through the associated feature parameter or
  // may have been included in the source code. The latter is required to enable
  // multiple surveys with a single finch group, as a limitation with finch
  // prevents duplicate param names even for different features within a group.
  // The feature parameter name is "en_site_id" for legacy reasons, as this
  // was the HaTS v1 equivalent of a trigger ID in HaTS Next.
  trigger_id = presupplied_trigger_id ? *presupplied_trigger_id
                                      : base::FeatureParam<std::string>(
                                            feature, kHatsSurveyEnSiteID, "")
                                            .Get();

  user_prompted =
      base::FeatureParam<bool>(feature, "user_prompted", false).Get();
}

void GetActiveSurveyConfigs(SurveyConfigs& survey_configs_by_triggers_) {
  auto surveys = GetAllSurveyConfigs();

  // Filter down to active surveys configs and store them in a map for faster
  // access. Triggers within the browser may attempt to show surveys regardless
  // of whether the feature is enabled, so checking whether a particular survey
  // is enabled should be fast.
  for (const SurveyConfig& survey : surveys) {
    if (!survey.enabled || survey.trigger_id.empty()) {
      continue;
    }

    survey_configs_by_triggers_.emplace(survey.trigger, survey);
  }
}

}  // namespace hats
