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
};

using EditorMenuViewTest = views::ViewsTestBase;

TEST_F(EditorMenuViewTest, CreatesChips) {
  MockEditorMenuViewDelegate delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("Query ID 1", u"Label 1", PresetQueryCategory::kUnknown),
      PresetTextQuery("Query ID 2", u"Label 2", PresetQueryCategory::kUnknown)};

  EditorMenuView editor_menu_view =
      EditorMenuView(queries, gfx::Rect(200, 300, 80, 200), &delegate);

  const auto& editor_menu_chips = editor_menu_view.chips_for_testing();
  ASSERT_THAT(editor_menu_chips, testing::SizeIs(queries.size()));
  EXPECT_EQ(editor_menu_chips[0]->GetText(), queries[0].name);
  EXPECT_EQ(editor_menu_chips[1]->GetText(), queries[1].name);
}

}  // namespace

}  // namespace chromeos::editor_menu
