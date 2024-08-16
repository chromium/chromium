// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/security_state/core/security_state.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace {

// Helper class that wraps a TestingProfile and a TestWebContents for a test
// harness. Inspired by RenderViewHostTestHarness, but doesn't use inheritance
// so the helper can be composed with other helpers in the test harness.
class ScopedWebContentsTestHelper {
 public:
  ScopedWebContentsTestHelper() {
    web_contents_ = factory_.CreateWebContents(&profile_);
  }

  ScopedWebContentsTestHelper(const ScopedWebContentsTestHelper&) = delete;
  ScopedWebContentsTestHelper& operator=(const ScopedWebContentsTestHelper&) =
      delete;

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
};

class SafetyTipPageInfoBubbleViewTest : public testing::Test {
 public:
  SafetyTipPageInfoBubbleViewTest() {}

  SafetyTipPageInfoBubbleViewTest(const SafetyTipPageInfoBubbleViewTest&) =
      delete;
  SafetyTipPageInfoBubbleViewTest& operator=(
      const SafetyTipPageInfoBubbleViewTest&) = delete;

  // testing::Test:
  void SetUp() override {
    views::Widget::InitParams parent_params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
    parent_params.context = views_helper_.GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(std::move(parent_params));

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents,
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));

    bubble_ = CreateSafetyTipBubbleForTesting(
        parent_window_->GetNativeView(), web_contents,
        security_state::SafetyTipStatus::kLookalike,
        GURL("https://www.google.tld"), base::DoNothing());
  }

  void TearDown() override { parent_window_->CloseNow(); }

 protected:
  ScopedWebContentsTestHelper web_contents_helper_;
  views::ScopedViewsTestHelper views_helper_{
      std::make_unique<ChromeTestViewsDelegate<>>()};

  raw_ptr<PageInfoBubbleViewBase, DanglingUntriaged> bubble_ = nullptr;
  raw_ptr<views::Widget, DanglingUntriaged> parent_window_ =
      nullptr;  // Weak. Owned by the NativeWidget.
};

}  // namespace

TEST_F(SafetyTipPageInfoBubbleViewTest, OpenAndClose) {
  // This test just opens and closes the bubble.
}
