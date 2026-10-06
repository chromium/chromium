// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_segmented_control.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

class VerticalTabStripSegmentedControlTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(&profile_));
    EXPECT_CALL(std::as_const(mock_browser_window_interface_), GetProfile())
        .WillRepeatedly(testing::Return(&profile_));

    animation_controller_ = std::make_unique<BrowserAnimationController>(
        mock_browser_window_interface_);
    animation_controller_->AddAnimationProvider(
        std::make_unique<OrganizerPanelAnimations>());
    state_controller_ = std::make_unique<OrganizerPanelController>(
        mock_browser_window_interface_, /*root_action_item=*/nullptr);

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    control_ = widget_->SetContentsView(
        std::make_unique<VerticalTabStripSegmentedControl>(
            &mock_browser_window_interface_));
    widget_->Show();
  }

  void TearDown() override {
    control_ = nullptr;
    widget_.reset();
    state_controller_.reset();
    animation_controller_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  VerticalTabStripSegmentedControl* control() { return control_; }
  OrganizerPanelController* state_controller() {
    return state_controller_.get();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<VerticalTabStripSegmentedControl> control_;
  std::unique_ptr<BrowserAnimationController> animation_controller_;
  std::unique_ptr<OrganizerPanelController> state_controller_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  TestingProfile profile_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(VerticalTabStripSegmentedControlTest, PreferredSize) {
  constexpr auto kTotalInsets = gfx::Insets(2);
  const int button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  const int expected_width = (button_size * 2) + kTotalInsets.width();
  const int expected_height = button_size + kTotalInsets.height();
  EXPECT_EQ(control()->GetPreferredSize(),
            gfx::Size(expected_width, expected_height));
}

TEST_F(VerticalTabStripSegmentedControlTest, DefaultActiveSegmentAndButtons) {
  EXPECT_EQ(control()->active_segment(),
            VerticalTabStripSegmentedControl::Segment::kTabStrip);

  auto* tab_strip_btn = control()->GetButton(
      VerticalTabStripSegmentedControl::Segment::kTabStrip);
  ASSERT_TRUE(tab_strip_btn);
  EXPECT_TRUE(tab_strip_btn->GetVisible());
  EXPECT_EQ(tab_strip_btn->GetProperty(views::kElementIdentifierKey),
            kVerticalTabStripTabStripButtonElementId);
  EXPECT_EQ(tab_strip_btn->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_TAB_STRIP_BUTTON_TOOLTIP));

  auto* organizer_btn = control()->GetButton(
      VerticalTabStripSegmentedControl::Segment::kOrganizer);
  ASSERT_TRUE(organizer_btn);
  EXPECT_TRUE(organizer_btn->GetVisible());
  EXPECT_EQ(organizer_btn->GetProperty(views::kElementIdentifierKey),
            kTabSearchButtonElementId);
  EXPECT_EQ(organizer_btn->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_SEARCH));
}

TEST_F(VerticalTabStripSegmentedControlTest,
       StateControllerUpdatesActiveSegment) {
  // Organizer panel visible -> Organizer segment active.
  state_controller()->SetOrganizerVisible(true);
  EXPECT_EQ(control()->active_segment(),
            VerticalTabStripSegmentedControl::Segment::kOrganizer);

  // Organizer panel hidden -> TabStrip segment active.
  state_controller()->SetOrganizerVisible(false);
  EXPECT_EQ(control()->active_segment(),
            VerticalTabStripSegmentedControl::Segment::kTabStrip);
}

TEST_F(VerticalTabStripSegmentedControlTest, ButtonPressTogglesActiveSegment) {
  EXPECT_EQ(control()->active_segment(),
            VerticalTabStripSegmentedControl::Segment::kTabStrip);

  // Pressing organizer button triggers state controller to show organizer
  // panel.
  control()
      ->GetButton(VerticalTabStripSegmentedControl::Segment::kOrganizer)
      ->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                                  ui::EF_NONE));
  control()
      ->GetButton(VerticalTabStripSegmentedControl::Segment::kOrganizer)
      ->OnKeyReleased(ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_SPACE,
                                   ui::EF_NONE));
  EXPECT_EQ(control()->active_segment(),
            VerticalTabStripSegmentedControl::Segment::kOrganizer);

  // Pressing tab strip button triggers state controller to hide organizer
  // panel.
  control()
      ->GetButton(VerticalTabStripSegmentedControl::Segment::kTabStrip)
      ->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                                  ui::EF_NONE));
  control()
      ->GetButton(VerticalTabStripSegmentedControl::Segment::kTabStrip)
      ->OnKeyReleased(ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_SPACE,
                                   ui::EF_NONE));
  EXPECT_EQ(control()->active_segment(),
            VerticalTabStripSegmentedControl::Segment::kTabStrip);
}
