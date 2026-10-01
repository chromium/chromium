// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_SURVEY_CONFIG_H_
#define CHROME_BROWSER_UI_HATS_SURVEY_CONFIG_H_

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/compose/buildflags.h"
#include "pdf/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
// Trigger identifiers currently used; duplicates not allowed.
inline constexpr char kHatsSurveyTriggerAutofillAddress[] = "autofill-address";
inline constexpr char kHatsSurveyTriggerAutofillAiSavePrompt[] =
    "autofill-ai-walletable-entity-save-prompt";
inline constexpr char
    kHatsSurveyTriggerAutofillAddressUserDeclinedSuggestion[] =
        "autofill-address-users-perception";
inline constexpr char kHatsSurveyTriggerAutofillAddressUserDeclinedSave[] =
    "autofill-address-user-declined-save";
inline constexpr char kHatsSurveyTriggerAutofillPasswordUserPerception[] =
    "autofill-password-users-perception";
inline constexpr char kHatsSurveyTriggerAutofillCard[] = "autofill-card";
inline constexpr char kHatsSurveyTriggerAutofillPassword[] =
    "autofill-password";
inline constexpr char kHatsSurveyTriggerAutoPipAllowed[] = "autopip-allowed";
inline constexpr char kHatsSurveyTriggerAutoPipBlocked[] = "autopip-blocked";
inline constexpr char kHatsSurveyTriggerAutoPipPermissionPromptIgnored[] =
    "autopip-permission-prompt-ignored";
inline constexpr char kHatsSurveyTriggerContextualCueingDismissed[] =
    "contextual-cueing-dismissed";
inline constexpr char kHatsSurveyTriggerManageYourSavedInfoPerception[] =
    "autofill-manage-your-saved-info-perception";
inline constexpr char kHatsSurveyTriggerManagePasswordsPerception[] =
    "autofill-manage-passwords-perception";
inline constexpr char kHatsSurveyTriggerManagePaymentsPerception[] =
    "autofill-manage-payments-perception";
inline constexpr char kHatsSurveyTriggerManageContactInfoPerception[] =
    "autofill-manage-contact-info-perception";
inline constexpr char kHatsSurveyTriggerManageIdentityDocsPerception[] =
    "autofill-manage-identity-docs-perception";
inline constexpr char kHatsSurveyTriggerManageTravelPerception[] =
    "autofill-manage-travel-perception";
inline constexpr char kHatsSurveyTriggerDownloadWarningBubbleBypass[] =
    "download-warning-bubble-bypass";
inline constexpr char kHatsSurveyTriggerDownloadWarningBubbleHeed[] =
    "download-warning-bubble-heed";
inline constexpr char kHatsSurveyTriggerDownloadWarningBubbleIgnore[] =
    "download-warning-bubble-ignore";
inline constexpr char kHatsSurveyTriggerDownloadWarningPageBypass[] =
    "download-warning-page-bypass";
inline constexpr char kHatsSurveyTriggerDownloadWarningPageHeed[] =
    "download-warning-page-heed";
inline constexpr char kHatsSurveyTriggerDownloadWarningPageIgnore[] =
    "download-warning-page-ignore";
inline constexpr char kHatsSurveyTriggerHistoryEmbeddings[] =
    "history-embeddings";
inline constexpr char kHatsSurveyTriggerHistoryPageExperiment[] =
    "history-page-experiment";
inline constexpr char kHatsSurveyTriggerHistoryPageControl[] =
    "history-page-control";
inline constexpr char kHatsSurveyTriggerIdentityAddressBubbleSignin[] =
    "identity-address-bubble-signin";
inline constexpr char kHatsSurveyTriggerIdentityDiceWebSigninAccepted[] =
    "identity-dice-web-signin-accepted";
inline constexpr char kHatsSurveyTriggerIdentityDiceWebSigninDeclined[] =
    "identity-dice-web-signin-declined";
inline constexpr char kHatsSurveyTriggerIdentityFirstRunSignin[] =
    "identity-first-run-signin";
inline constexpr char kHatsSurveyTriggerIdentityFirstRunCompleted[] =
    "identity-first-run-completed";
inline constexpr char kHatsSurveyTriggerIdentityPasswordBubbleSignin[] =
    "identity-password-bubble-signin";
inline constexpr char kHatsSurveyTriggerIdentityProfileMenuDismissed[] =
    "identity-profile-menu-dismissed";
