// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace webui {
namespace {

// The host of a Top Chrome WebUI that has moved to TopChromeWebUIProfile.
#if BUILDFLAG(IS_ANDROID)
constexpr const char* kMovedWebUIHost = chrome::kChromeUITabStripNtbHost;
#else
constexpr const char* kMovedWebUIHost = chrome::kChromeUIWebUIToolbarHost;
#endif

GURL GetMovedWebUiUrl(std::string_view scheme) {
  return GURL(base::StrCat(
      {scheme, url::kStandardSchemeSeparator, kMovedWebUIHost, "/"}));
}

class TopChromeWebUIProfileTest : public testing::Test {
 protected:
  TopChromeWebUIProfileTest() {
    feature_list_.InitAndEnableFeature(features::kTopChromeWebUISharedRenderer);
  }

  void TearDown() override {
    content::RenderProcessHost::SetRunRendererInProcess(false);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile user_profile_;
};

// With --single-process, the only renderer process can't host the WebUIs of
// another BrowserContext, so the WebUIs stay in the user profile.
TEST_F(TopChromeWebUIProfileTest, UsesUserProfileInSingleProcess) {
  content::RenderProcessHost::SetRunRendererInProcess(true);
  EXPECT_FALSE(ShouldUseTopChromeWebUIProfile());
  EXPECT_EQ(&user_profile_,
            &GetProfileForTopChromeWebUIContents(
                &user_profile_, GetMovedWebUiUrl(content::kChromeUIScheme)));
}

// Only the WebUIs that have moved to TopChromeWebUIProfile are created there.
TEST_F(TopChromeWebUIProfileTest, UsesTopChromeWebUIProfileForMovedWebUIs) {
  ScopedTopChromeWebUIProfileForTesting scoped_profile;
  EXPECT_TRUE(ShouldUseTopChromeWebUIProfile());
  EXPECT_EQ(GetTopChromeWebUIProfile(),
            &GetProfileForTopChromeWebUIContents(
                &user_profile_, GetMovedWebUiUrl(content::kChromeUIScheme)));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(GetTopChromeWebUIProfile(),
            &GetProfileForTopChromeWebUIContents(
                &user_profile_, GURL(chrome::kChromeUIOmniboxPopupURL)));
  EXPECT_EQ(GetTopChromeWebUIProfile(),
            &GetProfileForTopChromeWebUIContents(
                &user_profile_, GURL(chrome::kChromeUITabSearchURL)));
#endif
  EXPECT_EQ(&user_profile_,
            &GetProfileForTopChromeWebUIContents(
                &user_profile_, GURL("chrome://not-moved.top-chrome/")));
  EXPECT_EQ(
      &user_profile_,
      &GetProfileForTopChromeWebUIContents(
          &user_profile_, GetMovedWebUiUrl(content::kChromeUIUntrustedScheme)));
  EXPECT_EQ(&user_profile_, &GetProfileForTopChromeWebUIContents(
                                &user_profile_, GURL(url::kAboutBlankURL)));
}

}  // namespace
}  // namespace webui
