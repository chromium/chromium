// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

class OmniboxEditModel;

namespace {

// An arbitrary index for the result view under test. Used to test the selection
// state. There are 6 results total so the index should be in the range 0-5.
static constexpr size_t kTestResultViewIndex = 4;

class TestOmniboxPopupViewViews : public OmniboxPopupViewViews {
 public:
  explicit TestOmniboxPopupViewViews(OmniboxEditModel* edit_model)
      : OmniboxPopupViewViews(
            /*omnibox_view=*/nullptr,
            edit_model,
            /*location_bar_view=*/nullptr),
        selection_(OmniboxPopupSelection(0, OmniboxPopupSelection::NORMAL)) {}

  TestOmniboxPopupViewViews(const TestOmniboxPopupViewViews&) = delete;
  TestOmniboxPopupViewViews& operator=(const TestOmniboxPopupViewViews&) =
      delete;

  void SetSelectedIndex(size_t index) override { selection_.line = index; }

  size_t GetSelectedIndex() const override { return selection_.line; }

  OmniboxPopupSelection GetSelection() const override { return selection_; }

 private:
  OmniboxPopupSelection selection_;
};

}  // namespace

class OmniboxResultViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
#if !defined(USE_AURA)
    test_screen_ = std::make_unique<display::test::TestScreen>();
    display::Screen::SetScreenInstance(test_screen_.get());
#endif
    ChromeViewsTestBase::SetUp();

    // Create a widget and assign bounds to support calls to HitTestPoint.
    widget_ = CreateTestWidget();

    omnibox_controller_ = std::make_unique<OmniboxController>(
        /*view=*/nullptr, /*edit_model_delegate=*/nullptr,
        std::make_unique<TestOmniboxClient>());
    popup_view_ = std::make_unique<TestOmniboxPopupViewViews>(edit_model());
    result_view_ = new OmniboxResultView(popup_view_.get(), edit_model(),
                                         kTestResultViewIndex);

    views::View* root_view = widget_->GetRootView();
    root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));
    result_view_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
    root_view->AddChildView(result_view_.get());

    // Start by not hovering over the result view.
    FakeMouseEvent(ui::ET_MOUSE_MOVED, 0, 200, 200);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
    display::Screen::SetScreenInstance(nullptr);
    test_screen_.reset();
  }

  // Also sets the fake screen's mouse cursor to 0, 0.
  ui::MouseEvent FakeMouseEvent(ui::EventType type, int flags) {
    return FakeMouseEvent(type, flags, 0, 0);
  }

  // Also sets the fake screen's mouse cursor to |x|, |y|.
  ui::MouseEvent FakeMouseEvent(ui::EventType type,
                                int flags,
                                float x,
                                float y) {
#if !defined(USE_AURA)
    test_screen_->set_cursor_screen_point(gfx::Point(x, y));
#else
    aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(x, y));
#endif
    return ui::MouseEvent(type, gfx::Point(x, y), gfx::Point(),
                          ui::EventTimeForNow(), flags, 0);
  }

  OmniboxEditModel* edit_model() { return omnibox_controller_->edit_model(); }
  OmniboxPopupViewViews* popup_view() { return popup_view_.get(); }
  OmniboxResultView* result_view() { return result_view_; }

 private:
  std::unique_ptr<OmniboxController> omnibox_controller_;
  std::unique_ptr<TestOmniboxPopupViewViews> popup_view_;
  raw_ptr<OmniboxResultView> result_view_;
  std::unique_ptr<views::Widget> widget_;

  std::unique_ptr<display::test::TestScreen> test_screen_;
};

