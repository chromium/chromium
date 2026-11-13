// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"

namespace webui {

class TopChromeWebUIUserProfileTest : public testing::Test {
 protected:
  // Creates the WebUI toolbar's WebContents in TopChromeWebUIProfile, the only
  // one that may compute WebPreferences before its user profile is set.
  std::unique_ptr<content::WebContents> CreateToolbarWebContents() {
    Profile* top_chrome_webui_profile = GetTopChromeWebUIProfile();
    return content::WebContentsTester::CreateTestWebContents(
        top_chrome_webui_profile,
        content::SiteInstance::CreateForURL(
            top_chrome_webui_profile, GURL(chrome::kChromeUIWebUIToolbarURL)));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
};

// A WebContents that is not in TopChromeWebUIProfile uses its own profile.
TEST_F(TopChromeWebUIUserProfileTest, OwnProfileOutsideTopChromeWebUIProfile) {
  content::WebContents* web_contents =
      web_contents_factory_.CreateWebContents(&profile_);
  EXPECT_EQ(&profile_, GetUserProfile(web_contents));
}

// A WebContents in TopChromeWebUIProfile uses the user profile set for it,
// which is null until it is set and after it is destroyed.
TEST_F(TopChromeWebUIUserProfileTest, UserProfileInTopChromeWebUIProfile) {
  ScopedTopChromeWebUIProfileForTesting scoped_top_chrome_webui_profile;
  auto user_profile = std::make_unique<TestingProfile>();
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          GetTopChromeWebUIProfile(), /*instance=*/nullptr);
  EXPECT_FALSE(GetUserProfile(web_contents.get()));

  SetUserProfile(web_contents.get(), user_profile.get());
  EXPECT_EQ(user_profile.get(), GetUserProfile(web_contents.get()));

  user_profile.reset();
  EXPECT_FALSE(GetUserProfile(web_contents.get()));
}

// The WebUI toolbar in TopChromeWebUIProfile without a user profile computes
// WebPreferences before and after navigation using content defaults; a late
// SetUserProfile() binds the user profile without recomputing WebPreferences
// prematurely, and OnWebPreferencesChanged() recomputes them from the bound
// profile's prefs.
TEST_F(TopChromeWebUIUserProfileTest,
       LateSetUserProfileRecomputesWebPreferences) {
  ScopedTopChromeWebUIProfileForTesting scoped_top_chrome_webui_profile;
  TestingProfile user_profile;
  user_profile.GetPrefs()->SetBoolean(prefs::kWebKitDomPasteEnabled, true);

  std::unique_ptr<content::WebContents> web_contents =
      CreateToolbarWebContents();
  EXPECT_FALSE(GetUserProfile(web_contents.get()));
  EXPECT_FALSE(web_contents->GetOrCreateWebPreferences().dom_paste_enabled);

  // Commit a navigation without a user profile so
  // OverrideWebPreferencesAfterNavigation() also runs without a user profile.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL(chrome::kChromeUIWebUIToolbarURL));
  EXPECT_FALSE(web_contents->GetOrCreateWebPreferences().dom_paste_enabled);

  SetUserProfile(web_contents.get(), &user_profile);
  EXPECT_EQ(&user_profile, GetUserProfile(web_contents.get()));
  EXPECT_FALSE(web_contents->GetOrCreateWebPreferences().dom_paste_enabled);

  web_contents->OnWebPreferencesChanged();
  EXPECT_TRUE(web_contents->GetOrCreateWebPreferences().dom_paste_enabled);
}

// The initial WebUI toolbar clears its generic font maps whether or not its
// user profile is set, and computes color-related state without one.
TEST_F(TopChromeWebUIUserProfileTest,
       LateSetUserProfileKeepsInitialWebUIFonts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInitialWebUI, features::kWebUIReloadButton}, {});
  ScopedTopChromeWebUIProfileForTesting scoped_top_chrome_webui_profile;
  TestingProfile user_profile;
  user_profile.GetPrefs()->SetInteger(prefs::kWebKitDefaultFontSize, 22);

  std::unique_ptr<content::WebContents> web_contents =
      CreateToolbarWebContents();
  // Runs WebPreferencesNeedUpdateForColorRelatedStateChanges() and
  // GetForcedColorsForWebContents() without a user profile.
  web_contents->SetColorProviderSource(nullptr);
  EXPECT_TRUE(web_contents->GetOrCreateWebPreferences()
                  .standard_font_family_map.empty());
  EXPECT_NE(22, web_contents->GetOrCreateWebPreferences().default_font_size);

  SetUserProfile(web_contents.get(), &user_profile);
  web_contents->OnWebPreferencesChanged();
  EXPECT_TRUE(web_contents->GetOrCreateWebPreferences()
                  .standard_font_family_map.empty());
  EXPECT_EQ(22, web_contents->GetOrCreateWebPreferences().default_font_size);
}
}  // namespace webui
