// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace {

// Requests that the HaTS service for |profile|, if available, attempt to
// launch the survey associated with |trigger| on |web_contents|. Where the
// survey for |trigger| is configured to accept product specific data related to
// the user's 3P cookie & privacy sandbox setting.
void LaunchHatsSurveyWithProductSpecificData(Profile* profile,
                                             content::WebContents* web_contents,
                                             const std::string& trigger) {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile, /* create_if_necessary = */ true);

  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service)
    return;

  const bool third_party_cookies_blocked =
      static_cast<content_settings::CookieControlsMode>(
          profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode)) ==
      content_settings::CookieControlsMode::kBlockThirdParty;
  const bool privacy_sandbox_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kPrivacySandboxApisEnabled);
  hats_service->LaunchDelayedSurveyForWebContents(
      trigger, web_contents, 20000,
      {{"3P cookies blocked", third_party_cookies_blocked},
       {"Privacy Sandbox enabled", privacy_sandbox_enabled}},
      /*product_specific_string_data=*/{},
      /*require_same_origin=*/true);
}

}  // namespace

namespace settings {

HatsHandler::HatsHandler() = default;

HatsHandler::~HatsHandler() = default;

void HatsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "trustSafetyInteractionOccurred",
      base::BindRepeating(&HatsHandler::HandleTrustSafetyInteractionOccurred,
                          base::Unretained(this)));
}

void HatsHandler::HandleTrustSafetyInteractionOccurred(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetList().size());
  auto interaction =
      static_cast<TrustSafetyInteraction>(args->GetList()[0].GetInt());

  // Both the HaTS service, and the T&S sentiment service (which is another
  // wrapper on the HaTS service), may decide to launch surveys based on this
  // user interaction. The HaTS service is responsible for ensuring that users
  // are not over-surveyed, and that other potential issues such as simultaneous
  // surveys are avoided.
  RequestHatsSurvey(interaction);
  InformSentimentService(interaction);
}

void HatsHandler::RequestHatsSurvey(TrustSafetyInteraction interaction) {
  if (interaction == TrustSafetyInteraction::OPENED_PRIVACY_SANDBOX) {
    LaunchHatsSurveyWithProductSpecificData(Profile::FromWebUI(web_ui()),
                                            web_ui()->GetWebContents(),
                                            kHatsSurveyTriggerPrivacySandbox);
  } else if (interaction == TrustSafetyInteraction::RAN_SAFETY_CHECK ||
             interaction == TrustSafetyInteraction::USED_PRIVACY_CARD) {
    // If the privacy settings survey is explicitly targeting users who have not
    // viewed the Privacy Sandbox page, and this user has viewed the page, do
    // not attempt to show the privacy settings survey.
    if (features::kHappinessTrackingSurveysForDesktopSettingsPrivacyNoSandbox
            .Get() &&
        Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
            prefs::kPrivacySandboxPageViewed)) {
      return;
    }
    LaunchHatsSurveyWithProductSpecificData(Profile::FromWebUI(web_ui()),
                                            web_ui()->GetWebContents(),
                                            kHatsSurveyTriggerSettingsPrivacy);
  }
}

void HatsHandler::InformSentimentService(TrustSafetyInteraction interaction) {
  auto* sentiment_service = TrustSafetySentimentServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  if (!sentiment_service)
    return;

  if (interaction == TrustSafetyInteraction::USED_PRIVACY_CARD) {
    sentiment_service->InteractedWithPrivacySettings(
        web_ui()->GetWebContents());
  } else if (interaction == TrustSafetyInteraction::RAN_SAFETY_CHECK) {
    sentiment_service->RanSafetyCheck();
  } else if (interaction == TrustSafetyInteraction::OPENED_PASSWORD_MANAGER) {
    sentiment_service->OpenedPasswordManager(web_ui()->GetWebContents());
  }
}

}  // namespace settings
