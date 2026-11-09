// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/top_chrome_webui_profile_impl.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_process_host.h"

namespace webui {

namespace {

std::unique_ptr<TopChromeWebUIProfileImpl>& GetTopChromeWebUIProfileInstance() {
  static base::NoDestructor<std::unique_ptr<TopChromeWebUIProfileImpl>> profile;
  return *profile;
}

}  // namespace

bool ShouldUseTopChromeWebUIProfile() {
  return base::FeatureList::IsEnabled(
             features::kTopChromeWebUISharedRenderer) &&
         !content::RenderProcessHost::run_renderer_in_process();
}

void CreateTopChromeWebUIProfile() {
  std::unique_ptr<TopChromeWebUIProfileImpl>& profile =
      GetTopChromeWebUIProfileInstance();
  CHECK(!profile);
  profile = std::make_unique<TopChromeWebUIProfileImpl>();
}

void DestroyTopChromeWebUIProfile() {
  GetTopChromeWebUIProfileInstance().reset();
}

Profile* GetTopChromeWebUIProfile() {
  return GetTopChromeWebUIProfileInstance().get();
}

Profile& GetProfileForTopChromeWebUIContents(Profile* user_profile) {
  CHECK(user_profile);
  Profile* profile = ShouldUseTopChromeWebUIProfile()
                         ? GetTopChromeWebUIProfile()
                         : user_profile;
  CHECK(profile);
  return *profile;
}

bool IsWebUIDataSourceShared(content::WebUI* web_ui) {
  return Profile::FromWebUI(web_ui)->IsTopChromeWebUIProfile();
}

ScopedTopChromeWebUIProfileForTesting::ScopedTopChromeWebUIProfileForTesting() {
  CreateTopChromeWebUIProfile();
}

ScopedTopChromeWebUIProfileForTesting::
    ~ScopedTopChromeWebUIProfileForTesting() {
  DestroyTopChromeWebUIProfile();
}

}  // namespace webui
