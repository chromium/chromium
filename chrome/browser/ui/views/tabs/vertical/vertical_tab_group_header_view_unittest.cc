// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tab_groups/token_id.h"
#include "components/tabs/public/mock_tab_group.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

class MockDelegate : public VerticalTabGroupHeaderView::Delegate {
 public:
  MOCK_METHOD(void,
              ToggleCollapsedState,
              (ToggleTabGroupCollapsedStateOrigin),
              (override));
  MOCK_METHOD(views::Widget*, ShowGroupEditorBubble, (bool), (override));
  MOCK_METHOD(std::u16string, GetGroupContentString, (), (const, override));
  MOCK_METHOD(bool, IsValid, (), (const, override));
  MOCK_METHOD(void, InitHeaderDrag, (const ui::LocatedEvent&), (override));
  MOCK_METHOD(bool, ContinueHeaderDrag, (const ui::LocatedEvent&), (override));
  MOCK_METHOD(void, CancelHeaderDrag, (), (override));
  MOCK_METHOD(const TabGroup&, GetTabGroup, (), (const, override));
  MOCK_METHOD(void, UpdateHoverCard, (int), (const, override));
  MOCK_METHOD(void, HideHoverCard, (int), (const, override));
  MOCK_METHOD(bool, IsFocusInTabStrip, (), (override));
  MOCK_METHOD(std::unique_ptr<ExpandOnHoverLock>,
              AcquireExpandOnHoverLock,
              (),
              (override));
  MOCK_METHOD(void, ShiftGroupUp, (), (override));
  MOCK_METHOD(void, ShiftGroupDown, (), (override));
};

int GetPlatformDependentAccelerator() {
#if BUILDFLAG(IS_MAC)
  return ui::EF_COMMAND_DOWN;
#else
  return ui::EF_CONTROL_DOWN;
#endif
}

}  // namespace

class VerticalTabGroupHeaderViewTest
    : public views::ViewsTestBase,
      public testing::WithParamInterface<bool> {
 public:
  VerticalTabGroupHeaderViewTest() {
    if (UseGroupHeaderHoverCards()) {
      feature_list_.InitWithFeatures({features::kTabGroupHoverCards}, {});
    } else {
      feature_list_.InitWithFeatures({}, {features::kTabGroupHoverCards});
    }
  }

  bool UseGroupHeaderHoverCards() { return GetParam(); }

  ~VerticalTabGroupHeaderViewTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};
// TODO(crbug.com/501977260): Re-enable this test on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_TooltipText DISABLED_TooltipText
#else
#define MAYBE_TooltipText TooltipText
#endif
TEST_P(VerticalTabGroupHeaderViewTest, MAYBE_TooltipText) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  EXPECT_CALL(delegate, GetGroupContentString())
      .WillRepeatedly(testing::Return(u"3 tabs"));

  auto header = std::make_unique<VerticalTabGroupHeaderView>(delegate, nullptr,
                                                             &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  // Initialize with data
  tabs::TabGroupData data;
  data.visual_data = visual_data;
  header->OnDataChanged(data);

  // Empty tool tip if hover cards are enabled.
  std::u16string expected_tooltip =
      UseGroupHeaderHoverCards()
          ? u""
          : l10n_util::GetStringFUTF16(IDS_TAB_GROUPS_NAMED_GROUP_TOOLTIP,
                                       u"Group Title", u"3 tabs");

  EXPECT_EQ(header->GetTooltipText(), expected_tooltip);

  // Test unnamed group
  tab_groups::TabGroupVisualData unnamed_visual_data(
      u"", tab_groups::TabGroupColorId::kRed, false);
  data.visual_data = unnamed_visual_data;
  header->OnDataChanged(data);

  // Empty tool tip if hover cards are enabled.
  expected_tooltip = UseGroupHeaderHoverCards()
                         ? u""
                         : l10n_util::GetStringFUTF16(
                               IDS_TAB_GROUPS_UNNAMED_GROUP_TOOLTIP, u"3 tabs");

  EXPECT_EQ(header->GetTooltipText(), expected_tooltip);
}

// TODO(crbug.com/501977260): Re-enable this test on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ShowHoverCardOnMouseEnter DISABLED_ShowHoverCardOnMouseEnter
#else
#define MAYBE_ShowHoverCardOnMouseEnter ShowHoverCardOnMouseEnter
#endif
TEST_P(VerticalTabGroupHeaderViewTest, MAYBE_ShowHoverCardOnMouseEnter) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header =
      widget->SetContentsView(std::make_unique<VerticalTabGroupHeaderView>(
          delegate, nullptr, &visual_data));
  widget->Show();

  if (UseGroupHeaderHoverCards()) {
    EXPECT_CALL(delegate, UpdateHoverCard(testing::_));
  } else {
    EXPECT_CALL(delegate, UpdateHoverCard(testing::_)).Times(0);
  }

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  generator.MoveMouseTo(header->GetBoundsInScreen().CenterPoint());
}

// TODO(crbug.com/501977260): Re-enable this test on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_EditorBubbleButtonVisibilityOnHover \
  DISABLED_EditorBubbleButtonVisibilityOnHover
#else
#define MAYBE_EditorBubbleButtonVisibilityOnHover \
  EditorBubbleButtonVisibilityOnHover
#endif
TEST_P(VerticalTabGroupHeaderViewTest,
       MAYBE_EditorBubbleButtonVisibilityOnHover) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header =
      widget->SetContentsView(std::make_unique<VerticalTabGroupHeaderView>(
          delegate, nullptr, &visual_data));
  widget->Show();

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());

  auto move_mouse_to = [&](bool inside_view) {
    if (inside_view) {
      generator.MoveMouseTo(header->GetBoundsInScreen().CenterPoint());
    } else {
      generator.MoveMouseTo(header->GetBoundsInScreen().bottom_right() +
                            gfx::Vector2d(10, 10));
    }
  };

  auto check_editor_bubble_button_visible = [&](bool expected_visibility) {
    EXPECT_EQ(expected_visibility,
              header->editor_bubble_button()->GetVisible());
  };

  // Move mouse outside the header.
  move_mouse_to(false);
  check_editor_bubble_button_visible(false);

  // Move mouse over the header.
  move_mouse_to(true);
  check_editor_bubble_button_visible(true);

  // Move mouse outside the header again.
  move_mouse_to(false);
  check_editor_bubble_button_visible(false);
}

TEST_P(VerticalTabGroupHeaderViewTest, OnKeyPress_ShiftUp) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<VerticalTabGroupHeaderView>(delegate, nullptr,
                                                             &visual_data);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_UP,
                     GetPlatformDependentAccelerator());

  EXPECT_CALL(delegate, ShiftGroupUp()).Times(1);

  EXPECT_TRUE(header->OnKeyPressed(event));
}

TEST_P(VerticalTabGroupHeaderViewTest, OnKeyPress_ShiftDown) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<VerticalTabGroupHeaderView>(delegate, nullptr,
                                                             &visual_data);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                     GetPlatformDependentAccelerator());

  EXPECT_CALL(delegate, ShiftGroupDown()).Times(1);

  EXPECT_TRUE(header->OnKeyPressed(event));
}

INSTANTIATE_TEST_SUITE_P(All,
                         VerticalTabGroupHeaderViewTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "HoverCardEnabled"
                                             : "HoverCardDisabled";
                         });
