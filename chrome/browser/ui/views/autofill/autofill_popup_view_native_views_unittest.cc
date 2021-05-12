// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/widget/widget_utils.h"

namespace {

struct TypeClicks {
  autofill::PopupItemId id;
  int click;
};

const struct TypeClicks kClickTestCase[] = {
    {autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE, 0},
    {autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_SEPARATOR, 0},
    {autofill::POPUP_ITEM_ID_CLEAR_FORM, 1},
    {autofill::POPUP_ITEM_ID_AUTOFILL_OPTIONS, 1},
    {autofill::POPUP_ITEM_ID_DATALIST_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_SCAN_CREDIT_CARD, 1},
    {autofill::POPUP_ITEM_ID_TITLE, 1},
    {autofill::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO, 1},
    {autofill::POPUP_ITEM_ID_USERNAME_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN, 1},
    {autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN, 1},
    {autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE, 1},
    {autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY, 1},
};

class AutofillPopupViewNativeViewsTest : public ChromeViewsTestBase {
 public:
  AutofillPopupViewNativeViewsTest() = default;
  ~AutofillPopupViewNativeViewsTest() override = default;

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

  void CreateAndShowView(const std::vector<int>& ids) {
    autofill_popup_controller_.set_suggestions(ids);
    view_ = std::make_unique<autofill::AutofillPopupViewNativeViews>(
        &autofill_popup_controller_, widget_.get());
    widget_->SetContentsView(view_.get());

    widget_->Show();
  }

  autofill::AutofillPopupViewNativeViews* view() { return view_.get(); }

 protected:
  std::unique_ptr<autofill::AutofillPopupViewNativeViews> view_;
  autofill::MockAutofillPopupController autofill_popup_controller_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewNativeViewsTest);
};

class AutofillPopupViewNativeViewsForEveryTypeTest
    : public AutofillPopupViewNativeViewsTest,
      public ::testing::WithParamInterface<TypeClicks> {};

TEST_F(AutofillPopupViewNativeViewsTest, ShowHideTest) {
  CreateAndShowView({0});
  EXPECT_CALL(autofill_popup_controller_, AcceptSuggestion(testing::_))
      .Times(0);
  view()->Hide();
}

// This is a regression test for crbug.com/1113255
TEST_F(AutofillPopupViewNativeViewsTest,
       ShowViewWithOnlyFooterItemsShouldNotCrash) {
  // Set suggestions to have only a footer item.
  autofill_popup_controller_.set_suggestions(
      {autofill::PopupItemId::POPUP_ITEM_ID_CLEAR_FORM});
  view_ = std::make_unique<autofill::AutofillPopupViewNativeViews>(
      &autofill_popup_controller_, widget_.get());
  widget_->SetContentsView(view_.get());
  widget_->Show();
  view_->Show();
}

TEST_F(AutofillPopupViewNativeViewsTest, AccessibilitySelectedEvent) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  CreateAndShowView({autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY,
                     autofill::POPUP_ITEM_ID_SEPARATOR,
                     autofill::POPUP_ITEM_ID_AUTOFILL_OPTIONS});

  // Checks that a selection event is not sent when the view's |is_selected_|
  // member does not change.
  view()->GetRowsForTesting()[0]->SetSelected(false);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a selection event is sent when an unselected view becomes
  // selected.
  view()->GetRowsForTesting()[0]->SetSelected(true);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when the view's
  // |is_selected_| member does not change.
  view()->GetRowsForTesting()[0]->SetSelected(true);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when a selected view becomes
  // unselected.
  view()->GetRowsForTesting()[0]->SetSelected(false);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));
}

