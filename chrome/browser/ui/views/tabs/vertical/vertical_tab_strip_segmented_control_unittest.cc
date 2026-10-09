// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_segmented_control.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
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
  TestingProfile* profile() { return &profile_; }

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
  constexpr auto kTotalInsets = gfx::Insets(3);
  constexpr int kBetweenChildSpacing = 4;
  const int button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  const int expected_width =
      (button_size * 2) + kBetweenChildSpacing + kTotalInsets.width();
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
  EXPECT_TRUE(tab_strip_btn->GetHasInkDropActionOnClick());
  EXPECT_EQ(views::InkDrop::Get(tab_strip_btn)->GetMode(),
            views::InkDropHost::InkDropMode::ON);
  EXPECT_EQ(views::InkDrop::Get(tab_strip_btn)->GetBaseColor(),
            tab_strip_btn->GetColorProvider()->GetColor(
                ui::kColorSysStateHoverOnSubtle));

  auto* organizer_btn = control()->GetButton(
      VerticalTabStripSegmentedControl::Segment::kOrganizer);
  ASSERT_TRUE(organizer_btn);
  EXPECT_TRUE(organizer_btn->GetVisible());
  EXPECT_EQ(organizer_btn->GetProperty(views::kElementIdentifierKey),
            kTabSearchButtonElementId);
  EXPECT_EQ(organizer_btn->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_SEARCH));
  EXPECT_TRUE(organizer_btn->GetHasInkDropActionOnClick());
  EXPECT_EQ(views::InkDrop::Get(organizer_btn)->GetMode(),
            views::InkDropHost::InkDropMode::ON);
  EXPECT_EQ(views::InkDrop::Get(organizer_btn)->GetBaseColor(),
            organizer_btn->GetColorProvider()->GetColor(
                ui::kColorSysStateHoverOnSubtle));
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

TEST_F(VerticalTabStripSegmentedControlTest, ContextMenuOnlyOnOrganizerButton) {
  auto* tab_strip_btn = control()->GetButton(
      VerticalTabStripSegmentedControl::Segment::kTabStrip);
  auto* organizer_btn = control()->GetButton(
      VerticalTabStripSegmentedControl::Segment::kOrganizer);

  EXPECT_EQ(tab_strip_btn->context_menu_controller(), nullptr);
  EXPECT_EQ(organizer_btn->context_menu_controller(), control());

  // Triggering context menu on tab strip button should not create menu model.
  control()->ShowContextMenuForViewImpl(tab_strip_btn, gfx::Point(),
                                        ui::mojom::MenuSourceType::kMouse);
  EXPECT_EQ(control()->menu_model_for_testing(), nullptr);

  // Triggering context menu on organizer button when pinned shows "Unpin".
  profile()->GetPrefs()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, true);
  control()->ShowContextMenuForViewImpl(organizer_btn, gfx::Point(),
                                        ui::mojom::MenuSourceType::kMouse);
  auto* model = control()->menu_model_for_testing();
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->GetItemCount(), 1u);
  EXPECT_EQ(model->GetCommandIdAt(0), IDC_TAB_SEARCH_TOGGLE_PIN);
  EXPECT_EQ(model->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_TAB_SEARCH_BUTTON_CXMENU_UNPIN));
  EXPECT_EQ(model->GetElementIdentifierAt(0),
            VerticalTabStripSegmentedControl::kTabSearchUnpinMenuItem);

  // Triggering context menu when unpinned shows "Pin".
  profile()->GetPrefs()->SetBoolean(prefs::kTabSearchPinnedToTabstrip, false);
  control()->ShowContextMenuForViewImpl(organizer_btn, gfx::Point(),
                                        ui::mojom::MenuSourceType::kMouse);
  model = control()->menu_model_for_testing();
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->GetItemCount(), 1u);
  EXPECT_EQ(model->GetCommandIdAt(0), IDC_TAB_SEARCH_TOGGLE_PIN);
  EXPECT_EQ(model->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_TAB_SEARCH_BUTTON_CXMENU_PIN));
}
