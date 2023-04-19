// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::IsFalse;
using testing::IsTrue;

class MockReadAnythingToolbarViewDelegate
    : public ReadAnythingToolbarView::Delegate {
 public:
  MOCK_METHOD(void, OnFontSizeChanged, (bool increase), (override));
  MOCK_METHOD(void, OnColorsChanged, (int new_index), (override));
  MOCK_METHOD(ReadAnythingMenuModel*, GetColorsModel, (), (override));
  MOCK_METHOD(void, OnLineSpacingChanged, (int new_index), (override));
  MOCK_METHOD(ReadAnythingMenuModel*, GetLineSpacingModel, (), (override));
  MOCK_METHOD(void, OnLetterSpacingChanged, (int new_index), (override));
  MOCK_METHOD(ReadAnythingMenuModel*, GetLetterSpacingModel, (), (override));
  MOCK_METHOD(void, OnSystemThemeChanged, (), (override));
};

class MockReadAnythingFontComboboxDelegate
    : public ReadAnythingFontCombobox::Delegate {
 public:
  MOCK_METHOD(void, OnFontChoiceChanged, (int new_index), (override));
  MOCK_METHOD(ReadAnythingFontModel*, GetFontComboboxModel, (), (override));
};

class MockReadAnythingCoordinator : public ReadAnythingCoordinator {
 public:
  explicit MockReadAnythingCoordinator(Browser* browser)
      : ReadAnythingCoordinator(browser) {}

  MOCK_METHOD(void,
              CreateAndRegisterEntry,
              (SidePanelRegistry * global_registry));
  MOCK_METHOD(ReadAnythingController*, GetController, ());
  MOCK_METHOD(ReadAnythingModel*, GetModel, ());
  MOCK_METHOD(void,
              AddObserver,
              (ReadAnythingCoordinator::Observer * observer));
  MOCK_METHOD(void,
              RemoveObserver,
              (ReadAnythingCoordinator::Observer * observer));
};

class ReadAnythingToolbarViewTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    coordinator_ = std::make_unique<MockReadAnythingCoordinator>(browser());

    toolbar_view_ = std::make_unique<ReadAnythingToolbarView>(
        coordinator_.get(), &toolbar_delegate_, &font_combobox_delegate_);
  }

  void TearDownOnMainThread() override { coordinator_ = nullptr; }

  // Wrapper methods around the ReadAnythingToolbarView.

  void DecreaseFontSizeCallback() { toolbar_view_->DecreaseFontSizeCallback(); }

  void IncreaseFontSizeCallback() { toolbar_view_->IncreaseFontSizeCallback(); }

  void ChangeColorsCallback() { toolbar_view_->ChangeColorsCallback(); }

  void ChangeLineSpacingCallback() {
    toolbar_view_->ChangeLineSpacingCallback();
  }

  void ChangeLetterSpacingCallback() {
    toolbar_view_->ChangeLetterSpacingCallback();
  }

  void OnThemeChanged() { toolbar_view_->OnThemeChanged(); }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) {
    toolbar_view_->GetAccessibleNodeData(node_data);
  }

  void OnReadAnythingThemeChanged(
      const std::string& font_name,
      double font_scale,
      ui::ColorId foreground_color_id,
      ui::ColorId background_color_id,
      ui::ColorId separator_color_id,
      ui::ColorId dropdown_color_id,
      ui::ColorId selected_color_id,
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing) {
    toolbar_view_->OnReadAnythingThemeChanged(
        font_name, font_scale, foreground_color_id, background_color_id,
        separator_color_id, dropdown_color_id, selected_color_id, line_spacing,
        letter_spacing);
  }

  views::Button::ButtonState GetDecreaseSizeButtonState() {
    return toolbar_view_->decrease_text_size_button_->GetState();
  }

  views::Button::ButtonState GetIncreaseSizeButtonState() {
    return toolbar_view_->increase_text_size_button_->GetState();
  }

  std::vector<views::View*> GetChildren() {
    std::vector<views::View*> children;
    children.emplace_back(toolbar_view_->font_combobox_);
    children.emplace_back(toolbar_view_->increase_text_size_button_);
    children.emplace_back(toolbar_view_->decrease_text_size_button_);
    children.emplace_back(toolbar_view_->colors_button_);
    children.emplace_back(toolbar_view_->line_spacing_button_);
    children.emplace_back(toolbar_view_->letter_spacing_button_);
    children.shrink_to_fit();
    return children;
  }

 protected:
  MockReadAnythingToolbarViewDelegate toolbar_delegate_;
  MockReadAnythingFontComboboxDelegate font_combobox_delegate_;

 private:
  std::unique_ptr<ReadAnythingToolbarView> toolbar_view_;
  std::unique_ptr<MockReadAnythingCoordinator> coordinator_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest,
                       DecreaseButtonDisabledAtMin) {
  OnReadAnythingThemeChanged(
      "", kReadAnythingMinimumFontScale, kColorReadAnythingForeground,
      kColorReadAnythingForeground, kColorReadAnythingForeground,
      kColorReadAnythingForeground, kColorReadAnythingForeground,
      read_anything::mojom::LineSpacing::kStandard,
      read_anything::mojom::LetterSpacing::kStandard);

  EXPECT_EQ(GetDecreaseSizeButtonState(),
            views::Button::ButtonState::STATE_DISABLED);
  EXPECT_EQ(GetIncreaseSizeButtonState(),
            views::Button::ButtonState::STATE_NORMAL);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, DecreaseFontSizeCallback) {
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(false)).Times(1);
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(true)).Times(0);

  DecreaseFontSizeCallback();

  EXPECT_EQ(GetDecreaseSizeButtonState(),
            views::Button::ButtonState::STATE_NORMAL);
  EXPECT_EQ(GetIncreaseSizeButtonState(),
            views::Button::ButtonState::STATE_NORMAL);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest,
                       IncreaseButtonDisabledAtMax) {
  OnReadAnythingThemeChanged(
      "", kReadAnythingMaximumFontScale, kColorReadAnythingForeground,
      kColorReadAnythingForeground, kColorReadAnythingForeground,
      kColorReadAnythingForeground, kColorReadAnythingForeground,
      read_anything::mojom::LineSpacing::kStandard,
      read_anything::mojom::LetterSpacing::kStandard);

  EXPECT_EQ(GetDecreaseSizeButtonState(),
            views::Button::ButtonState::STATE_NORMAL);
  EXPECT_EQ(GetIncreaseSizeButtonState(),
            views::Button::ButtonState::STATE_DISABLED);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, IncreaseFontSizeCallback) {
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(false)).Times(0);
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(true)).Times(1);

  IncreaseFontSizeCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, OnThemeChanged) {
  EXPECT_CALL(toolbar_delegate_, OnSystemThemeChanged()).Times(1);

  OnThemeChanged();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, ChangeColorsCallback) {
  EXPECT_CALL(toolbar_delegate_, OnColorsChanged(_)).Times(1);

  ChangeColorsCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, ChangeLineSpacingCallback) {
  EXPECT_CALL(toolbar_delegate_, OnLineSpacingChanged(_)).Times(1);

  ChangeLineSpacingCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest,
                       ChangeLetterSpacingCallback) {
  EXPECT_CALL(toolbar_delegate_, OnLetterSpacingChanged(_)).Times(1);

  ChangeLetterSpacingCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, AccessibleLabel) {
  ui::AXNodeData node_data;
  GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kToolbar, node_data.role);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_READING_MODE_TOOLBAR_LABEL),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest,
                       AllToolbarElementsInOneGroup) {
  for (views::View* view : GetChildren()) {
    EXPECT_EQ(view->GetGroup(), kToolbarGroupId);
  }
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest,
                       OnlyFirstElementIsGroupTraversable) {
  std::vector<views::View*> children = GetChildren();
  EXPECT_TRUE(children.front()->IsGroupFocusTraversable());
  for (size_t i = 1; i < children.size(); i++) {
    EXPECT_FALSE(children.at(i)->IsGroupFocusTraversable());
  }
}
