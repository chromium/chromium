// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"

using safe_browsing::SafeBrowsingState;

namespace {

// Generate the Product Specific bits data which accompanies privacy settings
// survey responses from |profile|.
SurveyBitsData GetPrivacySettingsProductSpecificBitsData(Profile* profile) {
  const bool third_party_cookies_blocked =
      static_cast<content_settings::CookieControlsMode>(
          profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode)) ==
      content_settings::CookieControlsMode::kBlockThirdParty;

  return {{"3P cookies blocked", third_party_cookies_blocked}};
}

// Rounds down the time on page to the nearest power of 2 in seconds, with a
// max of 16 mins. This is to bucketize the time on page to be sent with the
// HaTS survey.
int64_t BucketizeTimeOnPage(double time_on_page_ms) {
  constexpr int64_t kMaxTimeOnPageMinutes = 16;
  constexpr int64_t kMaxTimeBucketSeconds = kMaxTimeOnPageMinutes * 60;

  int64_t time_on_page_s = time_on_page_ms / 1000;
  if (time_on_page_s >= kMaxTimeBucketSeconds) {
    return kMaxTimeBucketSeconds;
  }
  return ukm::GetExponentialBucketMinForUserTiming(time_on_page_s);
}

}  // namespace

namespace settings {

HatsHandler::HatsHandler() = default;

HatsHandler::~HatsHandler() = default;

void HatsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "trustSafetyInteractionOccurred",
      base::BindRepeating(&HatsHandler::HandleTrustSafetyInteractionOccurred,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityPageHatsRequest",
      base::BindRepeating(&HatsHandler::HandleSecurityPageHatsRequest,
                          base::Unretained(this)));
}

/**
 * There are 4 arguments in the input list.
 * First arg is a set of SecurityPageV2Interactions.
 * Second arg indicates the SafeBrowsingState when the settings page was
 * opened.
 * Third arg indicates the total amount of time the user spent on the
 * security page.
 * Fourth arg indicates the SecuritySettingsBundleSetting when the settings page
 * was opened.
 */
void HatsHandler::HandleSecurityPageHatsRequest(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(4U, args.size());

  Profile* profile = Profile::FromWebUI(web_ui());

  // Enterprise users consideration.
  // If the admin disabled the survey, the survey will not be requested.
  if (!safe_browsing::IsSafeBrowsingSurveysEnabled(*profile->GetPrefs())) {
    return;
  }

  // Request HaTS survey.
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile, /* create_if_necessary = */ true);

  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service) {
    return;
  }

  // Do not send the survey if the user didn't stay on the page long enough.
  if (args[2].GetDouble() <
      features::kHappinessTrackingSurveysForSecurityPageTime.Get()
          .InMilliseconds()) {
    return;
  }
  // Generate the Product Specific bits data from |profile| and |args|.
  SurveyStringData product_specific_string_data =
      GetSecurityPageProductSpecificStringData(profile, args);

  hats_service->LaunchSurvey(
      kHatsSurveyTriggerSettingsSecurity,
      /*success_callback*/ base::DoNothing(),
      /*failure_callback*/ base::DoNothing(),
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/product_specific_string_data);
}

/**
 * Generate the Product Specific string data from |profile| and |args|.
 * - First arg in the list is a set of SecurityPageV2Interactions.
 * - Second arg in the list indicates the SafeBrowsingState.
 * - Third arg in the list indicates the amount of time user spent on the
 * security page in focus.
 * - Fourth arg in the list indicates the SecuritySettingsBundleSetting.
 */
