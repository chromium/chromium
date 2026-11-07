// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/i18n/message_formatter.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_unfocus_button.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/mock_tab_group.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class MockDelegate : public TabGroupHeaderView::Delegate,
                     public TabGroupUnfocusButton::Delegate {
 public:
  MockDelegate() {
    ON_CALL(*this, IsValid).WillByDefault(testing::Return(true));
    ON_CALL(*this, GetTabGroup)
        .WillByDefault(testing::ReturnRef(default_tab_group_));
    ON_CALL(*this, GetTabClosingHelper).WillByDefault(testing::Return(nullptr));
    ON_CALL(*this, CanShowEditorBubbleButton)
        .WillByDefault(testing::Return(true));
    ON_CALL(*this, GetTabGroupData)
        .WillByDefault(testing::ReturnRef(default_tab_group_data_));
    // Mirror `TabGroupView` for persistent groups: the focused header style
    // follows the focused state.
    ON_CALL(*this, ShouldUseFocusedHeaderStyle).WillByDefault([this]() {
      return IsGroupFocused();
    });
  }
  MOCK_METHOD(void,
              ToggleCollapsedState,
              (ToggleTabGroupCollapsedStateOrigin),
              (override));
  MOCK_METHOD(std::unique_ptr<views::Widget>,
              ShowGroupEditorBubble,
              (bool),
              (override));
  MOCK_METHOD(void, ExitFocusMode, (), (override));
  MOCK_METHOD(bool, IsValid, (), (const, override));
  MOCK_METHOD(void, InitHeaderDrag, (const ui::LocatedEvent&), (override));
  MOCK_METHOD(bool, ContinueHeaderDrag, (const ui::LocatedEvent&), (override));
  MOCK_METHOD(void, CancelHeaderDrag, (), (override));
  MOCK_METHOD(const TabGroup&, GetTabGroup, (), (const, override));
  MOCK_METHOD(const tabs::TabGroupData&,
              GetTabGroupData,
              (),
              (const, override));
  MOCK_METHOD(void, UpdateHoverCard, (int), (const, override));
  MOCK_METHOD(void, HideHoverCard, (int), (const, override));
  MOCK_METHOD(bool, IsFocusInTabStrip, (), (override));
  MOCK_METHOD(std::unique_ptr<ExpandOnHoverLock>,
              AcquireExpandOnHoverLock,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<TabHoverCardController::ScopedHideHoverCardLock>,
              AcquireHoverCardHideLock,
              (),
              (override));
  MOCK_METHOD(void, ShiftGroupPrevious, (), (override));
  MOCK_METHOD(void, ShiftGroupNext, (), (override));
  MOCK_METHOD(bool, IsGroupFocused, (), (const, override));
  MOCK_METHOD(bool, ShouldUseFocusedHeaderStyle, (), (const, override));
  MOCK_METHOD(bool, IsUnfocusButtonFocused, (), (const, override));
  MOCK_METHOD(bool, CanShowEditorBubbleButton, (), (const, override));
  MOCK_METHOD(HorizontalTabClosingHelper*,
              GetTabClosingHelper,
              (),
              (const, override));

  tabs::TabGroupData default_tab_group_data_;

 private:
  tabs::MockTabGroup default_tab_group_{nullptr,
                                        tab_groups::TabGroupId::GenerateNew(),
                                        tab_groups::TabGroupVisualData()};
};

int GetPlatformDependentAccelerator() {
#if BUILDFLAG(IS_MAC)
  return ui::EF_COMMAND_DOWN;
#else
  return ui::EF_CONTROL_DOWN;
#endif
}

}  // namespace

class TabGroupHeaderViewTest : public views::ViewsTestBase {
 public:
  TabGroupHeaderViewTest() = default;

  void MoveMouseTo(ui::test::EventGenerator& generator,
                   const views::View* view,
                   bool inside_view) {
    if (inside_view) {
      generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    } else {
      generator.MoveMouseTo(view->GetBoundsInScreen().bottom_right() +
                            gfx::Vector2d(10, 10));
    }
  }

  void SetGroupTitleAndLayout(TabGroupHeaderView* header,
                              MockDelegate& delegate,
                              const std::u16string& title) {
    tab_groups::TabGroupVisualData visual_data(
        title, tab_groups::TabGroupColorId::kBlue, false);
    delegate.default_tab_group_data_.visual_data = visual_data;
    header->OnDataChanged(delegate.default_tab_group_data_);
    const gfx::Size pref_size = header->CalculatePreferredSize({});
    header->SetBounds(0, 0, pref_size.width(), pref_size.height());
    views::test::RunScheduledLayout(header);
  }

  int CenteredTitleX(TabGroupHeaderView* header) {
    return (header->width() - header->title_label_for_testing()->width()) / 2;
  }

  ~TabGroupHeaderViewTest() override = default;
};

// Tooltip text for tab group header view is empty because
// hover cards are shown instead.
TEST_F(TabGroupHeaderViewTest, HeaderViewTooltipTextIsEmpty) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  // Initialize with data
  tabs::TabGroupData data;
  data.visual_data = visual_data;
  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

  EXPECT_EQ(header->GetTooltipText(), u"");

  // Test unnamed group
  tab_groups::TabGroupVisualData unnamed_visual_data(
      u"", tab_groups::TabGroupColorId::kRed, false);
  data.visual_data = unnamed_visual_data;
  header->OnDataChanged(data);

  EXPECT_EQ(header->GetTooltipText(), u"");
}

