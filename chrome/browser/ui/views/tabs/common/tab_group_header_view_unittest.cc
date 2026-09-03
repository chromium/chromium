// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"

#include <memory>
#include <string>

#include "base/i18n/message_formatter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/mock_tab_group.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

class MockDelegate : public TabGroupHeaderView::Delegate {
 public:
  MockDelegate() {
    ON_CALL(*this, GetTabClosingHelper).WillByDefault(testing::Return(nullptr));
  }
  MOCK_METHOD(void,
              ToggleCollapsedState,
              (ToggleTabGroupCollapsedStateOrigin),
              (override));
  MOCK_METHOD(std::unique_ptr<views::Widget>,
              ShowGroupEditorBubble,
              (bool),
              (override));
  MOCK_METHOD(std::u16string, GetGroupContentString, (), (const, override));
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
  MOCK_METHOD(void, ShiftGroupUp, (), (override));
  MOCK_METHOD(void, ShiftGroupDown, (), (override));
  MOCK_METHOD(bool, IsGroupFocused, (), (const, override));
  MOCK_METHOD(HorizontalTabClosingHelper*,
              GetTabClosingHelper,
              (),
              (const, override));
};

int GetPlatformDependentAccelerator() {
#if BUILDFLAG(IS_MAC)
  return ui::EF_COMMAND_DOWN;
#else
  return ui::EF_CONTROL_DOWN;
#endif
}

}  // namespace

class TabGroupHeaderViewTest : public views::ViewsTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  TabGroupHeaderViewTest() {
    if (UseGroupHeaderHoverCards()) {
      feature_list_.InitWithFeatures({features::kTabGroupHoverCards}, {});
    } else {
      feature_list_.InitWithFeatures({}, {features::kTabGroupHoverCards});
    }
  }

  bool UseGroupHeaderHoverCards() { return GetParam(); }

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

  ~TabGroupHeaderViewTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(TabGroupHeaderViewTest, TooltipText) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  EXPECT_CALL(delegate, GetGroupContentString())
      .WillRepeatedly(testing::Return(u"3 tabs"));

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

TEST_P(TabGroupHeaderViewTest, TitleLabelHeightWhenConstrained) {
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
  EXPECT_CALL(delegate, GetGroupContentString())
      .WillRepeatedly(testing::Return(u"1 tab"));

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

TEST_P(TabGroupHeaderViewTest, ShowHoverCardOnMouseEnter) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header = widget->SetContentsView(std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data));
  widget->Show();

  if (UseGroupHeaderHoverCards()) {
    EXPECT_CALL(delegate, UpdateHoverCard(testing::_));
  } else {
    EXPECT_CALL(delegate, UpdateHoverCard(testing::_)).Times(0);
  }

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  MoveMouseTo(generator, header, true);
}

TEST_P(TabGroupHeaderViewTest, EditorBubbleButtonVisibilityOnHover) {
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

TEST_P(TabGroupHeaderViewTest, FocusModeVisibility) {
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
}

TEST_P(TabGroupHeaderViewTest, LeftClickInFocusModeDoesNotToggleCollapse) {
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

TEST_P(TabGroupHeaderViewTest, OnKeyPress_ShiftUp) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_UP,
                     GetPlatformDependentAccelerator());

  EXPECT_CALL(delegate, ShiftGroupUp()).Times(1);

  EXPECT_TRUE(header->OnKeyPressed(event));
}

TEST_P(TabGroupHeaderViewTest, OnKeyPress_ShiftDown) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  auto header = std::make_unique<TabGroupHeaderView>(
      delegate, TabStripOrientation::kVertical, nullptr, &visual_data);

  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                     GetPlatformDependentAccelerator());

  EXPECT_CALL(delegate, ShiftGroupDown()).Times(1);

  EXPECT_TRUE(header->OnKeyPressed(event));
}

TEST_P(TabGroupHeaderViewTest, HoverCardAccessibilityText_OneTab) {
  if (!UseGroupHeaderHoverCards()) {
    GTEST_SKIP();
  }

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

TEST_P(TabGroupHeaderViewTest, HoverCardAccessibilityText_FiveTabs) {
  if (!UseGroupHeaderHoverCards()) {
    GTEST_SKIP();
  }

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

TEST_P(TabGroupHeaderViewTest, HoverCardAccessibilityText_ExcessTabs) {
  if (!UseGroupHeaderHoverCards()) {
    GTEST_SKIP();
  }

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

TEST_P(TabGroupHeaderViewTest, HoverCardAccessibilityText_UnnamedGroup) {
  if (!UseGroupHeaderHoverCards()) {
    GTEST_SKIP();
  }

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

TEST_P(TabGroupHeaderViewTest, HoverCardAccessibilityText_LongTabTitleElided) {
  if (!UseGroupHeaderHoverCards()) {
    GTEST_SKIP();
  }

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

TEST_P(TabGroupHeaderViewTest, HoverCardAccessibilityText_SharedGroup) {
  if (!UseGroupHeaderHoverCards()) {
    GTEST_SKIP();
  }

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

TEST_P(TabGroupHeaderViewTest, HoverCardAccessibilityText_CollapsedGroup) {
  if (!UseGroupHeaderHoverCards()) {
    GTEST_SKIP();
  }

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

INSTANTIATE_TEST_SUITE_P(All,
                         TabGroupHeaderViewTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "HoverCardEnabled"
                                             : "HoverCardDisabled";
                         });
