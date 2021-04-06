// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_helper.h"

#include "base/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

HatsHelper::~HatsHelper() = default;

HatsHelper::HatsHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

void HatsHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  // If the demo HaTS feature is enabled display a test survey on every page
  // load unless the "auto_prompt" parameter is explicitly set to false. The
  // demo feature also disables client-side HaTS rate limiting, thus setting
  // "auto_prompt" to false allows testing of non-demo surveys without
  // triggering a demo survey on every page load.
  const bool demo_enabled =
      base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo) &&
      base::FeatureParam<bool>(
          &features::kHappinessTrackingSurveysForDesktopDemo, "auto_prompt",
          true)
          .Get();

  if (!render_frame_host->GetParent() && demo_enabled) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        profile(), /*create_if_necessary=*/true);

    if (hats_service) {
        hats_service->LaunchSurvey(kHatsSurveyTriggerTesting, base::DoNothing(),
                                   base::DoNothing(),
                                   {{"Test Field 1", true},
                                    {"Test Field 2", false},
                                    {"Test Field 3", true}});
    }
  }
}

Profile* HatsHelper::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HatsHelper)
