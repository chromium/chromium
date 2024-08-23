// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_chip_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_textfield_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

class MockEditorMenuViewDelegate : public EditorMenuViewDelegate {
 public:
  MockEditorMenuViewDelegate() = default;
  ~MockEditorMenuViewDelegate() override = default;

  // EditorMenuViewDelegate:
  MOCK_METHOD(void, OnSettingsButtonPressed, (), (override));
  MOCK_METHOD(void,
              OnChipButtonPressed,
              (std::string_view text_query_id),
              (override));
  MOCK_METHOD(void,
              OnTextfieldArrowButtonPressed,
              (std::u16string_view text),
              (override));
  MOCK_METHOD(void,
              OnPromoCardWidgetClosed,
              (views::Widget::ClosedReason closed_reason),
              (override));
  MOCK_METHOD(void, OnEditorMenuVisibilityChanged, (bool visible), (override));
};

std::u16string_view GetChipLabel(const views::View* chip) {
  CHECK(views::IsViewClass<EditorMenuChipView>(chip));
  return views::AsViewClass<EditorMenuChipView>(chip)->GetText();
}

using EditorMenuViewTest = ChromeViewsTestBase;

class EditorMenuViewI18nEnabledTest : public EditorMenuViewTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kOrca,
                              chromeos::features::kFeatureManagementOrca,
                              chromeos::features::kOrcaUseL10nStrings},
        /*disabled_features=*/{});
    EditorMenuViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(EditorMenuViewTest, CreatesChips) {
  NiceMock<MockEditorMenuViewDelegate> delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Shorten", PresetQueryCategory::kShorten),
      PresetTextQuery("ID2", u"Elaborate", PresetQueryCategory::kElaborate)};

  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kRewrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());

  // Chips should be in a single row.
  const auto* chips_container = editor_menu_view->chips_container_for_testing();
  ASSERT_THAT(chips_container->children(), SizeIs(1));
  const auto* chip_row = chips_container->children()[0].get();
  ASSERT_THAT(chip_row->children(), SizeIs(queries.size()));
  // Chips should have correct text labels.
  EXPECT_EQ(GetChipLabel(chip_row->children()[0]), queries[0].name);
  EXPECT_EQ(GetChipLabel(chip_row->children()[1]), queries[1].name);
}

TEST_F(EditorMenuViewTest, CreatesChipsInMultipleRows) {
  NiceMock<MockEditorMenuViewDelegate> delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Rephrase", PresetQueryCategory::kRephrase),
      PresetTextQuery("ID2", u"Emojify", PresetQueryCategory::kEmojify),
      PresetTextQuery("ID3", u"Shorten", PresetQueryCategory::kShorten),
      PresetTextQuery("ID4", u"Elaborate", PresetQueryCategory::kElaborate),
      PresetTextQuery("ID5", u"Formalize", PresetQueryCategory::kFormalize)};

  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kRewrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());

  // Chips should be in two rows.
  EXPECT_THAT(
      editor_menu_view->chips_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(3))),
                  Pointee(Property(&views::View::children, SizeIs(2)))));
}

TEST_F(EditorMenuViewTest, TabKeyMovesFocus) {
  NiceMock<MockEditorMenuViewDelegate> delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Rephrase", PresetQueryCategory::kRephrase),
      PresetTextQuery("ID2", u"Emojify", PresetQueryCategory::kEmojify),
      PresetTextQuery("ID3", u"Shorten", PresetQueryCategory::kShorten),
      PresetTextQuery("ID4", u"Elaborate", PresetQueryCategory::kElaborate),
      PresetTextQuery("ID5", u"Formalize", PresetQueryCategory::kFormalize)};

  // Create and focus the editor menu.
  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kRewrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  editor_menu_view->RequestFocus();

  // Settings button should be focused.
  EXPECT_TRUE(views::IsViewClass<views::ImageButton>(
      editor_menu_view->GetFocusManager()->GetFocusedView()));

  // Press tab, focus should move to first chip.
  ui::test::EventGenerator generator(
      GetContext(), editor_menu_widget->GetNativeWindow()->GetRootWindow());
  generator.PressAndReleaseKey(ui::VKEY_TAB);

  ASSERT_TRUE(views::IsViewClass<EditorMenuChipView>(
      editor_menu_view->GetFocusManager()->GetFocusedView()));
  EXPECT_EQ(GetChipLabel(editor_menu_view->GetFocusManager()->GetFocusedView()),
            queries[0].name);

  // Press tab a few more times, focus should move to the last chip.
  generator.PressAndReleaseKey(ui::VKEY_TAB);
  generator.PressAndReleaseKey(ui::VKEY_TAB);
  generator.PressAndReleaseKey(ui::VKEY_TAB);
  generator.PressAndReleaseKey(ui::VKEY_TAB);

  ASSERT_TRUE(views::IsViewClass<EditorMenuChipView>(
      editor_menu_view->GetFocusManager()->GetFocusedView()));
  EXPECT_EQ(GetChipLabel(editor_menu_view->GetFocusManager()->GetFocusedView()),
            queries[4].name);

  // Press tab, focus should move to the textfield.
  generator.PressAndReleaseKey(ui::VKEY_TAB);

  EXPECT_TRUE(views::IsViewClass<views::Textfield>(
      editor_menu_view->GetFocusManager()->GetFocusedView()));
}

