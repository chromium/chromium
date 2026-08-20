// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace

class WebUIPermissionChipTest : public testing::Test {
 protected:
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

  content::BrowserTaskEnvironment browser_threads_;
  TestingProfile profile_;
  views::View dummy_view_;
  testing::NiceMock<MockWebUIToolbarControlDelegate> mock_toolbar_delegate_;
  std::unique_ptr<TestLocationBarModel> location_bar_model_;
  std::unique_ptr<TestLocationBarViewDelegate> delegate_;
  std::unique_ptr<WebUILocationBar> location_bar_;
};

TEST_F(WebUIPermissionChipTest, AnnounceAlertOnExpandEnded) {
  WebUIPermissionChip chip(location_bar_.get());
  const std::u16string message = u"Test Announcement";
  chip.SetMessage(message);

  EXPECT_CALL(mock_toolbar_delegate_, AnnounceAlert(message));

  // Simulate expansion animation ended IPC from WebUI. This should trigger the
  // a11y announcement so that screen readers speak the permission request
  // text once the chip has fully expanded and is visible.
  // In WebUIPermissionChip, OnExpandAnimationEnded() explicitly calls
  // AnnounceAlert() to perform this announcement.
  chip.AnimateExpand(base::Milliseconds(350));
  chip.OnExpandAnimationEnded();
}

TEST_F(WebUIPermissionChipTest, AnnounceAlertAndText) {
  WebUIPermissionChip chip(location_bar_.get());
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
  WebUIPermissionChip chip(location_bar_.get());
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
  WebUIPermissionChip chip(location_bar_.get());
  EXPECT_EQ(chip.GetThemeForTesting(), PermissionChipTheme::kNormalVisibility);

  chip.SetTheme(PermissionChipTheme::kInUseActivityIndicator);
  EXPECT_EQ(chip.GetThemeForTesting(),
            PermissionChipTheme::kInUseActivityIndicator);
}

TEST_F(WebUIPermissionChipTest, GetTextForTesting) {
  WebUIPermissionChip chip(location_bar_.get());
  EXPECT_TRUE(chip.GetTextForTesting().empty());

  const std::u16string message = u"Camera in use";
  chip.SetMessage(message);
  EXPECT_EQ(chip.GetTextForTesting(), message);
}

TEST_F(WebUIPermissionChipTest, GetIsRequestForTesting) {
  WebUIPermissionChip chip(location_bar_.get());
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
