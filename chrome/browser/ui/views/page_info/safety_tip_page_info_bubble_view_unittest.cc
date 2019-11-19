// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/security_state/core/security_state.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"

namespace {

// Helper class that wraps a TestingProfile and a TestWebContents for a test
// harness. Inspired by RenderViewHostTestHarness, but doesn't use inheritance
// so the helper can be composed with other helpers in the test harness.
class ScopedWebContentsTestHelper {
 public:
  ScopedWebContentsTestHelper() {
    web_contents_ = factory_.CreateWebContents(&profile_);
  }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory factory_;
  content::WebContents* web_contents_;  // Weak. Owned by factory_.

  DISALLOW_COPY_AND_ASSIGN(ScopedWebContentsTestHelper);
};

class SafetyTipPageInfoBubbleViewTest : public testing::Test {
 public:
  SafetyTipPageInfoBubbleViewTest() {}

  // testing::Test:
  void SetUp() override {
    views_helper_.test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());
    views::Widget::InitParams parent_params;
    parent_params.context = views_helper_.GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(std::move(parent_params));

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    TabSpecificContentSettings::CreateForWebContents(web_contents);

    bubble_ = CreateSafetyTipBubbleForTesting(
        parent_window_->GetNativeView(), web_contents,
        security_state::SafetyTipStatus::kBadReputation,
        GURL("https://www.fakegoogle.tld"), GURL("https://www.google.tld"),
        base::DoNothing());
  }

  void TearDown() override { parent_window_->CloseNow(); }

 protected:
  ScopedWebContentsTestHelper web_contents_helper_;
  views::ScopedViewsTestHelper views_helper_;

  PageInfoBubbleViewBase* bubble_ = nullptr;
  views::Widget* parent_window_ = nullptr;  // Weak. Owned by the NativeWidget.

 private:
  DISALLOW_COPY_AND_ASSIGN(SafetyTipPageInfoBubbleViewTest);
};

}  // namespace

TEST_F(SafetyTipPageInfoBubbleViewTest, OpenAndClose) {
  // This test just opens and closes the bubble.
}