inline constexpr char kHatsSurveyTriggerIdentityProfileMenuSignin[] =
    "identity-profile-menu-signin";
inline constexpr char
    kHatsSurveyTriggerIdentityProfilePickerAddProfileSignin[] =
        "identity-profile-picker-add-profile-signin";
inline constexpr char kHatsSurveyTriggerIdentityRefreshedFirstRunCompleted[] =
    "identity-refreshed-first-run-completed";
inline constexpr char kHatsSurveyTriggerFirstRunDesktopRevampCompleted[] =
    "identity-revamp-first-run-completed";
inline constexpr char
    kHatsSurveyTriggerFirstRunDesktopRevampNoFeatureShowcaseCompleted[] =
        "identity-revamp-no-feature-showcase-first-run-completed";
inline constexpr char kHatsSurveyTriggerPreFirstRunDesktopRefreshCompleted[] =
    "identity-pre-first-run-desktop-refresh-completed";
inline constexpr char
    kHatsSurveyTriggerPreFirstRunDesktopRefreshNoFeatureShowcaseCompleted[] =
        "identity-pre-first-run-desktop-refresh-no-feature-showcase-completed";
inline constexpr char
    kHatsSurveyTriggerIdentitySigninInterceptProfileSeparation[] =
        "identity-signin-intercept-profile-separation";
inline constexpr char kHatsSurveyTriggerIdentitySigninPromoBubbleDismissed[] =
    "identity-signin-promo-bubble-dismissed";
inline constexpr char kHatsSurveyTriggerIdentitySwitchProfileFromProfileMenu[] =
    "identity-switch-profile-profile-menu";
inline constexpr char
    kHatsSurveyTriggerIdentitySwitchProfileFromProfilePicker[] =
        "identity-switch-profile-profile-picker";
inline constexpr char kHatsSurveyTriggerLensOverlayResults[] =
    "lens-overlay-results";
inline constexpr char kHatsSurveyTriggerNtpModules[] = "ntp-modules";
inline constexpr char kHatsSurveyTriggerNextPanel[] = "next-panel";
inline constexpr char kHatsSurveyTriggerNtpPhotosModuleOptOut[] =
    "ntp-photos-module-opt-out";
inline constexpr char kHatsSurveyTriggerPerformanceControlsPPM[] =
    "performance-ppm";
inline constexpr char kHatsSurveyTriggerPrivacyGuide[] = "privacy-guide";
inline constexpr char kHatsSurveyTriggerRedWarning[] = "red-warning";
inline constexpr char kHatsSurveyTriggerSettings[] = "settings";
inline constexpr char kHatsSurveyTriggerSEHijacking[] =
    "search-engine-hijacking";
inline constexpr char kHatsSurveyTriggerSettingsPrivacy[] = "settings-privacy";
inline constexpr char kHatsSurveyTriggerSettingsSecurity[] =
    "settings-security";
inline constexpr char kHatsSurveyTriggerSettingsSecurityV2[] =
    "settings-security-v2";
inline constexpr char kHatsSurveyTriggerTrustSafetyPrivacySettings[] =
    "ts-privacy-settings";
inline constexpr char kHatsSurveyTriggerTrustSafetyTrustedSurface[] =
    "ts-trusted-surface";
inline constexpr char kHatsSurveyTriggerTrustSafetyTransactions[] =
    "ts-transactions";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2BrowsingData[] =
    "ts-v2-browsing-data";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2ControlGroup[] =
    "ts-v2-control-group";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2DownloadWarningUI[] =
    "ts-v2-download-warning-ui";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2PasswordCheck[] =
    "ts-v2-password-check";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI[] =
    "ts-v2-password-protection-ui";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2SafetyCheck[] =
    "ts-v2-safety-check";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2SafetyHubNotification[] =
    "ts-v2-safety-hub-notification";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2SafetyHubInteraction[] =
    "ts-v2-safety-hub-interaction";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2TrustedSurface[] =
    "ts-v2-trusted-surface";
inline constexpr char kHatsSurveyTriggerTrustSafetyV2PrivacyGuide[] =
    "ts-v2-privacy-guide";
inline constexpr char
    kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial[] =
        "ts-v2-safe-browsing-interstitial";