TEST_F(AutofillPopupViewNativeViewsTest, AccessibilityTest) {
  CreateAndShowView({autofill::POPUP_ITEM_ID_DATALIST_ENTRY,
                     autofill::POPUP_ITEM_ID_SEPARATOR,
                     autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY,
                     autofill::POPUP_ITEM_ID_AUTOFILL_OPTIONS});

  // Select first item.
  view()->GetRowsForTesting()[0]->SetSelected(true);

  EXPECT_EQ(view()->GetRowsForTesting().size(), 4u);

  // Item 0.
  ui::AXNodeData node_data_0;
  view()->GetRowsForTesting()[0]->GetAccessibleNodeData(&node_data_0);
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_0.role);
  EXPECT_EQ(1, node_data_0.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_0.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_TRUE(
      node_data_0.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 1 (separator).
  ui::AXNodeData node_data_1;
  view()->GetRowsForTesting()[1]->GetAccessibleNodeData(&node_data_1);
  EXPECT_FALSE(node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_FALSE(node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kSplitter, node_data_1.role);
  EXPECT_FALSE(
      node_data_1.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 2.
  ui::AXNodeData node_data_2;
  view()->GetRowsForTesting()[2]->GetAccessibleNodeData(&node_data_2);
  EXPECT_EQ(2, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_2.role);
  EXPECT_FALSE(
      node_data_2.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 3 (footer).
  ui::AXNodeData node_data_3;
  view()->GetRowsForTesting()[3]->GetAccessibleNodeData(&node_data_3);
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_3.role);
  EXPECT_FALSE(
      node_data_3.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(AutofillPopupViewNativeViewsTest, Gestures) {
  CreateAndShowView({autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                     autofill::POPUP_ITEM_ID_SEPARATOR,
                     autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY});

  // Tap down will select an element.
  ui::GestureEvent tap_down_event(
      /*x=*/0, /*y=*/0, /*flags=*/0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  EXPECT_CALL(autofill_popup_controller_, SetSelectedLine(testing::Eq(0)));
  view()->GetRowsForTesting()[0]->OnGestureEvent(&tap_down_event);

  // Tapping will accept the selection.
  ui::GestureEvent tap_event(/*x=*/0, /*y=*/0, /*flags=*/0,
                             ui::EventTimeForNow(),
                             ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  EXPECT_CALL(autofill_popup_controller_, AcceptSuggestion(0));
  view()->GetRowsForTesting()[0]->OnGestureEvent(&tap_event);

  // Canceling gesture clears any selection.
  ui::GestureEvent tap_cancel(
      /*x=*/0, /*y=*/0, /*flags=*/0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_CANCEL));
  EXPECT_CALL(autofill_popup_controller_, SelectionCleared());
  view()->GetRowsForTesting()[2]->OnGestureEvent(&tap_cancel);
}

TEST_F(AutofillPopupViewNativeViewsTest, ClickDisabledEntry) {
  autofill::Suggestion opt_int_suggestion(
      "", "", "", autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN);
  opt_int_suggestion.is_loading = autofill::Suggestion::IsLoading(true);
  autofill_popup_controller_.set_suggestions({opt_int_suggestion});
  view_ = std::make_unique<autofill::AutofillPopupViewNativeViews>(
      &autofill_popup_controller_, widget_.get());
  widget_->SetContentsView(view_.get());
  widget_->Show();

  EXPECT_CALL(autofill_popup_controller_, AcceptSuggestion).Times(0);

  gfx::Point inside_point(view()->GetRowsForTesting()[0]->x() + 1,
                          view()->GetRowsForTesting()[0]->y() + 1);
  ui::MouseEvent click_mouse_event(
      ui::ET_MOUSE_PRESSED, inside_point, inside_point, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
  widget_->OnMouseEvent(&click_mouse_event);
}

TEST_P(AutofillPopupViewNativeViewsForEveryTypeTest, ShowClickTest) {
  const TypeClicks& click = GetParam();
  CreateAndShowView({click.id});
  EXPECT_CALL(autofill_popup_controller_, AcceptSuggestion).Times(click.click);
  gfx::Point center =
      view()->GetRowsForTesting()[0]->GetBoundsInScreen().CenterPoint();

  // Because we use GetBoundsInScreen above, and because macOS may reposition
  // the window, we need to turn this bit off or the clicks will miss their
  // targets.
  generator_->set_assume_window_at_origin(false);
  generator_->set_current_screen_location(center);
  generator_->ClickLeftButton();
  view()->RemoveAllChildViews(true /* delete_children */);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillPopupViewNativeViewsForEveryTypeTest,
                         ::testing::ValuesIn(kClickTestCase));

}  // namespace