SurveyStringData HatsHandler::GetSecurityPageProductSpecificStringData(
    Profile* profile,
    const base::Value::List& args) {
  const base::Value::List& interactions = args[0].GetList();
  auto safe_browsing_state = static_cast<SafeBrowsingState>(args[1].GetInt());

  auto security_settings_bundle_setting =
      static_cast<SecuritySettingsBundleSetting>(args[3].GetInt());

  std::string security_page_interactions = "";
  std::set<SecurityPageV2Interaction> interaction_set;
  // cast the int values to SecurityPageV2Interactions.
  for (const auto& interaction_value : interactions) {
    interaction_set.insert(
        static_cast<SecurityPageV2Interaction>(interaction_value.GetInt()));
  }

  // Generate the string representation of the interactions.
  std::vector<std::string> interaction_strings;
  for (const auto& interaction : interaction_set) {
    switch (interaction) {
      case SecurityPageV2Interaction::ENHANCED_BUNDLE_RADIO_BUTTON_CLICK: {
        interaction_strings.push_back("enhanced_bundle_radio_button_clicked");
        break;
      }
      case SecurityPageV2Interaction::STANDARD_BUNDLE_RADIO_BUTTON_CLICK: {
        interaction_strings.push_back("standard_bundle_radio_button_clicked");
        break;
      }
      case SecurityPageV2Interaction::SAFE_BROWSING_ROW_EXPANDED: {
        interaction_strings.push_back("safe_browsing_row_expanded");
        break;
      }
      case SecurityPageV2Interaction::
          STANDARD_SAFE_BROWSING_RADIO_BUTTON_CLICK: {
        interaction_strings.push_back(
            "standard_safe_browsing_radio_button_clicked");
        break;
      }
      case SecurityPageV2Interaction::
          ENHANCED_SAFE_BROWSING_RADIO_BUTTON_CLICK: {
        interaction_strings.push_back(
            "enhanced_safe_browsing_radio_button_clicked");
        break;
      }
    }
  }
  if (interaction_strings.empty()) {
    interaction_strings.push_back("no_interaction");
  }
  security_page_interactions = base::JoinString(interaction_strings, ", ");

  std::string safe_browsing_state_before = "";
  switch (safe_browsing_state) {
    case SafeBrowsingState::ENHANCED_PROTECTION: {
      safe_browsing_state_before = "enhanced_protection";
      break;
    }
    case SafeBrowsingState::STANDARD_PROTECTION: {
      safe_browsing_state_before = "standard_protection";
      break;
    }
    case SafeBrowsingState::NO_SAFE_BROWSING: {
      safe_browsing_state_before = "no_protection";
      break;
    }
  }

  std::string security_settings_bundle_setting_before = "";
  switch (security_settings_bundle_setting) {
    case SecuritySettingsBundleSetting::ENHANCED: {
      security_settings_bundle_setting_before = "enhanced_protection";
      break;
    }
    case SecuritySettingsBundleSetting::STANDARD: {
      security_settings_bundle_setting_before = "standard_protection";
      break;
    }
  }

  std::string safe_browsing_state_current = "";
  bool safe_browsing_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool safe_browsing_enhanced_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnhanced);
  if (safe_browsing_enhanced_enabled) {
    safe_browsing_state_current = "enhanced_protection";
  } else if (safe_browsing_enabled) {
    safe_browsing_state_current = "standard_protection";
  } else {
    safe_browsing_state_current = "no_protection";
  }

  std::string security_settings_bundle_setting_current = "";
  int security_settings_bundle_pref =
      profile->GetPrefs()->GetInteger(prefs::kSecuritySettingsBundle);
  auto current_bundle_setting =
      static_cast<SecuritySettingsBundleSetting>(security_settings_bundle_pref);

  switch (current_bundle_setting) {
    case SecuritySettingsBundleSetting::ENHANCED: {
      security_settings_bundle_setting_current = "enhanced_protection";
      break;
    }
    case SecuritySettingsBundleSetting::STANDARD: {
      security_settings_bundle_setting_current = "standard_protection";
      break;
    }
  }

  std::string client_channel =
      std::string(version_info::GetChannelString(chrome::GetChannel()));

  return {
      {"Security page user actions", security_page_interactions},
      {"Safe browsing setting when security page opened",
       safe_browsing_state_before},
      {"Security settings bundle setting when security page opened",
       security_settings_bundle_setting_before},
      {"Safe browsing setting when security page closed",
       safe_browsing_state_current},
      {"Security settings bundle setting when security page closed",
       security_settings_bundle_setting_current},
      {"Client channel", client_channel},
      {"Time on page (bucketed seconds)",
       base::NumberToString(BucketizeTimeOnPage(args[2].GetDouble()))},
  };
}

