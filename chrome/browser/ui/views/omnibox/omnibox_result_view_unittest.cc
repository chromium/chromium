// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <memory>

#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace {

// An arbitrary index for the result view under test. Used to test the selection
// state. There are 6 results total so the index should be in the range 0-5.
static constexpr int kTestResultViewIndex = 4;

class TestOmniboxPopupContentsView : public OmniboxPopupContentsView {
 public:
  explicit TestOmniboxPopupContentsView(OmniboxEditModel* edit_model,
                                        const ui::ThemeProvider* theme_provider)
      : OmniboxPopupContentsView(
            /*omnibox_view=*/nullptr,
            edit_model,
            /*location_bar_view=*/nullptr,
            theme_provider),
        selected_index_(0) {}

  void SetSelectedLine(size_t index) override { selected_index_ = index; }

  bool IsSelectedIndex(size_t index) const override {
    return selected_index_ == index;
  }

 private:
  size_t selected_index_;

  DISALLOW_COPY_AND_ASSIGN(TestOmniboxPopupContentsView);
};

}  // namespace

class OmniboxResultViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create a widget and assign bounds to support calls to HitTestPoint.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(std::move(init_params));

    const ui::ThemeProvider* theme_provider = widget_->GetThemeProvider();
    edit_model_ = std::make_unique<OmniboxEditModel>(
        nullptr, nullptr, std::make_unique<TestOmniboxClient>());
    popup_view_ = std::make_unique<TestOmniboxPopupContentsView>(
        edit_model_.get(), theme_provider);
    result_view_ = new OmniboxResultView(popup_view_.get(),
                                         kTestResultViewIndex, theme_provider);

    views::View* root_view = widget_->GetRootView();
    root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));
    result_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
    root_view->AddChildView(result_view_);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  ui::MouseEvent CreateEvent(ui::EventType type, int flags) {
    return CreateEvent(type, flags, 0, 0);
  }

  ui::MouseEvent CreateEvent(ui::EventType type, int flags, float x, float y) {
    return ui::MouseEvent(type, gfx::Point(x, y), gfx::Point(),
                          ui::EventTimeForNow(), flags, 0);
  }

  OmniboxPopupContentsView* popup_view() { return popup_view_.get(); }
  OmniboxResultView* result_view() { return result_view_; }

 private:
  std::unique_ptr<OmniboxEditModel> edit_model_;
  std::unique_ptr<TestOmniboxPopupContentsView> popup_view_;
  OmniboxResultView* result_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(OmniboxResultViewTest, MousePressedWithLeftButtonSelectsThisResult) {
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Right button press should not select.
  result_view()->OnMousePressed(
      CreateEvent(ui::ET_MOUSE_PRESSED, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Middle button press should not select.
  result_view()->OnMousePressed(
      CreateEvent(ui::ET_MOUSE_PRESSED, ui::EF_MIDDLE_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Multi-button press should not select.
  result_view()->OnMousePressed(
      CreateEvent(ui::ET_MOUSE_PRESSED,
                  ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Left button press should select.
  result_view()->OnMousePressed(
      CreateEvent(ui::ET_MOUSE_PRESSED, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_TRUE(popup_view()->IsSelectedIndex(kTestResultViewIndex));
}

TEST_F(OmniboxResultViewTest, MouseDragWithLeftButtonSelectsThisResult) {
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Right button drag should not select.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Middle button drag should not select.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED, ui::EF_MIDDLE_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Multi-button drag should not select.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED,
                  ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_FALSE(popup_view()->IsSelectedIndex(kTestResultViewIndex));

  // Left button drag should select.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_TRUE(popup_view()->IsSelectedIndex(kTestResultViewIndex));
}

TEST_F(OmniboxResultViewTest, MouseDragWithNonLeftButtonSetsHoveredState) {
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Right button drag should put the view in the HOVERED state.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Left button drag should take the view out of the HOVERED state.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());
}

TEST_F(OmniboxResultViewTest, MouseDragOutOfViewCancelsHoverState) {
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Right button drag in the view should put the view in the HOVERED state.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON, 0, 0));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Right button drag outside of the view should revert the HOVERED state.
  result_view()->OnMouseDragged(
      CreateEvent(ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON, 200, 200));
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());
}

TEST_F(OmniboxResultViewTest, MouseMoveAndExitSetsHoveredState) {
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Moving the mouse over the view should put the view in the HOVERED state.
  result_view()->OnMouseMoved(CreateEvent(ui::ET_MOUSE_MOVED, 0));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Continuing to move over the view should not change the state.
  result_view()->OnMouseMoved(CreateEvent(ui::ET_MOUSE_MOVED, 0));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // But exiting should revert the HOVERED state.
  result_view()->OnMouseExited(CreateEvent(ui::ET_MOUSE_MOVED, 0));
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());
}

TEST_F(OmniboxResultViewTest, AccessibleNodeData) {
  // Check accessibility of result.
  base::string16 match_url = base::ASCIIToUTF16("https://google.com");
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = base::ASCIIToUTF16("Google");
  match.allowed_to_be_default_match = true;
  result_view()->SetMatch(match);
  ui::AXNodeData result_node_data;
  result_view()->GetAccessibleNodeData(&result_node_data);
  EXPECT_TRUE(
      result_node_data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(
      result_node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(result_node_data.role, ax::mojom::Role::kListBoxOption);
  EXPECT_EQ(
      result_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      base::ASCIIToUTF16("Google https://google.com location from history"));
  EXPECT_EQ(
      result_node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
      kTestResultViewIndex + 1);
  // TODO(accessibility) Find a way to test this.
  // EXPECT_EQ(result_node_data.GetIntAttribute(
  //   ax::mojom::IntAttribute::kSetSize), 1);

  // Select it and check selected state.
  ui::AXNodeData result_after_click;
  result_view()->OnMousePressed(
      CreateEvent(ui::ET_MOUSE_PRESSED, ui::EF_LEFT_MOUSE_BUTTON));
  result_view()->GetAccessibleNodeData(&result_after_click);
  EXPECT_TRUE(
      result_after_click.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Check accessibility of list box.
  ui::AXNodeData popup_node_data;
  popup_view()->GetAccessibleNodeData(&popup_node_data);
  EXPECT_EQ(popup_node_data.role, ax::mojom::Role::kListBox);
  EXPECT_FALSE(popup_node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(popup_node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(popup_node_data.HasState(ax::mojom::State::kInvisible));
}