TEST_F(TabGroupHeaderViewTest, TitleLabelHeightWhenConstrained) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  header->OnDataChanged(data);

  // Set the header bounds to a height smaller than the label's preferred line
  // height.
  const int constrained_height = 10;
  header->SetBounds(0, 0, 200, constrained_height);
  header->DeprecatedLayoutImmediately();

  // The label height should be constrained_height rather than snapping to 0.
  EXPECT_GT(header->title_label_for_testing()->bounds().height(), 0);
  EXPECT_EQ(header->title_label_for_testing()->bounds().height(),
            constrained_height);
}

TEST_F(TabGroupHeaderViewTest, ShowHoverCardOnMouseEnter) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, UpdateHoverCard(testing::_));

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  MoveMouseTo(generator, header, true);
}

TEST_F(TabGroupHeaderViewTest, EditorBubbleButtonVisibilityOnHover) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  widget->Show();

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());

  auto check_editor_bubble_button_visible = [&](bool expected_visibility) {
    EXPECT_EQ(expected_visibility,
              header->editor_bubble_button()->GetVisible());
  };

  // Move mouse outside the header.
  MoveMouseTo(generator, header, false);
  check_editor_bubble_button_visible(false);

  // Move mouse over the header.
  MoveMouseTo(generator, header, true);
  check_editor_bubble_button_visible(true);

  // Move mouse outside the header again.
  MoveMouseTo(generator, header, false);
  check_editor_bubble_button_visible(false);
}

TEST_F(TabGroupHeaderViewTest, FocusModeVisibility) {
  MockDelegate delegate;
  ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(true));

  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  widget->Show();

  // In focus mode, the 3-dot menu should always be visible and collapse button
  // hidden.
  EXPECT_TRUE(header->editor_bubble_button()->GetVisible());
  EXPECT_FALSE(header->collapse_icon_for_testing()->GetVisible());

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());

  // Even when hovering over the header, 3-dot button is visible and collapse
  // button is hidden.
  MoveMouseTo(generator, header, true);
  EXPECT_TRUE(header->editor_bubble_button()->GetVisible());
  EXPECT_FALSE(header->collapse_icon_for_testing()->GetVisible());

  // Move mouse outside the header.
  MoveMouseTo(generator, header, false);
  EXPECT_TRUE(header->editor_bubble_button()->GetVisible());
  EXPECT_FALSE(header->collapse_icon_for_testing()->GetVisible());

  // In the collapsed tab strip, the 3-dot button should always be visible.
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);
  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.is_sharing_group = true;
  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

  header->SetBounds(0, 0,
                    GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                    GetLayoutConstant(LayoutConstant::kVerticalTabHeight));
  header->DeprecatedLayoutImmediately();
  EXPECT_TRUE(header->editor_bubble_button()->GetVisible());
  EXPECT_GT(header->editor_bubble_button()->width(), 0);
  EXPECT_EQ(header->editor_bubble_button()->bounds().CenterPoint().x(),
            header->GetLocalBounds().CenterPoint().x());
  EXPECT_FALSE(header->collapse_icon_for_testing()->GetVisible());
}

TEST_F(TabGroupHeaderViewTest, LeftClickInFocusModeDoesNotToggleCollapse) {
  MockDelegate delegate;
  ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(true));
  EXPECT_CALL(delegate, ToggleCollapsedState(testing::_)).Times(0);

  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  widget->Show();

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  MoveMouseTo(generator, header, true);
  generator.ClickLeftButton();
}

TEST_F(TabGroupHeaderViewTest, OnKeyPress_ToggleCollapse_SpaceAndReturn) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data);

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(delegate, ToggleCollapsedState(
                            ToggleTabGroupCollapsedStateOrigin::kKeyboard))
      .Times(2);

  ui::KeyEvent space_event(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  EXPECT_TRUE(header->OnKeyPressed(space_event));

  ui::KeyEvent return_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                            ui::EF_NONE);
  EXPECT_TRUE(header->OnKeyPressed(return_event));
}

TEST_F(TabGroupHeaderViewTest,
       OnKeyPress_EditorBubbleOpen_DoesNotToggleCollapse) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(delegate, ToggleCollapsedState(
                            ToggleTabGroupCollapsedStateOrigin::kKeyboard))
      .Times(0);

  std::unique_ptr<views::Widget> bubble_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  header->editor_bubble_tracker_for_testing()->Opened(std::move(bubble_widget));

  ui::KeyEvent space_event(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  EXPECT_FALSE(header->OnKeyPressed(space_event));

  ui::KeyEvent return_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                            ui::EF_NONE);
  EXPECT_FALSE(header->OnKeyPressed(return_event));
}

TEST_F(TabGroupHeaderViewTest, OnKeyPress_Vertical_ShiftPreviousAndNext) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  ui::KeyEvent up_event(ui::EventType::kKeyPressed, ui::VKEY_UP,
                        GetPlatformDependentAccelerator());
  EXPECT_CALL(delegate, ShiftGroupPrevious()).Times(1);
  EXPECT_TRUE(header->OnKeyPressed(up_event));

  ui::KeyEvent down_event(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                          GetPlatformDependentAccelerator());
  EXPECT_CALL(delegate, ShiftGroupNext()).Times(1);
  EXPECT_TRUE(header->OnKeyPressed(down_event));
}

TEST_F(TabGroupHeaderViewTest, OnKeyPress_Horizontal_ShiftPreviousAndNext) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data);

  ui::KeyEvent left_event(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                          GetPlatformDependentAccelerator());
  EXPECT_CALL(delegate, ShiftGroupPrevious()).Times(1);
  EXPECT_TRUE(header->OnKeyPressed(left_event));

  ui::KeyEvent right_event(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           GetPlatformDependentAccelerator());
  EXPECT_CALL(delegate, ShiftGroupNext()).Times(1);
  EXPECT_TRUE(header->OnKeyPressed(right_event));
}

