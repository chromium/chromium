// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
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
  MOCK_METHOD(void, InitHeaderDrag, (const ui::MouseEvent&), (override));
  MOCK_METHOD(bool, ContinueHeaderDrag, (const ui::MouseEvent&), (override));
  MOCK_METHOD(void, CancelHeaderDrag, (), (override));
  MOCK_METHOD(void, HideHoverCard, (), (const, override));
};

}  // namespace

class VerticalTabGroupHeaderViewTest : public views::ViewsTestBase {
 public:
  VerticalTabGroupHeaderViewTest() = default;
  ~VerticalTabGroupHeaderViewTest() override = default;
};

TEST_F(VerticalTabGroupHeaderViewTest, TooltipText) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  EXPECT_CALL(delegate, GetGroupContentString())
      .WillRepeatedly(testing::Return(u"3 tabs"));

  auto header = std::make_unique<VerticalTabGroupHeaderView>(delegate, nullptr,
                                                             &visual_data);

  // Initialize with data
  header->OnDataChanged(&visual_data, false, false);

  std::u16string expected_tooltip = l10n_util::GetStringFUTF16(
      IDS_TAB_GROUPS_NAMED_GROUP_TOOLTIP, u"Group Title", u"3 tabs");

  EXPECT_EQ(header->GetTooltipText(), expected_tooltip);

  // Test unnamed group
  tab_groups::TabGroupVisualData unnamed_visual_data(
      u"", tab_groups::TabGroupColorId::kRed, false);
  header->OnDataChanged(&unnamed_visual_data, false, false);

  expected_tooltip = l10n_util::GetStringFUTF16(
      IDS_TAB_GROUPS_UNNAMED_GROUP_TOOLTIP, u"3 tabs");

  EXPECT_EQ(header->GetTooltipText(), expected_tooltip);
}

TEST_F(VerticalTabGroupHeaderViewTest, HideHoverCardOnMouseEnter) {
  MockDelegate delegate;
  tab_groups::TabGroupVisualData visual_data(
      u"Group Title", tab_groups::TabGroupColorId::kBlue, false);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* header =
      widget->SetContentsView(std::make_unique<VerticalTabGroupHeaderView>(
          delegate, nullptr, &visual_data));
  widget->Show();

  EXPECT_CALL(delegate, HideHoverCard());

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  generator.MoveMouseTo(header->GetBoundsInScreen().CenterPoint());
}