void HatsHandler::HandleTrustSafetyInteractionOccurred(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  auto interaction = static_cast<TrustSafetyInteraction>(args[0].GetInt());

  // Both the HaTS service, and the T&S sentiment service (which is another
  // wrapper on the HaTS service), may decide to launch surveys based on this
  // user interaction. The HaTS service is responsible for ensuring that users
  // are not over-surveyed, and that other potential issues such as simultaneous
  // surveys are avoided.
  RequestHatsSurvey(interaction);
  InformSentimentService(interaction);
}

void HatsHandler::RequestHatsSurvey(TrustSafetyInteraction interaction) {
  Profile* profile = Profile::FromWebUI(web_ui());
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile, /* create_if_necessary = */ true);

  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service) {
    return;
  }

  std::string trigger = "";
  int timeout_ms = 0;
  SurveyBitsData product_specific_bits_data = {};
  auto navigation_behavior = HatsService::NavigationBehavior::ALLOW_ANY;

  switch (interaction) {
    case TrustSafetyInteraction::RAN_SAFETY_CHECK:
      [[fallthrough]];
    case TrustSafetyInteraction::USED_PRIVACY_CARD: {
      // The control group for the Privacy guide HaTS experiment will need to
      // see either safety check or the privacy page to be eligible and have
      // never seen privacy guide.
      if (features::kHappinessTrackingSurveysForDesktopSettingsPrivacyNoGuide
              .Get() &&
          Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
              prefs::kPrivacyGuideViewed)) {
        return;
      }
      trigger = kHatsSurveyTriggerSettingsPrivacy;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopSettingsPrivacyTime.Get()
              .InMilliseconds();
      product_specific_bits_data =
          GetPrivacySettingsProductSpecificBitsData(profile);
      navigation_behavior =
          HatsService::NavigationBehavior::REQUIRE_SAME_ORIGIN;
      break;
    }
    case TrustSafetyInteraction::COMPLETED_PRIVACY_GUIDE: {
      trigger = kHatsSurveyTriggerPrivacyGuide;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopPrivacyGuideTime.Get()
              .InMilliseconds();
      navigation_behavior =
          HatsService::NavigationBehavior::REQUIRE_SAME_ORIGIN;
      break;
    }
    case TrustSafetyInteraction::OPENED_PASSWORD_MANAGER:
      [[fallthrough]];
    case TrustSafetyInteraction::RAN_PASSWORD_CHECK: {
      // Only relevant for the sentiment service
      return;
    }
  }
  // If we haven't returned, a trigger must have been set in the switch above.
  CHECK_NE(trigger, "");

  hats_service->LaunchDelayedSurveyForWebContents(
      trigger, web_ui()->GetWebContents(), timeout_ms,
      product_specific_bits_data,
      /*product_specific_string_data=*/{}, navigation_behavior);
}

void HatsHandler::InformSentimentService(TrustSafetyInteraction interaction) {
  auto* sentiment_service = TrustSafetySentimentServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  if (!sentiment_service) {
    return;
  }

  if (interaction == TrustSafetyInteraction::USED_PRIVACY_CARD) {
    sentiment_service->InteractedWithPrivacySettings(
        web_ui()->GetWebContents());
  } else if (interaction == TrustSafetyInteraction::RAN_SAFETY_CHECK) {
    sentiment_service->RanSafetyCheck();
  } else if (interaction == TrustSafetyInteraction::OPENED_PASSWORD_MANAGER) {
    sentiment_service->OpenedPasswordManager(web_ui()->GetWebContents());
  } else if (interaction == TrustSafetyInteraction::RAN_PASSWORD_CHECK) {
    sentiment_service->RanPasswordCheck();
  } else if (interaction == TrustSafetyInteraction::COMPLETED_PRIVACY_GUIDE) {
    sentiment_service->FinishedPrivacyGuide();
  }
}

}  // namespace settings
