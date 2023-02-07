// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

using testing::NiceMock;

namespace autofill {

namespace {

const std::vector<PopupItemId> kClickablePopupItemIds{
    POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY,
    POPUP_ITEM_ID_PASSWORD_ENTRY,
    POPUP_ITEM_ID_CLEAR_FORM,
    POPUP_ITEM_ID_AUTOFILL_OPTIONS,
    POPUP_ITEM_ID_DATALIST_ENTRY,
    POPUP_ITEM_ID_SCAN_CREDIT_CARD,
    POPUP_ITEM_ID_TITLE,
    POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO,
    POPUP_ITEM_ID_USERNAME_ENTRY,
    POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
    POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
    POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN,
    POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE,
    POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY,
    POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY,
};

const std::vector<PopupItemId> kUnclickablePopupItemIds{
    POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE,
    POPUP_ITEM_ID_SEPARATOR,
};

bool IsClickable(PopupItemId id) {
  DCHECK(base::Contains(kClickablePopupItemIds, id) ^
         base::Contains(kUnclickablePopupItemIds, id));
  return base::Contains(kClickablePopupItemIds, id);
}

}  // namespace

class PopupViewViewsTest : public ChromeViewsTestBase {
 public:
  PopupViewViewsTest() = default;
  PopupViewViewsTest(PopupViewViewsTest&) = delete;
  PopupViewViewsTest& operator=(PopupViewViewsTest&) = delete;
  ~PopupViewViewsTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void TearDown() override {
    generator_.reset();
    view_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void CreateAndShowView() {
    view_ = std::make_unique<PopupViewViews>(controller().GetWeakPtr(),
                                             widget_.get());
    widget().SetContentsView(view_.get());
    widget().Show();
    view().SchedulePaint();
  }

  void CreateAndShowView(const std::vector<int>& ids) {
    controller().set_suggestions(ids);
    CreateAndShowView();
  }

  void Paint() {
#if !BUILDFLAG(IS_MAC)
    Paint(widget().GetRootView());
#else
    // TODO(crbug.com/123): On Mac OS we need to trigger Paint() on the roots of
    // the individual rows. The reason is that the views::ViewScrollView()
    // created in PopupViewViews::CreateChildViews() owns a Layer.
    // As a consequence, views::View::Paint() does not propagate to the rows
    // because the recursion stops in views::View::RecursivePaintHelper().
    for (size_t index = 0; index < GetNumberOfRows(); ++index) {
      views::View* root = &GetRowViewAt(index);
      while (!root->layer() && root->parent()) {
        root = root->parent();
      }
      Paint(root);
    }
#endif
  }

  void Paint(views::View* view) {
    SkBitmap bitmap;
    gfx::Size size = view->size();
    ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                     false);
    view->Paint(
        views::PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
  }

  gfx::Point GetCenterOfSuggestion(size_t row_index) {
    return GetRowViewAt(row_index).GetBoundsInScreen().CenterPoint();
  }

 protected:
  views::View& GetRowViewAt(size_t index) {
    return *absl::visit([](views::View* view) { return view; },
                        view().GetRowsForTesting()[index]);
  }

  PopupRowView& GetPopupRowViewAt(size_t index) {
    return view().GetPopupRowViewAt(index);
  }

  size_t GetNumberOfRows() { return view().GetRowsForTesting().size(); }

  MockAutofillPopupController& controller() {
    return autofill_popup_controller_;
  }
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupViewViews& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<PopupViewViews> view_;
  NiceMock<MockAutofillPopupController> autofill_popup_controller_;
};

class PopupViewViewsTestWithAnyPopupItemId
    : public PopupViewViewsTest,
      public ::testing::WithParamInterface<PopupItemId> {
 public:
  PopupItemId popup_item_id() const { return GetParam(); }
};

class PopupViewViewsTestWithClickablePopupItemId
    : public PopupViewViewsTest,
      public ::testing::WithParamInterface<PopupItemId> {
 public:
  PopupItemId popup_item_id() const {
    DCHECK(IsClickable(GetParam()));
    return GetParam();
  }
};

TEST_F(PopupViewViewsTest, ShowHideTest) {
  CreateAndShowView({0});
  EXPECT_CALL(controller(), AcceptSuggestion(testing::_)).Times(0);
  view().Hide();
}

// This is a regression test for crbug.com/1113255.
TEST_F(PopupViewViewsTest, ShowViewWithOnlyFooterItemsShouldNotCrash) {
  // Set suggestions to have only a footer item.
  std::vector<int> suggestion_ids = {PopupItemId::POPUP_ITEM_ID_CLEAR_FORM};
  controller().set_suggestions(suggestion_ids);
  CreateAndShowView();
}

TEST_F(PopupViewViewsTest, AccessibilitySelectedEvent) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  CreateAndShowView({POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, POPUP_ITEM_ID_SEPARATOR,
                     POPUP_ITEM_ID_AUTOFILL_OPTIONS});

