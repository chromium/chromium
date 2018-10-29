// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/test/widget_test.h"
#include "url/gurl.h"

class BrowserNonClientFrameViewTest : public TestWithBrowserView {
 public:
  explicit BrowserNonClientFrameViewTest(Browser::Type type)
      : TestWithBrowserView(type, false), frame_view_(nullptr) {}

  // TestWithBrowserView override:
  void SetUp() override {
#if defined(OS_WIN)
    // Use opaque frame.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableDwmComposition);
#endif
    TestWithBrowserView::SetUp();
    views::Widget* widget = browser_view()->GetWidget();
    frame_view_ = static_cast<BrowserNonClientFrameView*>(
        widget->non_client_view()->frame_view());
  }

 protected:
  // Owned by the browser view.
  BrowserNonClientFrameView* frame_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewTest);
};

class BrowserNonClientFrameViewPopupTest
    : public BrowserNonClientFrameViewTest {
 public:
  BrowserNonClientFrameViewPopupTest()
      : BrowserNonClientFrameViewTest(Browser::TYPE_POPUP) {}
};

TEST_F(BrowserNonClientFrameViewPopupTest, HitTestPopupTopChrome) {
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(-1, 4, 1, 1)));
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(4, -1, 1, 1)));
  const int top_inset = frame_view_->GetTopInset(false);
  EXPECT_FALSE(frame_view_->HitTestRect(gfx::Rect(4, top_inset, 1, 1)));
  if (top_inset > 0)
    EXPECT_TRUE(frame_view_->HitTestRect(gfx::Rect(4, top_inset - 1, 1, 1)));
}

class BrowserNonClientFrameViewTabbedTest
    : public BrowserNonClientFrameViewTest {
 public:
  BrowserNonClientFrameViewTabbedTest()
      : BrowserNonClientFrameViewTest(Browser::TYPE_TABBED) {}
};

TEST_F(BrowserNonClientFrameViewTabbedTest, HitTestTabstrip) {
  gfx::Rect tabstrip_bounds =
      frame_view_->browser_view()->tabstrip()->GetLocalBounds();
  EXPECT_FALSE(tabstrip_bounds.IsEmpty());

  // Completely outside bounds.
  EXPECT_FALSE(frame_view_->HitTestRect(
      gfx::Rect(tabstrip_bounds.x() - 1, tabstrip_bounds.y() + 1, 1, 1)));
  EXPECT_FALSE(frame_view_->HitTestRect(
      gfx::Rect(tabstrip_bounds.x() + 1, tabstrip_bounds.y() - 1, 1, 1)));

  // Hits tab strip but not client area.
  EXPECT_TRUE(frame_view_->HitTestRect(
      gfx::Rect(tabstrip_bounds.x() + 1,
                tabstrip_bounds.bottom() -
                    GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) - 1,
                1, 1)));

  // Hits tab strip and client area.
  EXPECT_TRUE(frame_view_->HitTestRect(
      gfx::Rect(tabstrip_bounds.x() + 1,
                tabstrip_bounds.bottom() -
                    GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) - 1,
                100, 100)));
}

// Fixture for testing colors fetched from theme service.
class BrowserNonClientFrameViewThemeTest
    : public BrowserNonClientFrameViewTest {
 public:
  BrowserNonClientFrameViewThemeTest()
      : BrowserNonClientFrameViewTest(Browser::Type::TYPE_TABBED) {}

  // We override this method to provide our StubThemeService.
  TestingProfile::TestingFactories GetTestingFactories() override {
    auto testing_factories =
        BrowserNonClientFrameViewTest::GetTestingFactories();
    testing_factories.emplace_back(ThemeServiceFactory::GetInstance(),
                                   base::BindRepeating(&BuildStubThemeService));
    return testing_factories;
  }

 private:
  class StubThemeService : public ThemeService {
    SkColor GetDefaultColor(int id, bool incognito) const override {
      switch (id) {
        case ThemeProperties::COLOR_TAB_TEXT:
          return SK_ColorBLACK;
        case ThemeProperties::COLOR_TAB_TEXT_INACTIVE:
          return SK_ColorWHITE;
        default:
          return ThemeService::GetDefaultColor(id, incognito);
      }
    }
  };

  static std::unique_ptr<KeyedService> BuildStubThemeService(
      content::BrowserContext* context) {
    auto theme_service = std::make_unique<StubThemeService>();
    theme_service->Init(static_cast<Profile*>(context));
    return theme_service;
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewThemeTest);
};

// TODO(ellyjones): widget activation doesn't work on Mac.
// https://crbug.com/823543
#if defined(OS_MACOSX)
#define MAYBE_ActiveTabTextColor DISABLED_ActiveTabTextColor
#else
#define MAYBE_ActiveTabTextColor DISABLED_ActiveTabTextColor
#endif

TEST_F(BrowserNonClientFrameViewThemeTest, MAYBE_ActiveTabTextColor) {
  frame_view_->frame()->Show();

  // Get text color for active tab in active window.
  frame_view_->frame()->Activate();
  views::test::WidgetActivationWaiter active_waiter(frame_view_->frame(), true);
  active_waiter.Wait();
  SkColor active_color =
      frame_view_->GetTabForegroundColor(TabState::TAB_ACTIVE);

  // Get text color for active tab in inactive window.
  frame_view_->frame()->Deactivate();
  views::test::WidgetActivationWaiter inactive_waiter(frame_view_->frame(),
                                                      false);
  inactive_waiter.Wait();
  SkColor inactive_color =
      frame_view_->GetTabForegroundColor(TabState::TAB_ACTIVE);

  // These colors should be different since our stub ThemeService returns black
  // for active windows and white for inactive windows. We don't check the color
  // against the ones StubThemeService provides because
  // BrowserNonClientFrameView may adjust the colors for minimum contrast
  // against the tab background. As long as the colors are different, we're
  // good.
  EXPECT_NE(active_color, inactive_color);
}
