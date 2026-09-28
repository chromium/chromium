// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"

namespace {

class TestLocationBarViewDelegate : public LocationBarView::Delegate {
 public:
  explicit TestLocationBarViewDelegate(LocationBarModel* model)
      : model_(model) {}
  content::WebContents* GetWebContents() override { return nullptr; }
  LocationBarModel* GetLocationBarModel() override { return model_; }
  const LocationBarModel* GetLocationBarModel() const override {
    return model_;
  }
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override {
    return nullptr;
  }

 private:
  raw_ptr<LocationBarModel> model_;
};

// Reports a fixed element context for the browser it's attached to, so that
// elements shown in that context can be found through BrowserElements.
class TestBrowserElements : public BrowserElements {
 public:
  DECLARE_SAFE_CAST_TARGET()
  TestBrowserElements(BrowserWindowInterface& browser,
                      ui::ElementContext context)
      : BrowserElements(browser), context_(context) {}
  ui::ElementContext GetContext() override { return context_; }

 private:
  const ui::ElementContext context_;
};

DEFINE_SAFE_CAST_TARGET(TestBrowserElements)

constexpr ui::ElementContext kTestContext =
    ui::ElementContext::CreateFakeContextForTesting(1);

}  // namespace

class WebUIPermissionChipTest : public testing::Test {
 protected:
  // Only this fixture is a friend of WebUIPermissionChip, so tests read its
  // private timeout through here.
  static constexpr base::TimeDelta kAnchorFallbackTimeout =
      WebUIPermissionChip::kAnchorFallbackTimeout;

  // Just short of the timeout after which a pending `WaitForAnchor()` callback
  // runs anyway, leaving `GetAnchor()` to fall back.
  static constexpr base::TimeDelta kJustBeforeAnchorFallback =
      kAnchorFallbackTimeout - base::Milliseconds(1);

  void SetUp() override {
    location_bar_model_ = std::make_unique<TestLocationBarModel>();
    delegate_ = std::make_unique<TestLocationBarViewDelegate>(
        location_bar_model_.get());
    location_bar_ =
        std::make_unique<WebUILocationBar>(nullptr, delegate_.get());
    // Manually set the delegate to avoid Init() which needs a valid Browser.
    location_bar_->toolbar_delegate_ = &mock_toolbar_delegate_;

    ON_CALL(mock_toolbar_delegate_, GetView())
        .WillByDefault(testing::Return(&dummy_view_));
  }

  // Makes `location_bar_` return `browser` from GetBrowser(), without running
  // Init() which needs a valid Browser.
  void SetBrowser(BrowserWindowInterface* browser) {
    location_bar_->browser_ = browser;
  }

  content::BrowserTaskEnvironment browser_threads_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  views::View dummy_view_;
  testing::NiceMock<MockWebUIToolbarControlDelegate> mock_toolbar_delegate_;
  // Declared before `location_bar_` so that it outlives `location_bar_`.
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_;
  std::unique_ptr<TestLocationBarModel> location_bar_model_;
  std::unique_ptr<TestLocationBarViewDelegate> delegate_;
  std::unique_ptr<WebUILocationBar> location_bar_;
};

TEST_F(WebUIPermissionChipTest, DoesNotAnnounceAlertOnExpandEnded) {
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);
  const std::u16string message = u"Test Announcement";
  chip.SetMessage(message);

  // The chip must not announce itself when the expand animation ends. This
  // mirrors the Native Views `PermissionChipView`, where `ChipController`
  // decides whether to announce (it stays silent when the prompt bubble starts
  // open, since the bubble fires its own `ax::mojom::Event::kAlert`).
  // Announcing here would make screen readers speak the request twice.
  EXPECT_CALL(mock_toolbar_delegate_, AnnounceAlert(testing::_)).Times(0);

  // Simulate the expansion animation ended IPC from WebUI.
  chip.AnimateExpand(base::Milliseconds(350));
  chip.OnExpandAnimationEnded();
}

TEST_F(WebUIPermissionChipTest, AnnounceAlertAndText) {
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);
  const std::u16string message = u"Direct Announcement";

  EXPECT_CALL(mock_toolbar_delegate_, AnnounceAlert(message)).Times(2);

  chip.AnnounceAlert(message);
  chip.AnnounceText(message);
}

