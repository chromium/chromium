// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_profile_service.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

InitialWebUIProfileService::InitialWebUIProfileService(Profile* profile)
    : profile_(profile) {
  if (features::kWebUIReloadButtonProfilePrewarming.Get()) {
    PrewarmWebUI();
  }
}

InitialWebUIProfileService::~InitialWebUIProfileService() = default;

void InitialWebUIProfileService::Shutdown() {
  toolbar_web_contents_.reset();
}

std::unique_ptr<content::WebContents>
InitialWebUIProfileService::TakeToolbarContents() {
  return std::move(toolbar_web_contents_);
}

void InitialWebUIProfileService::PrewarmWebUI() {
  if ((features::IsWebUIToolbarEnabled() &&
       features::kWebUIReloadButtonPrewarmWebUI.Get()) ||
      base::FeatureList::IsEnabled(
          features::kWebUIToolbarProcessOverheadExperiment)) {
    const bool pre_navigate =
        features::kWebUIReloadButtonPrewarmWebUIPreNavigate.Get() ||
        base::FeatureList::IsEnabled(
            features::kWebUIToolbarProcessOverheadExperiment);
    toolbar_web_contents_ =
        waap::PrewarmHelper::PrewarmWebUIContents(profile_, pre_navigate);
  }
}
