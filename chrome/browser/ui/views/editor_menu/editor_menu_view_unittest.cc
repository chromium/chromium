// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"

#include <string_view>

#include "chrome/browser/ui/views/editor_menu/editor_menu_chip_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

using ::testing::SizeIs;

class MockEditorMenuViewDelegate : public EditorMenuViewDelegate {
 public:
  MockEditorMenuViewDelegate() = default;
  ~MockEditorMenuViewDelegate() override = default;

  // EditorMenuViewDelegate:
  void OnSettingsButtonPressed() override {}

  void OnChipButtonPressed(std::string_view text_query_id) override {}

  void OnTextfieldArrowButtonPressed(std::u16string_view text) override {}

  void OnPromoCardWidgetClosed(
      views::Widget::ClosedReason closed_reason) override {}

  void OnEditorMenuVisibilityChanged(bool visible) override {}
};

std::u16string_view GetChipLabel(const views::View* chip) {
  CHECK(views::IsViewClass<EditorMenuChipView>(chip));
  return views::AsViewClass<EditorMenuChipView>(chip)->GetText();
}

using EditorMenuViewTest = views::ViewsTestBase;

TEST_F(EditorMenuViewTest, CreatesChips) {
  MockEditorMenuViewDelegate delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Shorten", PresetQueryCategory::kShorten),
      PresetTextQuery("ID2", u"Elaborate", PresetQueryCategory::kElaborate)};

  EditorMenuView editor_menu_view =
      EditorMenuView(queries, gfx::Rect(200, 300, 400, 200), &delegate);

  // Chips should be in a single row.
  const auto* chips_container = editor_menu_view.chips_container_for_testing();
  ASSERT_THAT(chips_container->children(), SizeIs(1));
  const auto* chip_row = chips_container->children()[0];
  ASSERT_THAT(chip_row->children(), SizeIs(queries.size()));
  // Chips should have correct text labels.
  EXPECT_EQ(GetChipLabel(chip_row->children()[0]), queries[0].name);
  EXPECT_EQ(GetChipLabel(chip_row->children()[1]), queries[1].name);
}

}  // namespace

}  // namespace chromeos::editor_menu