class TestPermissionChipObserver : public PermissionChipInterface::Observer {
 public:
  explicit TestPermissionChipObserver(WebUIPermissionChip* chip)
      : chip_(chip) {}
  void OnCollapseAnimationEnded() override { chip_->SetVisible(false); }

 private:
  raw_ptr<WebUIPermissionChip> chip_;
};

TEST_F(WebUIPermissionChipTest, CollapseAnimationEndedReentrancy) {
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);
  chip.SetVisible(true);
  TestPermissionChipObserver observer(&chip);
  chip.AddObserver(&observer);

  // Trigger expand then collapse to set the animation state flags correctly.
  chip.AnimateExpand(base::Milliseconds(100));
  chip.AnimateCollapse(base::Milliseconds(100));

  // This should not crash.
  chip.OnCollapseAnimationEnded();
  EXPECT_FALSE(chip.GetVisible());
}

TEST_F(WebUIPermissionChipTest, GetThemeForTesting) {
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);
  EXPECT_EQ(chip.GetThemeForTesting(), PermissionChipTheme::kNormalVisibility);

  chip.SetTheme(PermissionChipTheme::kInUseActivityIndicator);
  EXPECT_EQ(chip.GetThemeForTesting(),
            PermissionChipTheme::kInUseActivityIndicator);
}

TEST_F(WebUIPermissionChipTest, GetTextForTesting) {
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);
  EXPECT_TRUE(chip.GetTextForTesting().empty());

  const std::u16string message = u"Camera in use";
  chip.SetMessage(message);
  EXPECT_EQ(chip.GetTextForTesting(), message);
}

TEST_F(WebUIPermissionChipTest, GetIsRequestForTesting) {
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);
  EXPECT_TRUE(chip.GetIsRequestForTesting());

  chip.SetTheme(PermissionChipTheme::kLowVisibility);
  EXPECT_TRUE(chip.GetIsRequestForTesting());

  chip.SetTheme(PermissionChipTheme::kInUseActivityIndicator);
  EXPECT_FALSE(chip.GetIsRequestForTesting());

  chip.SetTheme(PermissionChipTheme::kBlockedActivityIndicator);
  EXPECT_FALSE(chip.GetIsRequestForTesting());

  chip.SetTheme(PermissionChipTheme::kOnSystemBlockedActivityIndicator);
  EXPECT_FALSE(chip.GetIsRequestForTesting());
}

TEST_F(WebUIPermissionChipTest, GetAnchorPrefersChipElementOverLocationBar) {
  SetBrowser(&mock_browser_);
  TestBrowserElements browser_elements(mock_browser_, kTestContext);
  ui::test::TestElement location_bar_element(kLocationBarElementId,
                                             kTestContext);
  ui::test::TestElement chip_element(
      PermissionChipView::kPermissionRequestChipElementId, kTestContext);
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);

  // Falls back to the location bar while the chip is not tracked.
  location_bar_element.Show();
  EXPECT_EQ(chip.GetAnchor().GetIfElement(), &location_bar_element);

  // Anchors to the chip once it is tracked.
  chip_element.Show();
  EXPECT_EQ(chip.GetAnchor().GetIfElement(), &chip_element);
}

TEST_F(WebUIPermissionChipTest,
       WaitForAnchorRunsImmediatelyIfElementIsTracked) {
  SetBrowser(&mock_browser_);
  TestBrowserElements browser_elements(mock_browser_, kTestContext);
  ui::test::TestElement chip_element(
      PermissionChipView::kPermissionRequestChipElementId, kTestContext);
  chip_element.Show();
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);

  base::test::TestFuture<void> future;
  chip.WaitForAnchor(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(chip.GetAnchor().GetIfElement(), &chip_element);
}