TEST_F(EditorMenuViewTest, EnterKeySubmitsPresetQuery) {
  NiceMock<MockEditorMenuViewDelegate> delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Rephrase", PresetQueryCategory::kRephrase),
      PresetTextQuery("ID2", u"Emojify", PresetQueryCategory::kEmojify)};

  // Create and show the editor menu.
  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kRewrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();

  // Focus the first chip.
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  auto* chip_row =
      editor_menu_view->chips_container_for_testing()->children()[0].get();
  chip_row->children()[0]->RequestFocus();

  EXPECT_TRUE(views::IsViewClass<EditorMenuChipView>(
      editor_menu_view->GetFocusManager()->GetFocusedView()));

  // Press enter key to submit preset query.
  EXPECT_CALL(delegate, OnChipButtonPressed(queries[0].text_query_id));
  ui::test::EventGenerator generator(
      GetContext(), editor_menu_widget->GetNativeWindow()->GetRootWindow());
  generator.PressAndReleaseKey(ui::VKEY_RETURN);
}

TEST_F(EditorMenuViewTest, EnterKeySubmitsFreeformQuery) {
  NiceMock<MockEditorMenuViewDelegate> delegate;

  // Create and show the editor menu.
  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kWrite, PresetTextQueries(),
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();

  // Focus the textfield.
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  editor_menu_view->textfield_for_testing()->textfield()->RequestFocus();

  EXPECT_TRUE(views::IsViewClass<views::Textfield>(
      editor_menu_view->GetFocusManager()->GetFocusedView()));

  // Type into the textfield, then press enter key to submit freeform query.
  EXPECT_CALL(delegate, OnTextfieldArrowButtonPressed(Eq(u"ab")));
  editor_menu_view->ResetPreTargetHandler();
  ui::test::EventGenerator generator(
      GetContext(), editor_menu_widget->GetNativeWindow()->GetRootWindow());
  generator.PressAndReleaseKey(ui::VKEY_A);
  generator.PressAndReleaseKey(ui::VKEY_B);
  generator.PressAndReleaseKey(ui::VKEY_RETURN);
}

TEST_F(EditorMenuViewTest, DisablesMenu) {
  NiceMock<MockEditorMenuViewDelegate> delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Rephrase", PresetQueryCategory::kRephrase),
      PresetTextQuery("ID2", u"Emojify", PresetQueryCategory::kEmojify)};

  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kRewrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  editor_menu_view->DisableMenu();

  // Chips should be disabled.
  const auto* chip_row =
      editor_menu_view->chips_container_for_testing()->children()[0].get();
  EXPECT_THAT(chip_row->children(),
              Each(Pointee(Property(&views::View::GetEnabled, false))));
  // Textfield should be disabled.
  EXPECT_FALSE(
      editor_menu_view->textfield_for_testing()->textfield()->GetEnabled());
  EXPECT_FALSE(
      editor_menu_view->textfield_for_testing()->arrow_button()->GetEnabled());
}

TEST_F(EditorMenuViewTest, AccessibleProperties) {
  NiceMock<MockEditorMenuViewDelegate> delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Rephrase", PresetQueryCategory::kRephrase),
      PresetTextQuery("ID2", u"Emojify", PresetQueryCategory::kEmojify)};

  // Rewrite Editor Mode
  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kRewrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  ui::AXNodeData data;

  editor_menu_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Rewrite");

  // Write Editor Mode
  editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kWrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();
  editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  data = ui::AXNodeData();

  editor_menu_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Help me write");
}

TEST_F(EditorMenuViewI18nEnabledTest, AccessibleProperties) {
  NiceMock<MockEditorMenuViewDelegate> delegate;
  const PresetTextQueries queries = {
      PresetTextQuery("ID1", u"Rephrase", PresetQueryCategory::kRephrase),
      PresetTextQuery("ID2", u"Emojify", PresetQueryCategory::kEmojify)};

  // Rewrite Editor Mode
  std::unique_ptr<views::Widget> editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kRewrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();
  auto* editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  ui::AXNodeData data;

  editor_menu_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_EDITOR_MENU_REWRITE_CARD_TITLE));

  // Write Editor Mode
  editor_menu_widget =
      EditorMenuView::CreateWidget(EditorMenuMode::kWrite, queries,
                                   gfx::Rect(200, 300, 400, 200), &delegate);
  editor_menu_widget->Show();
  editor_menu_view =
      views::AsViewClass<EditorMenuView>(editor_menu_widget->GetContentsView());
  data = ui::AXNodeData();

  editor_menu_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_EDITOR_MENU_WRITE_CARD_TITLE));
}

}  // namespace

}  // namespace chromeos::editor_menu
