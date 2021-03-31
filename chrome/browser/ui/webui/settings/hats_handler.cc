// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace settings {

HatsHandler::HatsHandler() = default;

HatsHandler::~HatsHandler() = default;

void HatsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "tryShowHatsSurvey",
      base::BindRepeating(&HatsHandler::HandleTryShowHatsSurvey,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "tryShowPrivacySandboxSurvey",
      base::BindRepeating(&HatsHandler::HandleTryShowPrivacySandboxHatsSurvey,
                          base::Unretained(this)));
}

void HatsHandler::HandleTryShowHatsSurvey(const base::ListValue* args) {
  // If the privacy settings survey is explicitly targeting users who have not
  // viewed the Privacy Sandbox page, and this user has viewed the page, do
  // not attempt to show the privacy settings survey.
  if (features::kHappinessTrackingSurveysForDesktopSettingsPrivacyNoSandbox
          .Get() &&
      Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
          prefs::kPrivacySandboxPageViewed)) {
    return;
  }

  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()), /* create_if_necessary = */ true);
  if (hats_service) {
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerSettingsPrivacy, web_ui()->GetWebContents(), 20000);
  }
}

void HatsHandler::HandleTryShowPrivacySandboxHatsSurvey(
    const base::ListValue* args) {
  auto* profile = Profile::FromWebUI(web_ui());
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile, /* create_if_necessary = */ true);

  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service)
    return;

  bool third_party_cookies_blocked =
      static_cast<content_settings::CookieControlsMode>(
          profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode)) ==
      content_settings::CookieControlsMode::kBlockThirdParty;
  bool privacy_sandbox_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kPrivacySandboxApisEnabled);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerPrivacySandbox, web_ui()->GetWebContents(), 20000,
      {{"3P cookies blocked", third_party_cookies_blocked},
       {"Privacy Sandbox enabled", privacy_sandbox_enabled}});
}

}  // namespace settings