TEST_F(TabGroupHeaderViewTest,
       ShowContextMenuWhenBubbleAlreadyOpenReturnsEarly) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  widget->Show();

  std::unique_ptr<views::Widget> bubble_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  header->editor_bubble_tracker_for_testing()->Opened(std::move(bubble_widget));

  EXPECT_CALL(delegate, ShowGroupEditorBubble(testing::_)).Times(0);

  header->ShowContextMenuForViewImpl(header, gfx::Point(),
                                     ui::mojom::MenuSourceType::kNone);
}

TEST_F(TabGroupHeaderViewTest, HoverCardAccessibilityText_OneTab) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 1;

  tabs::TabGroupTabData tab;
  tab.title = u"Tab 1";
  tab.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab);

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

#if BUILDFLAG(IS_MAC)
  std::u16string expected_acc_text =
      u" group Group Title - 1 Tab, \u2022  Tab 1 - Expanded";
#else
  std::u16string expected_acc_text =
      u" group Group Title - 1 tab, \u2022  Tab 1 - Expanded";
#endif

  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, AccessibleNameWhenGroupFocused) {
  MockDelegate delegate;
  ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(true));
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);
  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 1;
  tabs::TabGroupTabData tab;
  tab.title = u"Tab 1";
  tab.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab);
  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

  // "Focused on" prefix, with the base label's leading space trimmed.
#if BUILDFLAG(IS_MAC)
  const std::u16string expected_acc_text =
      u"Focused on group Group Title - 1 Tab, \u2022  Tab 1 - Expanded";
#else
  const std::u16string expected_acc_text =
      u"Focused on group Group Title - 1 tab, \u2022  Tab 1 - Expanded";
#endif
  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, AccessibleNameForEphemeralGroup) {
  MockDelegate delegate;
  ON_CALL(delegate, IsValid()).WillByDefault(testing::Return(true));
  ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(true));
  tab_groups::TabGroupVisualData visual_data(
      u"", tab_groups::TabGroupColorId::kGrey, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);
  mock_tab_group.SetIsEphemeral(true);
  mock_tab_group.AddTab();
  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 1;
  tabs::TabGroupTabData tab;
  tab.title = u"Tab 1";
  tab.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab);
  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

  // Ephemeral group: describes the tabs, with a custom role description.
#if BUILDFLAG(IS_MAC)
  const std::u16string expected_acc_text =
      u"Focused on 1 Tab, \u2022  Tab 1 - Expanded";
#else
  const std::u16string expected_acc_text =
      u"Focused on 1 tab, \u2022  Tab 1 - Expanded";
#endif
  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
  EXPECT_EQ(
      header->GetViewAccessibility().GetRoleDescription(),
      l10n_util::GetStringUTF16(IDS_GROUP_AX_ROLE_DESCRIPTION_EPHEMERAL_GROUP));

  // Converting to a regular group drops the role description.
  mock_tab_group.SetIsEphemeral(false);
  header->OnDataChanged(data);
  EXPECT_TRUE(header->GetViewAccessibility().GetRoleDescription().empty());
}

TEST_F(TabGroupHeaderViewTest, HoverCardAccessibilityText_FiveTabs) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 5;

  for (int i = 0; i < 5; ++i) {
    tabs::TabGroupTabData tab;
    tab.title = u"Tab " + base::NumberToString16(i + 1);
    tab.last_committed_url = GURL("https://google.com");
    data.tab_data.push_back(tab);
  }

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

#if BUILDFLAG(IS_MAC)
  std::u16string expected_acc_text =
      u" group Group Title - 5 Tabs, \u2022  Tab 1, \u2022  Tab 2, \u2022  Tab "
      u"3, "
      u"\u2022  Tab 4, \u2022  Tab 5 - Expanded";
#else
  std::u16string expected_acc_text =
      u" group Group Title - 5 tabs, \u2022  Tab 1, \u2022  Tab 2, \u2022  Tab "
      u"3, "
      u"\u2022  Tab 4, \u2022  Tab 5 - Expanded";
#endif

  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, HoverCardAccessibilityText_ExcessTabs) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 6;

  for (int i = 0; i < 5; ++i) {
    tabs::TabGroupTabData tab;
    tab.title = u"Tab " + base::NumberToString16(i + 1);
    tab.last_committed_url = GURL("https://google.com");
    data.tab_data.push_back(tab);
  }

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

#if BUILDFLAG(IS_MAC)
  std::u16string expected_acc_text =
      u" group Group Title - 6 Tabs, \u2022  Tab 1, \u2022  Tab 2, \u2022  Tab "
      u"3, "
      u"\u2022  Tab 4, \u2022  Tab 5, + 1 More - Expanded";
#else
  std::u16string expected_acc_text =
      u" group Group Title - 6 tabs, \u2022  Tab 1, \u2022  Tab 2, \u2022  Tab "
      u"3, "
      u"\u2022  Tab 4, \u2022  Tab 5, + 1 more - Expanded";
#endif

  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, HoverCardAccessibilityText_UnnamedGroup) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tab_groups::TabGroupVisualData unnamed_visual_data(
      u"", tab_groups::TabGroupColorId::kRed, false);

  tabs::TabGroupData data;
  data.visual_data = unnamed_visual_data;
  data.num_tabs_in_group = 2;

  tabs::TabGroupTabData tab1;
  tab1.title = u"Tab 1";
  tab1.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab1);

  tabs::TabGroupTabData tab2;
  tab2.title = u"Tab 2";
  tab2.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab2);

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

#if BUILDFLAG(IS_MAC)
  std::u16string expected_acc_text =
      u" unnamed group - 2 Tabs, \u2022  Tab 1, \u2022  Tab 2 - Expanded";