inline constexpr char kHatsSurveyTriggerWallpaperSearch[] = "wallpaper-search";
#if BUILDFLAG(ENABLE_COMPOSE)
inline constexpr char kHatsSurveyTriggerComposeAcceptance[] =
    "compose-acceptance";
inline constexpr char kHatsSurveyTriggerComposeClose[] = "compose-close";
inline constexpr char kHatsSurveyTriggerComposeNudgeClose[] =
    "compose-nudge-close";
#endif  // #if BUILDFLAG(ENABLE_COMPOSE)
inline constexpr char kHatsSurveyTriggerWhatsNew[] = "whats-new";
inline constexpr char kHatsSurveyTriggerReadingModeExit[] = "reading-mode-exit";
#else   // BUILDFLAG(IS_ANDROID)
inline constexpr char kHatsSurveyTriggerAndroidStartupSurvey[] =
    "startup_survey";
inline constexpr char kHatsSurveyTriggerRedWarningAndroid[] =
    "red-warning-android";
inline constexpr char kHatsSurveyTriggerSigninFirstRun[] = "signin-first-run";
inline constexpr char kHatsSurveyTriggerSigninWeb[] = "signin-web";
inline constexpr char kHatsSurveyTriggerSigninNtpSigninButton[] =
    "signin-ntp-signin-button";
inline constexpr char kHatsSurveyTriggerSigninNtpAccountAvatarTap[] =
    "signin-ntp-account-avatar-tap";
inline constexpr char kHatsSurveyTriggerSigninNtpPromo[] = "signin-ntp-promo";
inline constexpr char kHatsSurveyTriggerSigninBookmarkPromo[] =
    "signin-bookmark-promo";
inline constexpr char kHatsSurveyTriggerSuspiciousSiteWarning[] =
    "suspicious-site-warning";
#endif  // #if !BUILDFLAG(IS_ANDROID)

inline constexpr char
    kHatsSurveyTriggerAutofillPersonalizationAndTrustAddressFilled[] =
        "autofill-personalization-and-trust-address-filled";
// This survey is a requirement for the Ambient Autofill feature to allow users
// to provide feedback and should not be removed without legal approval. Its
// PSDs must allow to see if Ambient Autofill (pContext record type) was used.
inline constexpr char
    kHatsSurveyTriggerAutofillPersonalizationAndTrustAutofillAiFilled[] =
        "autofill-personalization-and-trust-autofill-ai-filled";
inline constexpr char
    kHatsSurveyTriggerAutofillPersonalizationAndTrustCreditCardFilled[] =
        "autofill-personalization-and-trust-credit-card-filled";
inline constexpr char
    kHatsSurveyTriggerAutofillPersonalizationAndTrustOneTimePasswordFilled[] =
        "autofill-personalization-and-trust-one-time-password-filled";
inline constexpr char
    kHatsSurveyTriggerAutofillPersonalizationAndTrustAtMemoryFilled[] =
        "autofill-personalization-and-trust-at-memory-filled";
inline constexpr char kHatsSurveyTriggerPermissionsPrompt[] =
    "permissions-prompt";
inline constexpr char kHatsSurveyTriggerOnFocusZpsSuggestionsHappiness[] =
    "omnibox-on-focus-happiness";
inline constexpr char kHatsSurveyTriggerOnFocusZpsSuggestionsUtility[] =
    "omnibox-on-focus-utility";

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
inline constexpr char kHatsSurveyConsumerTriggerPdfSaveToDrive[] =
    "save-to-drive-consumer";
inline constexpr char kHatsSurveyEnterpriseTriggerPdfSaveToDrive[] =
    "save-to-drive-enterprise";
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

inline constexpr char kHatsSurveyTriggerTesting[] = "testing";
// The Trigger ID for a test HaTS Next survey which is available for testing
// and demo purposes when the migration feature flag is enabled.
inline constexpr char kHatsNextSurveyTriggerIDTesting[] =
    "HLpeYy5Av0ugnJ3q1cK0XzzA8UHv";

namespace hats {
struct SurveyConfig {
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.hats
  enum RequestedBrowserType {
    // A standard survey, shown only in regular mode.
    kRegular = 0,
    // An Incognito survey, shown only in incognito.
    kIncognito = 1,
  };

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.hats
  // Enum to control the minimum profile age check before showing a survey.
  // The profile age is determined by the creation time of the profile
  // directory, and is NOT related to the age of the user.
  enum class ProfileAgeRequirement {
    // Default requirement: Only show the survey if the current profile was
    // created at least 30 days ago. This helps filter out transient or
    // very new profiles, aiming for feedback from more established users.
    kOneMonthOrOlder,

