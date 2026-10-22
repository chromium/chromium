// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"

#include <memory>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webui {

class TopChromeWebUIUserProfileTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
};

// A WebContents that is not in TopChromeWebUIProfile uses its own profile.
TEST_F(TopChromeWebUIUserProfileTest, OwnProfileOutsideTopChromeWebUIProfile) {
  EXPECT_EQ(&profile_,
            GetUserProfile(web_contents_factory_.CreateWebContents(&profile_)));
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

}  // namespace webui
