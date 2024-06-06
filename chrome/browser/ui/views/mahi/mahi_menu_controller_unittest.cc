// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/chromeos/mahi/test/scoped_mahi_web_contents_manager_for_testing.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/test/mock_magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/browser/ui/views/mahi/mahi_condensed_menu_view.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "ash/system/mahi/test/mock_mahi_media_app_events_proxy.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos::mahi {

using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

class MahiMenuControllerTest : public ChromeViewsTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  MahiMenuControllerTest() {
    if (IsMagicBoostEnabled()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kMahi, features::kMagicBoost},
          /*disabled_features=*/{});

      scoped_magic_boost_card_controller_ =
          std::make_unique<ScopedMagicBoostCardControllerForTesting>(
              &mock_magic_boost_card_controller_);
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kMahi},
          /*disabled_features=*/{features::kMagicBoost});
    }

    menu_controller_ =
        std::make_unique<MahiMenuController>(read_write_cards_ui_controller_);

    scoped_mahi_web_contents_manager_ =
        std::make_unique<::mahi::ScopedMahiWebContentsManagerForTesting>(
            &fake_mahi_web_contents_manager_);
    // Sets the focused page's distillability to true so that it does not block
    // the menu widget's display.
    ChangePageDistillability(true);
    // Sets the default pref is true for testing.
    ChangePrefValue(true);
  }

  bool IsMagicBoostEnabled() const { return GetParam(); }

  MahiMenuControllerTest(const MahiMenuControllerTest&) = delete;
  MahiMenuControllerTest& operator=(const MahiMenuControllerTest&) = delete;

  ~MahiMenuControllerTest() override = default;

  void TearDown() override {
    // Manually reset `menu_controller_` here because it requires the existence
    // of `mock_mahi_media_app_events_proxy_` to destroy.
    menu_controller_.reset();
    ChromeViewsTestBase::TearDown();
  }

  MahiMenuController* menu_controller() { return menu_controller_.get(); }

  MockMagicBoostCardController& mock_magic_boost_card_controller() {
    return mock_magic_boost_card_controller_;
  }

  void ChangePageDistillability(bool value) {
    fake_mahi_web_contents_manager_.set_focused_web_content_is_distillable(
        value);
  }

  void ChangePrefValue(bool value) {
    fake_mahi_web_contents_manager_.SetPrefForTesting(value);
  }

 protected:
  ReadWriteCardsUiController read_write_cards_ui_controller_;

 private:
  base::test::ScopedFeatureList feature_list_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      ash::switches::SetIgnoreMahiSecretKeyForTest();
  // Providing a mock MahiMediaAppEvnetsProxy to satisfy MahiMenuController.
  testing::NiceMock<::ash::MockMahiMediaAppEventsProxy>
      mock_mahi_media_app_events_proxy_;
  chromeos::ScopedMahiMediaAppEventsProxySetter
      scoped_mahi_media_app_events_proxy_{&mock_mahi_media_app_events_proxy_};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<MahiMenuController> menu_controller_;

  ::mahi::FakeMahiWebContentsManager fake_mahi_web_contents_manager_;
  std::unique_ptr<::mahi::ScopedMahiWebContentsManagerForTesting>
      scoped_mahi_web_contents_manager_;

  // TODO(b/344037679): Remove these when we use
  // `ReadWriteCardsManagerImpl` to fetch the controller.
  NiceMock<MockMagicBoostCardController> mock_magic_boost_card_controller_;
  std::unique_ptr<ScopedMagicBoostCardControllerForTesting>
      scoped_magic_boost_card_controller_;
};