TEST_F(OmniboxResultViewTest, MousePressedWithLeftButtonSelectsThisResult) {
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Right button press should not select.
  result_view()->OnMousePressed(
      FakeMouseEvent(ui::ET_MOUSE_PRESSED, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Middle button press should not select.
  result_view()->OnMousePressed(
      FakeMouseEvent(ui::ET_MOUSE_PRESSED, ui::EF_MIDDLE_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Multi-button press should not select.
  result_view()->OnMousePressed(
      FakeMouseEvent(ui::ET_MOUSE_PRESSED,
                     ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Left button press should select.
  result_view()->OnMousePressed(
      FakeMouseEvent(ui::ET_MOUSE_PRESSED, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_EQ(popup_view()->GetSelectedIndex(), kTestResultViewIndex);
}

TEST_F(OmniboxResultViewTest, MouseDragWithLeftButtonSelectsThisResult) {
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Right button drag should not select.
  result_view()->OnMouseDragged(
      FakeMouseEvent(ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Middle button drag should not select.
  result_view()->OnMouseDragged(
      FakeMouseEvent(ui::ET_MOUSE_DRAGGED, ui::EF_MIDDLE_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Multi-button drag should not select.
  result_view()->OnMouseDragged(
      FakeMouseEvent(ui::ET_MOUSE_DRAGGED,
                     ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_NE(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_NE(popup_view()->GetSelectedIndex(), kTestResultViewIndex);

  // Left button drag should select.
  result_view()->OnMouseDragged(
      FakeMouseEvent(ui::ET_MOUSE_DRAGGED, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(OmniboxPartState::SELECTED, result_view()->GetThemeState());
  EXPECT_EQ(popup_view()->GetSelectedIndex(), kTestResultViewIndex);
}

TEST_F(OmniboxResultViewTest, MouseDragWithNonLeftButtonSetsHoveredState) {
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Right button drag should put the view in the HOVERED state.
  result_view()->OnMouseDragged(
      FakeMouseEvent(ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON, 50, 50));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Left button drag should take the view out of the HOVERED state.
  result_view()->OnMouseDragged(
      FakeMouseEvent(ui::ET_MOUSE_DRAGGED, ui::EF_LEFT_MOUSE_BUTTON, 200, 200));
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());
}

TEST_F(OmniboxResultViewTest, MouseDragOutOfViewCancelsHoverState) {
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Right button drag in the view should put the view in the HOVERED state.
  result_view()->OnMouseDragged(
      FakeMouseEvent(ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON, 50, 50));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Right button drag outside of the view should revert the HOVERED state.
  result_view()->OnMouseDragged(FakeMouseEvent(
      ui::ET_MOUSE_DRAGGED, ui::EF_RIGHT_MOUSE_BUTTON, 200, 200));
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());
}

TEST_F(OmniboxResultViewTest, MouseEnterAndExitSetsHoveredState) {
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // The mouse entering the view should put the view in the HOVERED state.
  result_view()->OnMouseMoved(FakeMouseEvent(ui::ET_MOUSE_MOVED, 0, 50, 50));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // Continuing to move over the view should not change the state.
  result_view()->OnMouseMoved(FakeMouseEvent(ui::ET_MOUSE_MOVED, 0, 50, 50));
  EXPECT_EQ(OmniboxPartState::HOVERED, result_view()->GetThemeState());

  // But exiting should revert the HOVERED state.
  result_view()->OnMouseExited(FakeMouseEvent(ui::ET_MOUSE_MOVED, 0, 200, 200));
  EXPECT_NE(OmniboxPartState::HOVERED, result_view()->GetThemeState());
}

TEST_F(OmniboxResultViewTest, AccessibleNodeData) {
  // Check accessibility of result.
  std::u16string match_url = u"https://google.com";
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = u"Google";
  match.allowed_to_be_default_match = true;
  result_view()->SetMatch(match);
  ui::AXNodeData result_node_data;
  result_view()->GetAccessibleNodeData(&result_node_data);
  EXPECT_TRUE(
      result_node_data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(
      result_node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(result_node_data.role, ax::mojom::Role::kListBoxOption);
  // TODO(tommycli) Find a way to test this.
  // EXPECT_EQ(
  //   result_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
  //   u"Google https://google.com location from history");
  EXPECT_EQ(
      result_node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
      int{kTestResultViewIndex} + 1);
  // TODO(accessibility) Find a way to test this.
  // EXPECT_EQ(result_node_data.GetIntAttribute(
  //   ax::mojom::IntAttribute::kSetSize), 1);

  // Select it and check selected state.
  ui::AXNodeData result_after_click;
  result_view()->OnMousePressed(
      FakeMouseEvent(ui::ET_MOUSE_PRESSED, ui::EF_LEFT_MOUSE_BUTTON));
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
  EXPECT_FALSE(
      popup_node_data.HasIntAttribute(ax::mojom::IntAttribute::kPopupForId));
}
