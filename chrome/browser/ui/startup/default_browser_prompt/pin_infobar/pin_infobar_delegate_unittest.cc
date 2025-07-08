// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

class PinInfoBarDelegateTest : public testing::Test {
 protected:
  PinInfoBarDelegateTest() {
    feature_list_.InitAndEnableFeature(features::kOfferPinToTaskbarInfoBar);
  }

  void SetUp() override {
    infobar_manager_ =
        std::make_unique<infobars::ContentInfoBarManager>(web_contents_.get());
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobar_manager_.get();
  }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  ChromeLayoutProvider layout_provider_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_{
      content::WebContentsTester::CreateTestWebContents(
          content::WebContents::CreateParams(&profile_))};
};

// Executes the code to ensure that creating the infobar doesn't crash.
TEST_F(PinInfoBarDelegateTest, Create) {
  ASSERT_TRUE(infobar_manager()->infobars().empty());
  EXPECT_TRUE(PinInfoBarDelegate::Create(infobar_manager()));
  EXPECT_EQ(1u, infobar_manager()->infobars().size());
}

}  // namespace default_browser