  // Checks that a selection event is not sent when the view's |is_selected_|
  // member does not change.
  GetPopupRowViewAt(0).SetSelected(false);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a selection event is sent when an unselected view becomes
  // selected.
  GetPopupRowViewAt(0).SetSelected(true);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when the view's
  // |is_selected_| member does not change.
  GetPopupRowViewAt(0).SetSelected(true);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when a selected view becomes
  // unselected.
  GetPopupRowViewAt(0).SetSelected(false);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));
}

TEST_F(PopupViewViewsTest, AccessibilityTest) {
  CreateAndShowView({POPUP_ITEM_ID_DATALIST_ENTRY, POPUP_ITEM_ID_SEPARATOR,
                     POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY,
                     POPUP_ITEM_ID_AUTOFILL_OPTIONS});

  // Select first item.
  GetPopupRowViewAt(0).SetSelected(true);

  EXPECT_EQ(GetNumberOfRows(), 4u);

  // Item 0.
  ui::AXNodeData node_data_0;
  GetRowViewAt(0).GetAccessibleNodeData(&node_data_0);
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_0.role);
  EXPECT_EQ(1, node_data_0.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_0.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_TRUE(
      node_data_0.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 1 (separator).
  ui::AXNodeData node_data_1;
  GetRowViewAt(1).GetAccessibleNodeData(&node_data_1);
  EXPECT_FALSE(node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_FALSE(node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kSplitter, node_data_1.role);
  EXPECT_FALSE(
      node_data_1.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 2.
  ui::AXNodeData node_data_2;
  GetRowViewAt(2).GetAccessibleNodeData(&node_data_2);
  EXPECT_EQ(2, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_2.role);
  EXPECT_FALSE(
      node_data_2.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 3 (footer).
  ui::AXNodeData node_data_3;
  GetRowViewAt(3).GetAccessibleNodeData(&node_data_3);
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_3.role);
  EXPECT_FALSE(
      node_data_3.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(PopupViewViewsTest, Gestures) {
  CreateAndShowView({POPUP_ITEM_ID_PASSWORD_ENTRY, POPUP_ITEM_ID_SEPARATOR,
                     POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY});

  // Tap down will select an element.
  ui::GestureEvent tap_down_event(
      /*x=*/0, /*y=*/0, /*flags=*/0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  EXPECT_CALL(controller(), SetSelectedLine(testing::Eq(0)));
  GetPopupRowViewAt(0).OnGestureEvent(&tap_down_event);

  // Tapping will accept the selection.
  ui::GestureEvent tap_event(/*x=*/0, /*y=*/0, /*flags=*/0,
                             ui::EventTimeForNow(),
                             ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  EXPECT_CALL(controller(), AcceptSuggestion(0));
  GetPopupRowViewAt(0).OnGestureEvent(&tap_event);

  // Canceling gesture clears any selection.
  ui::GestureEvent tap_cancel(
      /*x=*/0, /*y=*/0, /*flags=*/0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_CANCEL));
  EXPECT_CALL(controller(), SelectionCleared());
  GetRowViewAt(2).OnGestureEvent(&tap_cancel);
}

TEST_F(PopupViewViewsTest, ClickDisabledEntry) {
  Suggestion opt_int_suggestion("", "", "",
                                POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN);
  opt_int_suggestion.is_loading = Suggestion::IsLoading(true);
  controller().set_suggestions({opt_int_suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);

  gfx::Point inside_point(GetRowViewAt(0).x() + 1, GetRowViewAt(0).y() + 1);
  ui::MouseEvent click_mouse_event(
      ui::ET_MOUSE_PRESSED, inside_point, inside_point, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  widget().OnMouseEvent(&click_mouse_event);
}

// Ensure that the voice_over value of suggestions is presented to the
// accessibility layer.
TEST_F(PopupViewViewsTest, VoiceOverTest) {
  const std::u16string voice_over_value = u"Password for user@gmail.com";
  // Create a realistic suggestion for a password.
  Suggestion suggestion(u"user@gmail.com");
  suggestion.labels = {{Suggestion::Text(u"example.com")}};
  suggestion.voice_over = voice_over_value;
  suggestion.additional_label = u"\u2022\u2022\u2022\u2022";
  suggestion.frontend_id = POPUP_ITEM_ID_USERNAME_ENTRY;

  // Create autofill menu.
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  // Verify that the accessibility layer gets the right string to read out.
  ui::AXNodeData node_data;
  GetRowViewAt(0).GetAccessibleNodeData(&node_data);
  EXPECT_EQ(voice_over_value,
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

// Tests that (only) clickable items trigger an AcceptSuggestion event.
TEST_P(PopupViewViewsTestWithAnyPopupItemId, ShowClickTest) {
  CreateAndShowView({popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion)
      .Times(IsClickable(popup_item_id()));
  generator().MoveMouseTo(gfx::Point(1000, 1000));
  ASSERT_FALSE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
  view().RemoveAllChildViews();
}

// Tests that after the mouse moves into the popup after display, clicking a
// suggestion triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickablePopupItemId,
       AcceptSuggestionIfUnfocusedAtPaint) {
  CreateAndShowView({popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(1);
  generator().MoveMouseTo(gfx::Point(1000, 1000));
  ASSERT_FALSE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
  view().RemoveAllChildViews();
}

// Tests that after the mouse moves from one suggestion to another, clicking the
// suggestion triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickablePopupItemId,
       AcceptSuggestionIfMouseSelectedAnotherRow) {
  CreateAndShowView({popup_item_id(), popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(1);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(1));  // Selects another row.
  generator().ClickLeftButton();
  view().RemoveAllChildViews();
}

// Tests that after the mouse moves from one suggestion to another and back to
// the first one, clicking the suggestion triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickablePopupItemId,
       AcceptSuggestionIfMouseTemporarilySelectedAnotherRow) {
  CreateAndShowView({popup_item_id(), popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(1);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(1));  // Selects another row.
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
  view().RemoveAllChildViews();
}

// Tests that even if the mouse hovers a suggestion when the popup is displayed,
// after moving the mouse out and back in on the popup, clicking the suggestion
// triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickablePopupItemId,
       AcceptSuggestionIfMouseExitedPopupSincePaint) {
  CreateAndShowView({popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(1);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(gfx::Point(1000, 1000));  // Exits the popup.
  ASSERT_FALSE(view().IsMouseHovered());
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
  view().RemoveAllChildViews();
}

// Tests that if the mouse hovers a suggestion when the popup is displayed,
// clicking the suggestion triggers no AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickablePopupItemId,
       IgnoreClickIfFocusedAtPaintWithoutExit) {
  CreateAndShowView({popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().ClickLeftButton();
  view().RemoveAllChildViews();
}

// Tests that if the mouse hovers a suggestion when the popup is displayed and
// moves around on this suggestion, clicking the suggestion triggers no
// AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickablePopupItemId,
       IgnoreClickIfFocusedAtPaintWithSlightMouseMovement) {
  CreateAndShowView({popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  int width = GetRowViewAt(0).width();
  int height = GetRowViewAt(0).height();
  for (int x : {-width / 3, width / 3}) {
    for (int y : {-height / 3, height / 3}) {
      generator().MoveMouseTo(GetCenterOfSuggestion(0) + gfx::Vector2d(x, y));
      ASSERT_TRUE(view().IsMouseHovered());
      Paint();
    }
  }
  generator().ClickLeftButton();
  view().RemoveAllChildViews();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsTestWithAnyPopupItemId,
                         testing::ValuesIn([] {
                           std::vector<PopupItemId> all_ids;
                           all_ids.insert(all_ids.end(),
                                          kClickablePopupItemIds.begin(),
                                          kClickablePopupItemIds.end());
                           all_ids.insert(all_ids.end(),
                                          kUnclickablePopupItemIds.begin(),
                                          kUnclickablePopupItemIds.end());
                           return all_ids;
                         }()));

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsTestWithClickablePopupItemId,
                         testing::ValuesIn(kClickablePopupItemIds));

}  // namespace autofill