    // Allow the survey to be shown regardless of how recently the profile
    // was created. Use this option with caution, as it can introduce bias.
    // For example, on shared computers where profiles are frequently reset,
    // this could lead to overrepresentation of these environments in survey
    // results, and bypass "at most 1 survey per user" throttling if not
    // combined with other constraints.
    kAnyAge
  };

  // Constructs a SurveyConfig by inspecting |feature|. This includes checking
  // if the feature is enabled, as well as inspecting the feature parameters
  // for the survey probability, and if |presupplied_trigger_id| is not
  // provided, the trigger ID. To pass any product specific data for the
  // survey, configure fields here, matches are CHECK enforced.
  // SurveyConfig that enable |log_responses_to_uma| and/or
  // |log_responses_to_ukm| will need to have surveys reviewed by privacy to
  // ensure they are appropriate to log to UMA and/or UKM. This is enforced
  // through the OWNERS mechanism.
  SurveyConfig(
      const base::Feature* feature,
      const std::string& trigger,
      const std::optional<std::string>& presupplied_trigger_id = std::nullopt,
      const std::vector<std::string>& product_specific_bits_data_fields = {},
      const std::vector<std::string>& product_specific_string_data_fields = {},
      bool log_responses_to_uma = false,
      bool log_responses_to_ukm = false,
      ProfileAgeRequirement profile_age_requirement =
          ProfileAgeRequirement::kOneMonthOrOlder,
      RequestedBrowserType requested_browser_type =
          RequestedBrowserType::kRegular);

  SurveyConfig();
  SurveyConfig(const SurveyConfig&);
  ~SurveyConfig();

  // Whether the survey is currently enabled and can be shown.
  bool enabled = false;

  // Probability [0,1] of how likely a chosen user will see the survey.
  double probability = 0.0f;

  // The trigger for this survey within the browser.
  std::string trigger;

  // Trigger ID for the survey.
  std::string trigger_id;

  // Histogram name for the survey.
  std::optional<std::string> hats_histogram_name;

  // ID that ties Chrome survey configuration to UKM. This ID can be configured
  // in Finch to any 64-bit unsigned integer. This ID should only be used to
  // distinguish surveys in UKM and no other purpose.
  std::optional<uint64_t> hats_survey_ukm_id;

  // The survey will prompt every time because the user has explicitly decided
  // to take the survey e.g. clicking a link.
  bool user_prompted = false;

  // Product Specific Bit Data fields which are sent with the survey
  // response.
  std::vector<std::string> product_specific_bits_data_fields;

  // Product Specific String Data fields which are sent with the survey
  // response.
  std::vector<std::string> product_specific_string_data_fields;

  // Specifies the profile age requirement.
  ProfileAgeRequirement profile_age_requirement =
      ProfileAgeRequirement::kOneMonthOrOlder;

  // Requested browser type decides where the survey can be shown.
  RequestedBrowserType requested_browser_type = RequestedBrowserType::kRegular;

  // The feature associated with the HaTS survey. It is used to check if the
  // survey is in the dogfood stage, meaning that it's launched only for a
  // subset of users controlled by some Google group.
  raw_ptr<const base::Feature> survey_feature;

  // Returns |hats_histogram_name| if |hats_histogram_name| is an non-empty
  // std::string that is prefixed with Feedback.HappinessTrackingSurvey.
  // Otherwise, returns std::nullopt.
  static std::optional<std::string> ValidateHatsHistogramName(
      const std::optional<std::string>& hats_histogram_name);

  // Returns |hats_survey_ukm_id| if |hats_survey_ukm_id| is an non-empty
  // optional greater than 0. Otherwise, returns std::nullopt.
  static std::optional<uint64_t> ValidateHatsSurveyUkmId(
      const std::optional<uint64_t> hats_survey_ukm_id);

};

using SurveyConfigs = base::flat_map<std::string, SurveyConfig>;
// Get the list of active ongoing surveys and store into
// |survey_configs_by_triggers_|.
void GetActiveSurveyConfigs(SurveyConfigs& survey_configs_by_triggers_);

}  // namespace hats

#endif  // CHROME_BROWSER_UI_HATS_SURVEY_CONFIG_H_