#else
  std::u16string expected_acc_text =
      u" unnamed group - 2 tabs, \u2022  Tab 1, \u2022  Tab 2 - Expanded";
#endif

  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, HoverCardAccessibilityText_LongTabTitleElided) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 1;

  tabs::TabGroupTabData tab;
  // 100 characters title
  tab.title =
      u"Very Long Tab Title "
      u"01234567890123456789012345678901234567890123456789012345678"
      u"901234567890123456789";
  tab.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab);

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

#if BUILDFLAG(IS_MAC)
  std::u16string expected_acc_text =
      u" group Group Title - 1 Tab, \u2022  Very Long Tab Title "
      u"01234567890123456789012345\u2026 - Expanded";
#else
  std::u16string expected_acc_text =
      u" group Group Title - 1 tab, \u2022  Very Long Tab Title "
      u"01234567890123456789012345\u2026 - Expanded";
#endif

  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, HoverCardAccessibilityText_SharedGroup) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 1;
  data.is_sharing_group = true;

  tabs::TabGroupTabData tab;
  tab.title = u"Tab 1";
  tab.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab);

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

#if BUILDFLAG(IS_MAC)
  std::u16string expected_acc_text =
      u"Shared group Group Title - 1 Tab, \u2022  Tab 1 - Expanded";
#else
  std::u16string expected_acc_text =
      u"Shared group Group Title - 1 tab, \u2022  Tab 1 - Expanded";
#endif

  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, HoverCardAccessibilityText_CollapsedGroup) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, true);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  data.num_tabs_in_group = 1;

  tabs::TabGroupTabData tab;
  tab.title = u"Tab 1";
  tab.last_committed_url = GURL("https://google.com");
  data.tab_data.push_back(tab);

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));
  header->OnDataChanged(data);

#if BUILDFLAG(IS_MAC)
  std::u16string expected_acc_text =
      u" group Group Title - 1 Tab, \u2022  Tab 1 - Collapsed";
#else
  std::u16string expected_acc_text =
      u" group Group Title - 1 tab, \u2022  Tab 1 - Collapsed";
#endif

  EXPECT_EQ(header->GetViewAccessibility().GetCachedName(), expected_acc_text);
}

TEST_F(TabGroupHeaderViewTest, HorizontalPreferredSize_NamedGroup) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  header->OnDataChanged(data);

  const gfx::Size preferred_size = header->CalculatePreferredSize({});
  EXPECT_EQ(preferred_size.height(), TabGroupStyle::GetEmptyChipSize());
  EXPECT_GE(preferred_size.width(), TabGroupStyle::GetEmptyChipSize());
}

TEST_F(TabGroupHeaderViewTest, HorizontalPreferredSize_UnnamedGroup) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"", tab_groups::TabGroupColorId::kBlue, false);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);

  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  tabs::TabGroupData data;
  data.visual_data = visual_data;

  EXPECT_CALL(delegate, GetTabGroupData())
      .WillRepeatedly(testing::ReturnRef(data));

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  header->OnDataChanged(data);

  const gfx::Size preferred_size = header->CalculatePreferredSize({});
  // An unnamed group header should match legacy tab strip empty chip size
  // (20x20).
  EXPECT_EQ(preferred_size, gfx::Size(TabGroupStyle::GetEmptyChipSize(),
                                      TabGroupStyle::GetEmptyChipSize()));
}

TEST_F(TabGroupHeaderViewTest, AnchorBounds_HorizontalWithParent) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 800, 600));

  // Simulate TabGroupView as parent container with the full tabstrip height.
  auto* container = widget->SetContentsView(std::make_unique<views::View>());
  container->SetBounds(10, 50, 300, 34);

  // Add the horizontal group header as a child, sized as a chip.
  auto* header = container->AddChildView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  header->SetBounds(0, 7, 80, 20);

  gfx::Rect anchor_bounds = header->GetAnchorBoundsInScreen();
  gfx::Rect header_screen_bounds = header->GetBoundsInScreen();
  gfx::Rect container_screen_bounds = container->GetBoundsInScreen();

  EXPECT_EQ(anchor_bounds.x(), header_screen_bounds.x());
  EXPECT_EQ(anchor_bounds.width(), header_screen_bounds.width());
  EXPECT_EQ(anchor_bounds.y(), container_screen_bounds.y());
  EXPECT_EQ(anchor_bounds.height(), container_screen_bounds.height());
  EXPECT_EQ(anchor_bounds.bottom(), container_screen_bounds.bottom());
}

TEST_F(TabGroupHeaderViewTest, AnchorBounds_VerticalWithParent) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 800, 600));

  auto* container = widget->SetContentsView(std::make_unique<views::View>());
  container->SetBounds(10, 50, 200, 150);

  auto* header = container->AddChildView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  header->SetBounds(8, 4, 180, 26);

  gfx::Rect anchor_bounds = header->GetAnchorBoundsInScreen();
  EXPECT_EQ(anchor_bounds, header->GetBoundsInScreen());
}

TEST_F(TabGroupHeaderViewTest, AnchorPosition_Orientation) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, false);

  auto horizontal_header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data);
  EXPECT_EQ(horizontal_header->GetAnchorPosition(),
            views::BubbleBorder::Arrow::TOP_LEFT);

  auto vertical_header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);
  EXPECT_EQ(vertical_header->GetAnchorPosition(),
            views::BubbleBorder::Arrow::LEFT_TOP);
}