TEST_F(WebUIPermissionChipTest, WaitForAnchorWaitsForElementToBeShown) {
  SetBrowser(&mock_browser_);
  TestBrowserElements browser_elements(mock_browser_, kTestContext);
  ui::test::TestElement location_bar_element(kLocationBarElementId,
                                             kTestContext);
  location_bar_element.Show();
  ui::test::TestElement indicator_chip_element(
      PermissionChipView::kIndicatorChipElementId, kTestContext);
  ui::test::TestElement chip_element(
      PermissionChipView::kPermissionRequestChipElementId, kTestContext);
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);

  base::test::TestFuture<void> future;
  chip.WaitForAnchor(future.GetCallback());
  // Waits for the chip even though the location bar is already tracked.
  browser_threads_.FastForwardBy(kJustBeforeAnchorFallback);
  EXPECT_FALSE(future.IsReady());

  // Another chip being shown doesn't resolve the request.
  indicator_chip_element.Show();
  EXPECT_FALSE(future.IsReady());

  chip_element.Show();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(chip.GetAnchor().GetIfElement(), &chip_element);
}

TEST_F(WebUIPermissionChipTest, WaitForAnchorFallsBackAfterTimeout) {
  SetBrowser(&mock_browser_);
  TestBrowserElements browser_elements(mock_browser_, kTestContext);
  ui::test::TestElement location_bar_element(kLocationBarElementId,
                                             kTestContext);
  location_bar_element.Show();
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);

  base::test::TestFuture<void> future;
  chip.WaitForAnchor(future.GetCallback());
  browser_threads_.FastForwardBy(kJustBeforeAnchorFallback);
  EXPECT_FALSE(future.IsReady());

  browser_threads_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(chip.GetAnchor().GetIfElement(), &location_bar_element);
}

TEST_F(WebUIPermissionChipTest, WaitForAnchorFallsBackWhenChipIsHidden) {
  SetBrowser(&mock_browser_);
  TestBrowserElements browser_elements(mock_browser_, kTestContext);
  ui::test::TestElement location_bar_element(kLocationBarElementId,
                                             kTestContext);
  location_bar_element.Show();
  WebUIPermissionChip chip(location_bar_.get(),
                           PermissionChipView::kPermissionRequestChipElementId);
  chip.SetVisible(true);

  base::test::TestFuture<void> future;
  chip.WaitForAnchor(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // The callback is run rather than dropped when the chip is hidden.
  chip.SetVisible(false);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(chip.GetAnchor().GetIfElement(), &location_bar_element);
}

TEST_F(WebUIPermissionChipTest, WaitForAnchorDropsCallbackIfChipIsDestroyed) {
  SetBrowser(&mock_browser_);
  TestBrowserElements browser_elements(mock_browser_, kTestContext);
  ui::test::TestElement chip_element(
      PermissionChipView::kPermissionRequestChipElementId, kTestContext);
  auto chip = std::make_unique<WebUIPermissionChip>(
      location_bar_.get(), PermissionChipView::kPermissionRequestChipElementId);

  base::test::TestFuture<void> future;
  chip->WaitForAnchor(future.GetCallback());
  chip.reset();

  // Neither the element being shown nor the fallback timeout should reach the
  // destroyed chip.
  chip_element.Show();
  browser_threads_.FastForwardBy(kAnchorFallbackTimeout);
  EXPECT_FALSE(future.IsReady());
}

TEST_F(WebUIPermissionChipTest, DashboardChipsAnchorToTheirOwnElements) {
  SetBrowser(&mock_browser_);
  TestBrowserElements browser_elements(mock_browser_, kTestContext);
  // Show both chips, so that a chip looking up the wrong element ID is caught.
  ui::test::TestElement request_chip_element(
      PermissionChipView::kPermissionRequestChipElementId, kTestContext);
  request_chip_element.Show();
  ui::test::TestElement indicator_chip_element(
      PermissionChipView::kIndicatorChipElementId, kTestContext);
  indicator_chip_element.Show();
  WebUIPermissionDashboard dashboard(location_bar_.get());

  EXPECT_EQ(dashboard.GetRequestChip()->GetAnchor().GetIfElement(),
            &request_chip_element);
  // The dashboard's anchor is used for page info, which opens from the
  // indicator chip.
  EXPECT_EQ(dashboard.GetAnchor().GetIfElement(), &indicator_chip_element);
}
