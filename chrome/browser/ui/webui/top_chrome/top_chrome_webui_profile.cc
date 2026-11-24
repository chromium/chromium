// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/top_chrome_webui_profile_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace webui {

namespace {

std::unique_ptr<TopChromeWebUIProfileImpl>& GetTopChromeWebUIProfileInstance() {
  static base::NoDestructor<std::unique_ptr<TopChromeWebUIProfileImpl>> profile;
  return *profile;
}

// Returns whether the Top Chrome WebUI at `url` has moved to this profile. The
// WebUIs here share renderer processes and WebUIDataSources across the windows
// of all profiles, so they must not depend on the profile of the WebUI. They
// get the user profile from webui::GetUserProfile() instead.
bool IsInTopChromeWebUIProfile(const GURL& url) {
  if (!url.SchemeIs(content::kChromeUIScheme)) {
    return false;
  }
  static constexpr auto kHosts = base::MakeFixedFlatSet<std::string_view>({
      chrome::kChromeUIOmniboxPopupHost,
      chrome::kChromeUITabSearchHost,
      chrome::kChromeUIWebUIToolbarHost,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      chrome::kChromeUIDefaultBrowserModalHost,
#endif
  });
  return kHosts.contains(url.host());
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

Profile& GetProfileForTopChromeWebUIContents(Profile* user_profile,
                                             const GURL& url) {
  CHECK(user_profile);
  CHECK(!user_profile->IsTopChromeWebUIProfile());
  if (!ShouldUseTopChromeWebUIProfile() || !IsInTopChromeWebUIProfile(url)) {
    return *user_profile;
  }
  Profile* profile = GetTopChromeWebUIProfile();
  CHECK(profile);
  return *profile;
}

ScopedTopChromeWebUIProfileForTesting::ScopedTopChromeWebUIProfileForTesting() {
  CreateTopChromeWebUIProfile();
}

ScopedTopChromeWebUIProfileForTesting::
    ~ScopedTopChromeWebUIProfileForTesting() {
  DestroyTopChromeWebUIProfile();
}

}  // namespace webui