TEST_F(TabGroupHeaderViewTest, HideHoverCardOnBlurWhenUnfocusButtonFocused) {
  MockDelegate delegate;
  ON_CALL(delegate, IsFocusInTabStrip()).WillByDefault(testing::Return(true));

  tab_groups::TabGroupVisualData visual_data(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container = widget->SetContentsView(std::make_unique<views::View>());
  auto* header = container->AddChildView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  auto* other_view = container->AddChildView(std::make_unique<views::View>());
  other_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->Show();

  views::FocusManager* focus_manager = widget->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);
  focus_manager->SetKeyboardAccessible(true);

  EXPECT_CALL(delegate,
              UpdateHoverCard(TabSlotController::HoverCardUpdateType::kFocus))
      .Times(1);
  focus_manager->SetFocusedView(header);

  // Blurring to another hovercard target in the tab strip does not hide the
  // hovercard.
  ON_CALL(delegate, IsUnfocusButtonFocused())
      .WillByDefault(testing::Return(false));
  EXPECT_CALL(delegate, HideHoverCard(testing::_)).Times(0);
  focus_manager->SetFocusedView(other_view);

  EXPECT_CALL(delegate,
              UpdateHoverCard(TabSlotController::HoverCardUpdateType::kFocus))
      .Times(1);
  focus_manager->SetFocusedView(header);

  // Blurring to the unfocus button hides the hovercard even though focus is
  // still in the tab strip.
  ON_CALL(delegate, IsUnfocusButtonFocused())
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(delegate,
              HideHoverCard(TabSlotController::HoverCardUpdateType::kFocus))
      .Times(1);
  focus_manager->SetFocusedView(other_view);
}

TEST_F(TabGroupHeaderViewTest, HideHoverCardOnMouseExitInFocusMode) {
  MockDelegate delegate;
  ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(true));

  tab_groups::TabGroupVisualData visual_data(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  header->SetBounds(0, 0, 100, 24);
  widget->Show();

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_CALL(delegate,
              UpdateHoverCard(TabSlotController::HoverCardUpdateType::kHover))
      .Times(1);
  MoveMouseTo(generator, header, true);

  EXPECT_CALL(delegate,
              HideHoverCard(TabSlotController::HoverCardUpdateType::kHover))
      .Times(1);
  MoveMouseTo(generator, header, false);
}

TEST_F(TabGroupHeaderViewTest, AcquiresHoverCardHideLockWhenEditorBubbleOpens) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, AcquireExpandOnHoverLock()).WillOnce([]() {
    return std::make_unique<ExpandOnHoverLock>();
  });
  EXPECT_CALL(delegate, AcquireHoverCardHideLock()).Times(1);
  EXPECT_CALL(delegate, ShowGroupEditorBubble(testing::_))
      .WillOnce([this](bool) {
        return CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
      });

  header->ShowContextMenuForViewImpl(header, gfx::Point(),
                                     ui::mojom::MenuSourceType::kNone);
}

TEST_F(TabGroupHeaderViewTest, UnfocusButtonCollapsedState) {
  MockDelegate delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* button =
      widget->SetContentsView(std::make_unique<TabGroupUnfocusButton>(
          delegate, TabStripOrientation::kVertical));

  const std::u16string expected_text =
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON);
  const std::u16string expected_ax_name =
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON_AX_LABEL);
  const int collapsed_width =
      tabs::kVerticalTabStripCollapsedWidth -
      2 * GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

  // Expanded width: shows text and no icon.
  button->SetBounds(0, 0, collapsed_width + 20, 24);
  EXPECT_EQ(button->GetText(), expected_text);
  EXPECT_FALSE(button->GetImageModel(views::Button::STATE_NORMAL).has_value());
  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), expected_ax_name);

  // Collapsed width: clears text, shows icon, and preserves accessible name.
  button->SetBounds(0, 0, collapsed_width, 24);
  EXPECT_TRUE(button->GetText().empty());
  ASSERT_TRUE(button->GetImageModel(views::Button::STATE_NORMAL).has_value());
  EXPECT_FALSE(button->GetImageModel(views::Button::STATE_NORMAL)->IsEmpty());
  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), expected_ax_name);

  // Expanding again restores text and clears icon.
  button->SetBounds(0, 0, collapsed_width + 20, 24);
  EXPECT_EQ(button->GetText(), expected_text);
  ASSERT_TRUE(button->GetImageModel(views::Button::STATE_NORMAL).has_value());
  EXPECT_TRUE(button->GetImageModel(views::Button::STATE_NORMAL)->IsEmpty());
  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), expected_ax_name);
}

TEST_F(TabGroupHeaderViewTest, UnfocusButtonAccessibleName) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"", tab_groups::TabGroupColorId::kGrey, false);
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);
  mock_tab_group.AddTab();
  EXPECT_CALL(delegate, GetTabGroup())
      .WillRepeatedly(testing::ReturnRef(mock_tab_group));

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* button =
      widget->SetContentsView(std::make_unique<TabGroupUnfocusButton>(
          delegate, TabStripOrientation::kHorizontal));
  const std::u16string group_name =
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON_AX_LABEL);

  // Regular group: "Unfocus tab group".
  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), group_name);

  // Ephemeral group: "Unfocus tab" / "Unfocus tabs".
  mock_tab_group.SetIsEphemeral(true);
  button->UpdateAccessibleName();
  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON_EPHEMERAL_AX_LABEL, 1));
  mock_tab_group.AddTab();
  mock_tab_group.AddTab();
  button->UpdateAccessibleName();
  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON_EPHEMERAL_AX_LABEL, 3));

  // The visible text never changes.
  EXPECT_EQ(button->GetText(),
            l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON));

  mock_tab_group.SetIsEphemeral(false);
  button->UpdateAccessibleName();
  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), group_name);
}

