// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/test/mock_mahi_media_app_events_proxy.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/browser/ui/views/mahi/mahi_condensed_menu_view.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"

namespace chromeos::mahi {

using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

class MahiMenuControllerTest : public ChromeViewsTestBase {
 public:
  MahiMenuControllerTest() {
    menu_controller_ =
        std::make_unique<MahiMenuController>(read_write_cards_ui_controller_);

    scoped_mahi_web_contents_manager_ =
        std::make_unique<chromeos::ScopedMahiWebContentsManagerOverride>(
            &fake_mahi_web_contents_manager_);

    fake_mahi_manager_ = std::make_unique<ash::FakeMahiManager>();
    // Sets the focused page's distillability to true so that it does not block
    // the menu widget's display.
    ChangePageDistillability(true);
  }

  MahiMenuControllerTest(const MahiMenuControllerTest&) = delete;
  MahiMenuControllerTest& operator=(const MahiMenuControllerTest&) = delete;

  ~MahiMenuControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
    ChromeViewsTestBase::SetUp();
  }

  void TearDown() override {
    // Manually reset `menu_controller_` here because it requires the existence
    // of `mock_mahi_media_app_events_proxy_` to destroy.
    menu_controller_.reset();
    ChromeViewsTestBase::TearDown();
  }

  MahiMenuController* menu_controller() { return menu_controller_.get(); }

  void ChangePageDistillability(bool value) {
    fake_mahi_web_contents_manager_.set_focused_web_content_is_distillable(
        value);
  }

 protected:
  ReadWriteCardsUiController read_write_cards_ui_controller_;
  std::unique_ptr<ash::FakeMahiManager> fake_mahi_manager_;

 private:
  base::test::ScopedFeatureList feature_list_;

  // Providing a mock MahiMediaAppEvnetsProxy and a fake mahi manager to satisfy
  // MahiMenuController.
  testing::NiceMock<::ash::MockMahiMediaAppEventsProxy>
      mock_mahi_media_app_events_proxy_;
  chromeos::ScopedMahiMediaAppEventsProxySetter
      scoped_mahi_media_app_events_proxy_{&mock_mahi_media_app_events_proxy_};

  std::unique_ptr<MahiMenuController> menu_controller_;

  ::mahi::FakeMahiWebContentsManager fake_mahi_web_contents_manager_;
  std::unique_ptr<chromeos::ScopedMahiWebContentsManagerOverride>
      scoped_mahi_web_contents_manager_;
};

// Tests the behavior of the controller when there's no text selected when
// `OnTextAvailable()` is triggered.
TEST_F(MahiMenuControllerTest, TextNotSelected) {
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // Menu widget should show when text is displayed.
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  EXPECT_TRUE(menu_controller()->menu_widget_for_test());
  EXPECT_TRUE(menu_controller()->menu_widget_for_test()->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MahiMenuView>(
      menu_controller()->menu_widget_for_test()->GetContentsView()));

  // Menu widget should hide when dismissed.
  menu_controller()->OnDismiss(/*is_other_command_executed=*/false);
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // If page is not distillable, then menu widget should not be triggered.
  ChangePageDistillability(false);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  EXPECT_FALSE(menu_controller()->menu_widget_for_test());
}

// Tests the behavior of the controller when `OnAnchorBoundsChanged()` is
// triggered.
TEST_F(MahiMenuControllerTest, BoundsChanged) {
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  gfx::Rect anchor_bounds = gfx::Rect(50, 50, 25, 100);
  menu_controller()->OnTextAvailable(anchor_bounds,
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");
  auto* widget = menu_controller()->menu_widget_for_test();
  EXPECT_TRUE(widget);

  EXPECT_EQ(
      editor_menu::GetEditorMenuBounds(anchor_bounds, widget->GetContentsView(),
                                       editor_menu::CardType::kMahiDefaultMenu),
      widget->GetRestoredBounds());

  anchor_bounds = gfx::Rect(0, 50, 55, 80);

  // Widget should change bounds accordingly.
  menu_controller()->OnAnchorBoundsChanged(anchor_bounds);
  EXPECT_EQ(
      editor_menu::GetEditorMenuBounds(anchor_bounds, widget->GetContentsView(),
                                       editor_menu::CardType::kMahiDefaultMenu),
      widget->GetRestoredBounds());
}

// Tests the behavior of the controller when there's text selected when
// `OnTextAvailable()` is triggered.
TEST_F(MahiMenuControllerTest, TextSelected) {
  EXPECT_FALSE(read_write_cards_ui_controller_.widget_for_test());

  // Menu widget should show when text is displayed.
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"test selected text",
                                     /*surrounding_text=*/"");

  EXPECT_TRUE(read_write_cards_ui_controller_.widget_for_test());
  EXPECT_TRUE(read_write_cards_ui_controller_.widget_for_test()->IsVisible());
  EXPECT_TRUE(read_write_cards_ui_controller_.GetMahiUiForTest());
  EXPECT_TRUE(views::IsViewClass<MahiCondensedMenuView>(
      read_write_cards_ui_controller_.GetMahiUiForTest()));

  // Menu widget should hide when dismissed.
  menu_controller()->OnDismiss(/*is_other_command_executed=*/false);
  EXPECT_FALSE(read_write_cards_ui_controller_.widget_for_test());
  EXPECT_FALSE(read_write_cards_ui_controller_.GetMahiUiForTest());
}

// Tests the behavior of the controller when feature enable state changed.
TEST_F(MahiMenuControllerTest, FeatureEnableStatusChange) {
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // Menu widget should show when text is displayed as the default is that Mahi
  // is enabled.
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  EXPECT_TRUE(menu_controller()->menu_widget_for_test());
  EXPECT_TRUE(menu_controller()->menu_widget_for_test()->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MahiMenuView>(
      menu_controller()->menu_widget_for_test()->GetContentsView()));

  // Menu widget should hide when dismissed.
  menu_controller()->OnDismiss(/*is_other_command_executed=*/false);
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // If MahiManager says the feature is not enabled, then menu widget should not
  // be triggered.
  fake_mahi_manager_->set_mahi_enabled(false);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // Set mahi_enabled to true should show the widget again.
  fake_mahi_manager_->set_mahi_enabled(true);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");
  EXPECT_TRUE(menu_controller()->menu_widget_for_test());
  EXPECT_TRUE(menu_controller()->menu_widget_for_test()->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MahiMenuView>(
      menu_controller()->menu_widget_for_test()->GetContentsView()));
}

TEST_F(MahiMenuControllerTest, DistillableMetrics) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram, true,
                                     0);
  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram,
                                     false, 0);

  ChangePageDistillability(false);
  menu_controller()->RecordPageDistillable();

  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram, true,
                                     0);
  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram,
                                     false, 1);

  // If page is not distillable, then menu widget should not be triggered.
  ChangePageDistillability(true);
  menu_controller()->RecordPageDistillable();

  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram, true,
                                     1);
  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram,
                                     false, 1);
}

}  // namespace chromeos::mahi
