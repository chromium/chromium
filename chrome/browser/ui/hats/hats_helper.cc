// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/search/search.h"
#include "content/public/browser/web_contents.h"

HatsHelper::~HatsHelper() = default;

HatsHelper::HatsHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(search::IsInstantExtendedAPIEnabled());
}

void HatsHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  const bool demo_enabled = base::FeatureList::IsEnabled(
      features::kHappinessTrackingSurveysForDesktopDemo);
  if (!render_frame_host->GetParent() &&
      (search::IsInstantNTP(web_contents()) || demo_enabled)) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        profile(), /*create_if_necessary=*/true);

    if (hats_service) {
      hats_service->LaunchSurvey(demo_enabled ? kHatsSurveyTriggerTesting
                                              : kHatsSurveyTriggerSatisfaction);
    }
  }
}

Profile* HatsHelper::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HatsHelper)