// The interpolated focus-animation insets match the resting insets exactly at
// both ends, so the header does not jump when the animation starts or ends.
TEST_F(TabGroupHeaderViewTest, FocusAnimationInsetsMatchRestingInsets) {
  MockDelegate delegate;
  ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(false));
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);
  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data);
  const gfx::Insets unfocused_insets = header->GetInteriorMargin();

  header->SetFocusAnimationState({.progress = 1.0f});
  EXPECT_EQ(header->GetInteriorMargin(),
            TabGroupStyle::GetInsetsForFocusedHeaderChip());

  header->SetFocusAnimationState({.progress = 0.0f});
  EXPECT_EQ(header->GetInteriorMargin(), unfocused_insets);

  // Midway, each edge lies strictly between the two resting values (or equals
  // them when they coincide), so the insets really interpolate.
  header->SetFocusAnimationState({.progress = 0.5f});
  const gfx::Insets focused_insets =
      TabGroupStyle::GetInsetsForFocusedHeaderChip();
  const gfx::Insets mid_insets = header->GetInteriorMargin();
  const auto between = [](int value, int a, int b) {
    return value >= std::min(a, b) && value <= std::max(a, b) &&
           (a == b || (value != a && value != b));
  };
  EXPECT_TRUE(between(mid_insets.left(), unfocused_insets.left(),
                      focused_insets.left()));
  EXPECT_TRUE(between(mid_insets.right(), unfocused_insets.right(),
                      focused_insets.right()));
  EXPECT_TRUE(
      between(mid_insets.top(), unfocused_insets.top(), focused_insets.top()));
  EXPECT_TRUE(between(mid_insets.bottom(), unfocused_insets.bottom(),
                      focused_insets.bottom()));
}

// Vertical headers do not change insets on focus, so the focus animation must
// leave them alone at every progress value.
TEST_F(TabGroupHeaderViewTest, FocusAnimationLeavesVerticalInsetsUnchanged) {
  MockDelegate delegate;
  ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(false));
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);
  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);
  const gfx::Insets resting_insets = header->GetInteriorMargin();

  for (float progress : {0.0f, 0.5f, 1.0f}) {
    header->SetFocusAnimationState({.progress = progress});
    EXPECT_EQ(header->GetInteriorMargin(), resting_insets) << progress;
  }
}

// The focus animation only changes the interior margin, never the preferred
// size, so the animating layout target stays stable across frames.
TEST_F(TabGroupHeaderViewTest, FocusAnimationDoesNotChangePreferredSize) {
  for (TabStripOrientation orientation :
       {TabStripOrientation::kHorizontal, TabStripOrientation::kVertical}) {
    MockDelegate delegate;
    ON_CALL(delegate, IsGroupFocused()).WillByDefault(testing::Return(false));
    tab_groups::TabGroupVisualData visual_data(
        u"Group Title", tab_groups::TabGroupColorId::kBlue, false);
    auto header = std::make_unique<TabGroupHeaderView>(delegate, orientation,
                                                       nullptr, &visual_data);
    const gfx::Size resting_size = header->GetPreferredSize();

    for (float progress : {0.0f, 0.5f, 1.0f}) {
      header->SetFocusAnimationState({.progress = progress});
      EXPECT_EQ(header->GetPreferredSize(), resting_size)
          << static_cast<int>(orientation) << " " << progress;
    }
    header->SetFocusAnimationState({});
    EXPECT_EQ(header->GetPreferredSize(), resting_size)
        << static_cast<int>(orientation);
  }
}

class TabGroupHeaderViewFocusRadiiTest
    : public TabGroupHeaderViewTest,
      public testing::WithParamInterface<
          std::tuple<TabStripOrientation, bool>> {
 public:
  TabStripOrientation orientation() const { return std::get<0>(GetParam()); }
  bool is_rtl() const { return std::get<1>(GetParam()); }

  void SetUp() override {
    TabGroupHeaderViewTest::SetUp();
    scoped_rtl_.emplace(is_rtl());
  }
  void TearDown() override {
    scoped_rtl_.reset();
    TabGroupHeaderViewTest::TearDown();
  }

  // Returns the header's background corner radii after
  // `SetFocusAnimationState()` for a header whose delegate reports
  // `is_focused`.
  gfx::RoundedCornersF GetRadii(bool is_focused,
                                std::optional<float> progress) {
    MockDelegate delegate;
    ON_CALL(delegate, IsGroupFocused())
        .WillByDefault(testing::Return(is_focused));
    tab_groups::TabGroupVisualData visual_data(
        u"Group Title", tab_groups::TabGroupColorId::kBlue, false);
    std::unique_ptr<views::Widget> widget =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
        delegate, orientation(), nullptr, &visual_data));
    // Force a background rebuild via a distinct intermediate value so the
    // early-return in `SetFocusAnimationState()` does not skip the update.
    header->SetFocusAnimationState({.progress = 0.5f});
    header->SetFocusAnimationState({.progress = progress});
    return header->GetBackground()->GetRoundedCornerRadii().value();
  }

 private:
  std::optional<base::i18n::ScopedRTLForTesting> scoped_rtl_;
};

