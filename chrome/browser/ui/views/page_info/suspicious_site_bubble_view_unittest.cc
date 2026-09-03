// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/suspicious_site_bubble_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/views_test_utils.h"

namespace {

class ScopedWebContentsTestHelper {
 public:
  ScopedWebContentsTestHelper() {
    web_contents_ = factory_.CreateWebContents(&profile_);
  }

  ScopedWebContentsTestHelper(const ScopedWebContentsTestHelper&) = delete;
  ScopedWebContentsTestHelper& operator=(const ScopedWebContentsTestHelper&) =
      delete;

  TestingProfile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace

class SuspiciousSiteBubbleViewTest : public testing::Test {
 public:
  SuspiciousSiteBubbleViewTest() = default;

  SuspiciousSiteBubbleViewTest(const SuspiciousSiteBubbleViewTest&) = delete;
  SuspiciousSiteBubbleViewTest& operator=(const SuspiciousSiteBubbleViewTest&) =
      delete;

  void SetUp() override {
    views::Widget::InitParams parent_params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_params.context = views_helper_.GetContext();
    parent_window_ = std::make_unique<views::Widget>();
    parent_window_->Init(std::move(parent_params));

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents,
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));

    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents, GURL("https://suspicious.example.com"));

    bubble_ = static_cast<SuspiciousSiteBubbleView*>(
        CreateSuspiciousSiteBubbleForTesting(parent_window_->GetNativeView(),
                                             web_contents));
  }

  void TearDown() override {
    bubble_ = nullptr;
    if (parent_window_) {
      parent_window_->CloseNow();
    }
    parent_window_.reset();
  }

 protected:
  ScopedWebContentsTestHelper web_contents_helper_;
  views::ScopedViewsTestHelper views_helper_{
      std::make_unique<ChromeTestViewsDelegate<>>()};

  raw_ptr<SuspiciousSiteBubbleView> bubble_ = nullptr;
  std::unique_ptr<views::Widget> parent_window_;
};

TEST_F(SuspiciousSiteBubbleViewTest, BubbleTypeAndElements) {
  EXPECT_EQ(PageInfoBubbleViewBase::GetShownBubbleType(),
            PageInfoBubbleViewBase::BUBBLE_SUSPICIOUS_SITE);
  EXPECT_NE(bubble_->back_to_safety_button_for_testing(), nullptr);
  EXPECT_NE(bubble_->mark_as_safe_button_for_testing(), nullptr);
  EXPECT_NE(bubble_->description_label_for_testing(), nullptr);
  EXPECT_TRUE(
      web_contents_helper_.web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(SuspiciousSiteBubbleViewTest, WebContentsInputIgnoredWhileBubbleOpen) {
  EXPECT_TRUE(
      web_contents_helper_.web_contents()->ShouldIgnoreInputEventsForTesting());
  views::Widget* widget = bubble_->GetWidget();
  bubble_ = nullptr;
  widget->CloseNow();
  EXPECT_FALSE(
      web_contents_helper_.web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(SuspiciousSiteBubbleViewTest, MarkAsSafeAllowsHost) {
  HostContentSettingsMap* hcsm = HostContentSettingsMapFactory::GetForProfile(
      web_contents_helper_.profile());
  safe_browsing::SuspiciousSiteWarningAllowlist allowlist(hcsm);
  EXPECT_FALSE(allowlist.IsSiteAllowedForHost("suspicious.example.com"));

  views::test::ButtonTestApi(bubble_->mark_as_safe_button_for_testing())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_TRUE(allowlist.IsSiteAllowedForHost("suspicious.example.com"));
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
  EXPECT_FALSE(
      web_contents_helper_.web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(SuspiciousSiteBubbleViewTest, BackToSafetyClick) {
  views::test::ButtonTestApi(bubble_->back_to_safety_button_for_testing())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
  EXPECT_EQ(web_contents_helper_.web_contents()->GetVisibleURL(),
            GURL(chrome::kChromeUINewTabURL));
  EXPECT_FALSE(
      web_contents_helper_.web_contents()->ShouldIgnoreInputEventsForTesting());
}
