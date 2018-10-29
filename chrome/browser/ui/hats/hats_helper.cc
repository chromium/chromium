// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_helper.h"

#include <memory>

#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/search/search.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

HatsHelper::HatsHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents), web_contents_(web_contents) {
  DCHECK(search::IsInstantExtendedAPIEnabled());
}

HatsHelper::~HatsHelper() {}

void HatsHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& /* validated_url */) {
  if (!render_frame_host->GetParent() && search::IsInstantNTP(web_contents_)) {
    HatsService* hats_service =
        HatsServiceFactory::GetForProfile(profile(), true);

    if (hats_service) {
      hats_service->LaunchSatisfactionSurvey();
    }
  }
}

Profile* HatsHelper::profile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}