// At the ends of the focus animation the interpolated corner radii equal the
// resting unfocused and focused radii, so the chip shape does not jump when
// the animation starts or ends.
TEST_P(TabGroupHeaderViewFocusRadiiTest, EndpointsMatchRestingRadii) {
  EXPECT_EQ(GetRadii(/*is_focused=*/false, 0.0f),
            GetRadii(/*is_focused=*/false, std::nullopt));
  EXPECT_EQ(GetRadii(/*is_focused=*/true, 1.0f),
            GetRadii(/*is_focused=*/true, std::nullopt));
  // Progress overrides the delegate's focus state.
  EXPECT_EQ(GetRadii(/*is_focused=*/true, 0.0f),
            GetRadii(/*is_focused=*/false, std::nullopt));
  EXPECT_EQ(GetRadii(/*is_focused=*/false, 1.0f),
            GetRadii(/*is_focused=*/true, std::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabGroupHeaderViewFocusRadiiTest,
    testing::Combine(testing::Values(TabStripOrientation::kHorizontal,
                                     TabStripOrientation::kVertical),
                     testing::Bool()),
    [](const testing::TestParamInfo<
        TabGroupHeaderViewFocusRadiiTest::ParamType>& info) {
      return base::StrCat(
          {std::get<0>(info.param) == TabStripOrientation::kHorizontal
               ? "Horizontal"
               : "Vertical",
           std::get<1>(info.param) ? "RTL" : "LTR"});
    });

// At the end of the focus animation the Unfocus button's interpolated corner
// radii equal its resting radii, and the label alpha follows `label_opacity`.
TEST_F(TabGroupHeaderViewTest, UnfocusButtonFocusAnimationState) {
  MockDelegate delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* button =
      widget->SetContentsView(std::make_unique<TabGroupUnfocusButton>(
          delegate, TabStripOrientation::kHorizontal));

  button->SetFocusAnimationState({});
  const gfx::RoundedCornersF resting_radii =
      button->GetBackground()->GetRoundedCornerRadii().value();
  const SkColor resting_text_color = button->GetCurrentTextColor();
  ASSERT_EQ(SkColorGetA(resting_text_color), SK_AlphaOPAQUE);

  button->SetFocusAnimationState({.progress = 1.0f, .content_opacity = 1.0f});
  EXPECT_EQ(button->GetBackground()->GetRoundedCornerRadii().value(),
            resting_radii);

  button->SetFocusAnimationState({.progress = 0.5f, .content_opacity = 0.0f});
  EXPECT_NE(button->GetBackground()->GetRoundedCornerRadii().value(),
            resting_radii);
  EXPECT_EQ(SkColorGetA(button->GetCurrentTextColor()), 0u);

  button->SetFocusAnimationState({.progress = 0.5f, .content_opacity = 0.5f});
  EXPECT_EQ(SkColorGetA(button->GetCurrentTextColor()), 128u);

  button->SetFocusAnimationState({});
  EXPECT_EQ(button->GetBackground()->GetRoundedCornerRadii().value(),
            resting_radii);
  EXPECT_EQ(button->GetCurrentTextColor(), resting_text_color);
}

// The collapsed-state icon honours the label opacity regardless of whether the
// collapse happens before or after the opacity is set.
TEST_F(TabGroupHeaderViewTest, UnfocusButtonCollapsedIconHonoursLabelOpacity) {
  MockDelegate delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* button =
      widget->SetContentsView(std::make_unique<TabGroupUnfocusButton>(
          delegate, TabStripOrientation::kVertical));
  const int collapsed_width =
      tabs::kVerticalTabStripCollapsedWidth -
      2 * GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

  // Opacity set first, then collapse.
  button->SetBounds(0, 0, collapsed_width + 20, 24);
  button->SetFocusAnimationState({.progress = 0.5f, .content_opacity = 0.0f});
  button->SetBounds(0, 0, collapsed_width, 24);
  ASSERT_TRUE(button->GetImageModel(views::Button::STATE_NORMAL).has_value());
  EXPECT_EQ(SkColorGetA(button->GetImageModel(views::Button::STATE_NORMAL)
                            ->GetVectorIcon()
                            .color()
                            .ResolveToSkColor(button->GetColorProvider())),
            0u);

  // Collapse first, then opacity.
  button->SetFocusAnimationState({});
  EXPECT_EQ(SkColorGetA(button->GetImageModel(views::Button::STATE_NORMAL)
                            ->GetVectorIcon()
                            .color()
                            .ResolveToSkColor(button->GetColorProvider())),
            SK_AlphaOPAQUE);
  button->SetFocusAnimationState({.progress = 0.5f, .content_opacity = 0.0f});
  EXPECT_EQ(SkColorGetA(button->GetImageModel(views::Button::STATE_NORMAL)
                            ->GetVectorIcon()
                            .color()
                            .ResolveToSkColor(button->GetColorProvider())),
            0u);
}

#if BUILDFLAG(IS_MAC)
TEST_F(TabGroupHeaderViewTest, SingleEmojiHeaderTitleIsVisuallyCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"\U0001F60A");
  EXPECT_EQ(CenteredTitleX(header) + 1, header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, SingleEmojiHeaderTitleIsVisuallyCenteredInRtl) {
  base::i18n::ScopedRTLForTesting scoped_rtl(true);
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"\U0001F60A");
  EXPECT_EQ(CenteredTitleX(header) - 1, header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, SkinToneEmojiHeaderTitleIsVisuallyCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"\U0001F44D\U0001F3FD");
  EXPECT_EQ(CenteredTitleX(header) + 1, header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, ZwjEmojiHeaderTitleIsVisuallyCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(
      header, delegate,
      u"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  EXPECT_EQ(CenteredTitleX(header) + 1, header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, FlagEmojiHeaderTitleIsVisuallyCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"\U0001F1FA\U0001F1F8");
  EXPECT_EQ(CenteredTitleX(header) + 1, header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, EmojiVariationSelectorTitleIsCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"\u263A\uFE0F");
  EXPECT_EQ(CenteredTitleX(header), header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, TextLikeVs16SymbolTitleIsCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"\u2666\uFE0F");
  EXPECT_EQ(CenteredTitleX(header), header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, MultiEmojiHeaderTitleIsCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"\U0001F60A\U0001F60A");
  EXPECT_EQ(CenteredTitleX(header), header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest,
       FocusedSingleEmojiHeaderTitleIsVisuallyCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused()).WillRepeatedly(testing::Return(true));

  SetGroupTitleAndLayout(header, delegate, u"\U0001F60A");
  EXPECT_EQ(TabGroupStyle::GetInsetsForFocusedHeaderChip().left() + 1,
            header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest,
       FocusedSingleEmojiHeaderTitleIsVisuallyCenteredInRtl) {
  base::i18n::ScopedRTLForTesting scoped_rtl(true);
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused()).WillRepeatedly(testing::Return(true));

  SetGroupTitleAndLayout(header, delegate, u"\U0001F60A");
  EXPECT_EQ(TabGroupStyle::GetInsetsForFocusedHeaderChip().left() - 1,
            header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest,
       FocusedSingleEmojiFocusAnimationInsetsMatchRestingInsets) {
  MockDelegate delegate;
  bool is_focused = false;
  ON_CALL(delegate, IsGroupFocused()).WillByDefault([&is_focused]() {
    return is_focused;
  });

  tab_groups::TabGroupVisualData visual_data(
      u"\U0001F60A", tab_groups::TabGroupColorId::kBlue, false);
  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data);

  const gfx::Insets unfocused_insets = header->GetInteriorMargin();
  is_focused = true;
  header->OnDataChanged(delegate.default_tab_group_data_);
  const gfx::Insets focused_insets = header->GetInteriorMargin();

  header->SetFocusAnimationState({.progress = 0.0f});
  EXPECT_EQ(header->GetInteriorMargin(), unfocused_insets);

  header->SetFocusAnimationState({.progress = 1.0f});
  EXPECT_EQ(header->GetInteriorMargin(), focused_insets);
}
#endif

// For ephemeral groups, `content_opacity` scales the alpha of both the title
// label and the editor bubble button icon, while persistent group headers stay
// opaque.
TEST_F(TabGroupHeaderViewTest, EphemeralHeaderFocusContentOpacity) {
  const tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);
  tabs::MockTabGroup mock_tab_group(nullptr, group_id, visual_data);
  mock_tab_group.SetIsEphemeral(true);

  tabs::TabGroupData data;
  data.visual_data = visual_data;

  MockDelegate delegate;
  ON_CALL(delegate, IsValid()).WillByDefault(testing::Return(true));
  ON_CALL(delegate, GetTabGroup())
      .WillByDefault(testing::ReturnRef(mock_tab_group));
  ON_CALL(delegate, GetTabGroupData()).WillByDefault(testing::ReturnRef(data));

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  header->OnDataChanged(data);

  const auto get_editor_icon_alpha = [&]() {
    return SkColorGetA(header->editor_bubble_button()
                           ->GetImageModel(views::Button::STATE_NORMAL)
                           ->GetVectorIcon()
                           .color()
                           .ResolveToSkColor(header->GetColorProvider()));
  };

  header->SetFocusAnimationState({.progress = 1.0f, .content_opacity = 1.0f});
  EXPECT_EQ(SkColorGetA(header->title_label_for_testing()->GetEnabledColor()),
            SK_AlphaOPAQUE);
  EXPECT_EQ(get_editor_icon_alpha(), SK_AlphaOPAQUE);

  header->SetFocusAnimationState({.progress = 0.5f, .content_opacity = 0.0f});
  EXPECT_EQ(SkColorGetA(header->title_label_for_testing()->GetEnabledColor()),
            0u);
  EXPECT_EQ(get_editor_icon_alpha(), 0u);

  header->SetFocusAnimationState({.progress = 0.5f, .content_opacity = 0.5f});
  EXPECT_EQ(SkColorGetA(header->title_label_for_testing()->GetEnabledColor()),
            128u);
  EXPECT_EQ(get_editor_icon_alpha(), 128u);

  // Persistent group headers ignore `content_opacity`.
  mock_tab_group.SetIsEphemeral(false);
  header->OnDataChanged(data);
  header->SetFocusAnimationState({.progress = 0.5f, .content_opacity = 0.0f});
  EXPECT_EQ(SkColorGetA(header->title_label_for_testing()->GetEnabledColor()),
            SK_AlphaOPAQUE);
  EXPECT_EQ(get_editor_icon_alpha(), SK_AlphaOPAQUE);
}

TEST_F(TabGroupHeaderViewTest, StandardHeaderTitleIsCentered) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Initial", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, IsGroupFocused())
      .WillRepeatedly(testing::Return(false));

  SetGroupTitleAndLayout(header, delegate, u"a");
  EXPECT_EQ(CenteredTitleX(header), header->title_label_for_testing()->x());
}

TEST_F(TabGroupHeaderViewTest, HeaderAccessibilityProperties) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(u"Group Title",
                                             tab_groups::TabGroupColorId::kBlue,
                                             /*is_collapsed=*/false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kHorizontal, nullptr, &visual_data);

  tabs::TabGroupData data;
  data.visual_data = visual_data;
  delegate.default_tab_group_data_ = data;
  header->OnDataChanged(data);

  ui::AXNodeData node_data;
  header->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kTabList);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kEditable));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Now collapse the group and verify accessibility state updates.
  tab_groups::TabGroupVisualData collapsed_visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue,
      /*is_collapsed=*/true);
  tabs::TabGroupData collapsed_data;
  collapsed_data.visual_data = collapsed_visual_data;
  delegate.default_tab_group_data_ = collapsed_data;
  header->OnDataChanged(collapsed_data);

  ui::AXNodeData collapsed_node_data;
  header->GetViewAccessibility().GetAccessibleNodeData(&collapsed_node_data);
  EXPECT_EQ(collapsed_node_data.role, ax::mojom::Role::kTabList);
  EXPECT_TRUE(collapsed_node_data.HasState(ax::mojom::State::kEditable));
  EXPECT_TRUE(collapsed_node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(collapsed_node_data.HasState(ax::mojom::State::kExpanded));
}