// Tests the behavior of the controller when there's no text selected when
// `OnTextAvailable()` is triggered.
TEST_P(MahiMenuControllerTest, TextNotSelected) {
  ON_CALL(mock_magic_boost_card_controller(),
          ShouldQuickAnswersAndMahiShowOptIn)
      .WillByDefault(Return(false));

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
TEST_P(MahiMenuControllerTest, BoundsChanged) {
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  gfx::Rect anchor_bounds = gfx::Rect(50, 50, 25, 100);
  menu_controller()->OnTextAvailable(anchor_bounds,
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");
  auto* widget = menu_controller()->menu_widget_for_test();
  EXPECT_TRUE(widget);

  EXPECT_EQ(editor_menu::GetEditorMenuBounds(anchor_bounds,
                                             widget->GetContentsView()),
            widget->GetRestoredBounds());

  anchor_bounds = gfx::Rect(0, 50, 55, 80);

  // Widget should change bounds accordingly.
  menu_controller()->OnAnchorBoundsChanged(anchor_bounds);
  EXPECT_EQ(editor_menu::GetEditorMenuBounds(anchor_bounds,
                                             widget->GetContentsView()),
            widget->GetRestoredBounds());
}

// Tests the behavior of the controller when there's text selected when
// `OnTextAvailable()` is triggered.
TEST_P(MahiMenuControllerTest, TextSelected) {
  ON_CALL(mock_magic_boost_card_controller(),
          ShouldQuickAnswersAndMahiShowOptIn)
      .WillByDefault(Return(false));

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

TEST_P(MahiMenuControllerTest, ShowOptInUiTextNotSelected) {
  ON_CALL(mock_magic_boost_card_controller(),
          ShouldQuickAnswersAndMahiShowOptIn)
      .WillByDefault(Return(true));

  // `ShowOptInUi` should be called when Magic Boost is enabled.
  if (IsMagicBoostEnabled()) {
    EXPECT_CALL(mock_magic_boost_card_controller(), ShowOptInUi);
    menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                       /*selected_text=*/"",
                                       /*surrounding_text=*/"");

    EXPECT_CALL(mock_magic_boost_card_controller(), CloseOptInUi);
    menu_controller()->OnDismiss(/*is_other_command_executed=*/false);

    Mock::VerifyAndClear(&mock_magic_boost_card_controller());
    return;
  }

  // Otherwise, no opt in UI is shown and `MahiMenuView` is shown.
  EXPECT_CALL(mock_magic_boost_card_controller(), ShowOptInUi).Times(0);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  EXPECT_TRUE(menu_controller()->menu_widget_for_test());
  EXPECT_TRUE(menu_controller()->menu_widget_for_test()->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MahiMenuView>(
      menu_controller()->menu_widget_for_test()->GetContentsView()));

  EXPECT_CALL(mock_magic_boost_card_controller(), CloseOptInUi).Times(0);
  menu_controller()->OnDismiss(/*is_other_command_executed=*/false);
}

TEST_P(MahiMenuControllerTest, ShowOptInUiTextSelected) {
  ON_CALL(mock_magic_boost_card_controller(),
          ShouldQuickAnswersAndMahiShowOptIn)
      .WillByDefault(Return(true));

  // `ShowOptInUi` should be called when Magic Boost is enabled.
  if (IsMagicBoostEnabled()) {
    EXPECT_CALL(mock_magic_boost_card_controller(), ShowOptInUi);
    menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                       /*selected_text=*/"test selected text",
                                       /*surrounding_text=*/"");

    EXPECT_CALL(mock_magic_boost_card_controller(), CloseOptInUi);
    menu_controller()->OnDismiss(/*is_other_command_executed=*/false);

    Mock::VerifyAndClear(&mock_magic_boost_card_controller());
    return;
  }

  // Otherwise, no opt in UI is shown and the condense menu view is shown.
  EXPECT_CALL(mock_magic_boost_card_controller(), ShowOptInUi).Times(0);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"test selected text",
                                     /*surrounding_text=*/"");

  EXPECT_TRUE(read_write_cards_ui_controller_.widget_for_test());
  EXPECT_TRUE(read_write_cards_ui_controller_.widget_for_test()->IsVisible());
  EXPECT_TRUE(read_write_cards_ui_controller_.GetMahiUiForTest());
  EXPECT_TRUE(views::IsViewClass<MahiCondensedMenuView>(
      read_write_cards_ui_controller_.GetMahiUiForTest()));

  EXPECT_CALL(mock_magic_boost_card_controller(), CloseOptInUi).Times(0);
  menu_controller()->OnDismiss(/*is_other_command_executed=*/false);
}

// Tests the behavior of the controller when pref state changed.
TEST_P(MahiMenuControllerTest, PrefChange) {
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

  // If pref value is false, then menu widget should not be triggered.
  ChangePrefValue(false);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");
  EXPECT_FALSE(menu_controller()->menu_widget_for_test());

  // Set pref to true should show the widget again.
  ChangePrefValue(true);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");
  EXPECT_TRUE(menu_controller()->menu_widget_for_test());
  EXPECT_TRUE(menu_controller()->menu_widget_for_test()->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MahiMenuView>(
      menu_controller()->menu_widget_for_test()->GetContentsView()));
}

TEST_P(MahiMenuControllerTest, DistillableMetrics) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram, true,
                                     0);
  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram,
                                     false, 0);

  ChangePageDistillability(false);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram, true,
                                     0);
  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram,
                                     false, 1);

  // If page is not distillable, then menu widget should not be triggered.
  ChangePageDistillability(true);
  menu_controller()->OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                     /*selected_text=*/"",
                                     /*surrounding_text=*/"");

  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram, true,
                                     1);
  histogram_tester.ExpectBucketCount(kMahiContextMenuDistillableHistogram,
                                     false, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MahiMenuControllerTest,
                         /*IsMagicBoostEnabled()=*/testing::Bool());

#if BUILDFLAG(IS_CHROMEOS_ASH)
class MahiMenuControllerFeatureKeyTest : public ChromeViewsTestBase {
 public:
  MahiMenuControllerFeatureKeyTest() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(ash::switches::kMahiFeatureKey, "hello");
  }

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
  // Providing a mock MahiMediaAppEvnetsProxy to satisfy MahiMenuController.
  testing::NiceMock<::ash::MockMahiMediaAppEventsProxy>
      mock_mahi_media_app_events_proxy_;
  chromeos::ScopedMahiMediaAppEventsProxySetter
      scoped_mahi_media_app_events_proxy_{&mock_mahi_media_app_events_proxy_};
};

TEST_F(MahiMenuControllerFeatureKeyTest, DoesNotShowWidgetIfFeatureKeyIsWrong) {
  ReadWriteCardsUiController read_write_cards_ui_controller;
  ::mahi::FakeMahiWebContentsManager fake_mahi_web_contents_manager;
  fake_mahi_web_contents_manager.set_focused_web_content_is_distillable(true);
  ::mahi::ScopedMahiWebContentsManagerForTesting
      scoped_mahi_web_contents_manager(&fake_mahi_web_contents_manager);
  MahiMenuController menu_controller(read_write_cards_ui_controller);

  menu_controller.OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                  /*selected_text=*/"",
                                  /*surrounding_text=*/"");

  EXPECT_THAT(menu_controller.menu_widget_for_test(), IsNull());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace chromeos::mahi
