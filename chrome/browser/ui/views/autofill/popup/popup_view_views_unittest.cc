// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views_test_api.h"
#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using CellIndex = PopupViewViews::CellIndex;
using CellType = PopupRowView::CellType;

const std::vector<PopupItemId> kClickablePopupItemIds{
    PopupItemId::kAutocompleteEntry,
    PopupItemId::kPasswordEntry,
    PopupItemId::kClearForm,
    PopupItemId::kAutofillOptions,
    PopupItemId::kDatalistEntry,
    PopupItemId::kScanCreditCard,
    PopupItemId::kTitle,
    PopupItemId::kUsernameEntry,
    PopupItemId::kAllSavedPasswordsEntry,
    PopupItemId::kPasswordAccountStorageOptIn,
    PopupItemId::kPasswordAccountStorageReSignin,
    PopupItemId::kPasswordAccountStorageOptInAndGenerate,
    PopupItemId::kPasswordAccountStorageEmpty,
    PopupItemId::kVirtualCreditCardEntry,
};

const std::vector<PopupItemId> kUnclickablePopupItemIds{
    PopupItemId::kInsecureContextPaymentDisabledMessage,
    PopupItemId::kSeparator,
};

bool IsClickable(PopupItemId id) {
  DCHECK(base::Contains(kClickablePopupItemIds, id) ^
         base::Contains(kUnclickablePopupItemIds, id));
  return base::Contains(kClickablePopupItemIds, id);
}

Suggestion CreateSuggestionWithChildren(
    std::vector<Suggestion> children,
    const std::u16string& name = u"Suggestion") {
  Suggestion parent(name);
  parent.popup_item_id = PopupItemId::kAddressEntry;
  parent.children = std::move(children);
  return parent;
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
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    web_contents_->Resize({0, 0, 1024, 768});
    ON_CALL(autofill_popup_controller_, GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
    ON_CALL(autofill_popup_controller_, OpenSubPopup)
        .WillByDefault(Return(autofill_popup_sub_controller_.GetWeakPtr()));

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

  void ShowView(PopupViewViews& view, views::Widget& widget) {
    widget.SetContentsView(&view);
    view.Show(AutoselectFirstSuggestion(false));
  }

  void CreateAndShowView() {
    view_ = std::make_unique<PopupViewViews>(controller().GetWeakPtr());
    ShowView(*view_, *widget_);
  }

  void CreateAndShowView(const std::vector<PopupItemId>& ids) {
    controller().set_suggestions(ids);
    CreateAndShowView();
  }

  void UpdateSuggestions(const std::vector<PopupItemId>& ids) {
    controller().set_suggestions(ids);
    static_cast<AutofillPopupView&>(view()).OnSuggestionsChanged();
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

  // Simulates the keyboard event and returns whether the event was handled.
  bool SimulateKeyPress(int windows_key_code,
                        bool shift_modifier_pressed = false,
                        bool non_shift_modifier_pressed = false) {
    int modifiers = blink::WebInputEvent::kNoModifiers;
    if (shift_modifier_pressed) {
      modifiers |= blink::WebInputEvent::Modifiers::kShiftKey;
    }
    if (non_shift_modifier_pressed) {
      modifiers |= blink::WebInputEvent::Modifiers::kAltKey;
    }

    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown, modifiers,
        ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    return test_api(view()).HandleKeyPressEvent(event);
  }

 protected:
  views::View& GetRowViewAt(size_t index) {
    return *absl::visit([](views::View* view) { return view; },
                        test_api(view()).rows()[index]);
  }

  PopupRowView& GetPopupRowViewAt(size_t index) {
    return *absl::get<PopupRowView*>(test_api(view()).rows()[index]);
  }

  size_t GetNumberOfRows() { return test_api(view()).rows().size(); }

  MockAutofillPopupController& controller() {
    return autofill_popup_controller_;
  }
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupViewViews& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

  std::pair<std::unique_ptr<NiceMock<MockAutofillPopupController>>,
            PopupViewViews*>
  OpenSubView(PopupViewViews& view,
              const std::vector<Suggestion>& suggestions = {
                  Suggestion(u"Suggestion")}) {
    auto sub_controller =
        std::make_unique<NiceMock<MockAutofillPopupController>>();
    sub_controller->set_suggestions(suggestions);
    ON_CALL(*sub_controller, OpenSubPopup)
        .WillByDefault(Return(autofill_popup_sub_controller_.GetWeakPtr()));
    base::WeakPtr<AutofillPopupView> sub_view_ptr =
        view.CreateSubPopupView(sub_controller->GetWeakPtr());
    auto* sub_view = static_cast<PopupViewViews*>(sub_view_ptr.get());
    sub_view->Show(AutoselectFirstSuggestion(false));
    return {std::move(sub_controller), sub_view};
  }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<PopupViewViews> view_;
  NiceMock<MockAutofillPopupController> autofill_popup_controller_;
  NiceMock<MockAutofillPopupController> autofill_popup_sub_controller_;
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
  CreateAndShowView({PopupItemId::kAutocompleteEntry});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  view().Hide();
}

TEST_F(PopupViewViewsTest, CanShowDropdownInBounds) {
  CreateAndShowView({PopupItemId::kAutocompleteEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutofillOptions});

  const int kSingleItemPopupHeight = view().GetPreferredSize().height();
  const int kElementY = 10;
  const int kElementHeight = 15;
  controller().set_element_bounds({10, kElementY, 100, kElementHeight});

  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds({0, 0, 100, 35}));

  // Test a smaller than the popup height (-10px) available space.
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight - 10}));

  // Test a larger than the popup height (+10px) available space.
  EXPECT_TRUE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight + 10}));

  view().Hide();

  // Repeat the same tests as for the single-suggestion popup above,
  // the list is scrollable so that the same restrictions apply.
  CreateAndShowView({PopupItemId::kAutocompleteEntry,
                     PopupItemId::kAutocompleteEntry,
                     PopupItemId::kAutocompleteEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutofillOptions});
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds({0, 0, 100, 35}));
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight - 10}));
  EXPECT_TRUE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight + 10}));
}

// This is a regression test for crbug.com/1113255.
TEST_F(PopupViewViewsTest, ShowViewWithOnlyFooterItemsShouldNotCrash) {
  // Set suggestions to have only a footer item.
  std::vector<PopupItemId> suggestion_ids = {PopupItemId::kClearForm};
  controller().set_suggestions(suggestion_ids);
  CreateAndShowView();
}

TEST_F(PopupViewViewsTest, AccessibilitySelectedEvent) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  CreateAndShowView({PopupItemId::kAutocompleteEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutofillOptions});

  // Checks that a selection event is not sent when the view's |is_selected_|
  // member does not change.
  GetPopupRowViewAt(0).SetSelectedCell(absl::nullopt);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a selection event is sent when an unselected view becomes
  // selected.
  GetPopupRowViewAt(0).SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when the view's
  // |is_selected_| member does not change.
  GetPopupRowViewAt(0).SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when a selected view becomes
  // unselected.
  GetPopupRowViewAt(0).SetSelectedCell(absl::nullopt);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));
}

TEST_F(PopupViewViewsTest, AccessibilityTest) {
  CreateAndShowView({PopupItemId::kDatalistEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutocompleteEntry,
                     PopupItemId::kAutofillOptions});

  // Select first item.
  GetPopupRowViewAt(0).SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_EQ(GetNumberOfRows(), 4u);

  // Item 0.
  ui::AXNodeData node_data_0;
  GetPopupRowViewAt(0).GetContentView().GetAccessibleNodeData(&node_data_0);
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
  GetPopupRowViewAt(2).GetContentView().GetAccessibleNodeData(&node_data_2);
  EXPECT_EQ(2, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_2.role);
  EXPECT_FALSE(
      node_data_2.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 3 (footer).
  ui::AXNodeData node_data_3;
  GetPopupRowViewAt(3).GetContentView().GetAccessibleNodeData(&node_data_3);
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_3.role);
  EXPECT_FALSE(
      node_data_3.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(PopupViewViewsTest, Gestures) {
  CreateAndShowView({PopupItemId::kPasswordEntry, PopupItemId::kSeparator,
                     PopupItemId::kAllSavedPasswordsEntry});

  // Tap down will select an element.
  ui::GestureEvent tap_down_event(
      /*x=*/0, /*y=*/0, /*flags=*/0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>(0u)));
  GetPopupRowViewAt(0).GetContentView().OnGestureEvent(&tap_down_event);

  // Tapping will accept the selection.
  ui::GestureEvent tap_event(/*x=*/0, /*y=*/0, /*flags=*/0,
                             ui::EventTimeForNow(),
                             ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  EXPECT_CALL(controller(), AcceptSuggestion(0, _));
  GetPopupRowViewAt(0).GetContentView().OnGestureEvent(&tap_event);

  // Canceling gesture clears any selection.
  ui::GestureEvent tap_cancel(
      /*x=*/0, /*y=*/0, /*flags=*/0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_CANCEL));
  EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>()));
  GetPopupRowViewAt(2).GetContentView().OnGestureEvent(&tap_cancel);
}

TEST_F(PopupViewViewsTest, ClickDisabledEntry) {
  Suggestion opt_int_suggestion("", "", "",
                                PopupItemId::kPasswordAccountStorageOptIn);
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

TEST_F(PopupViewViewsTest, CursorUpDownForSelectableCells) {
  // Set up the popup.
  CreateAndShowView(
      {PopupItemId::kAutocompleteEntry, PopupItemId::kAutocompleteEntry});

  // By default, no row is selected.
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // Test wrapping before the front.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(1u, CellType::kContent));

  // Test wrapping after the end.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(0u, CellType::kContent));

  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(1u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, LeftAndRightKeyEventsAreHandled) {
  // The control cell is present in suggestions with children.
  controller().set_suggestions(
      {CreateSuggestionWithChildren({Suggestion(u"Child #1")})});
  CreateAndShowView();
  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  // Hitting right again does not do anything.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);
}

TEST_F(PopupViewViewsTest, LeftAndRightKeyEventsAreHandledForRTL) {
  base::i18n::SetRTLForTesting(true);

  // The control cell is present in suggestions with children.
  controller().set_suggestions(
      {CreateSuggestionWithChildren({Suggestion(u"Child #1")})});
  CreateAndShowView();
  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kNonUserInput);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  // Hitting right again does not do anything.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);
}

TEST_F(PopupViewViewsTest, LeftAndRightKeyEventsAreHandledWithoutControl) {
  CreateAndShowView({PopupItemId::kAddressEntry});
  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Hitting right or left does not do anything, since there is only one cell to
  // select.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);
}

TEST_F(PopupViewViewsTest, CursorLeftRightDownForAutocompleteEntries) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillShowAutocompleteDeleteButton};

  // Set up the popup.
  CreateAndShowView(
      {PopupItemId::kAutocompleteEntry, PopupItemId::kAutocompleteEntry});

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Pressing left or right does nothing because the autocomplete cell is
  // handling it itself.
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(0u, CellType::kContent));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(0u, CellType::kContent));

  // Going down selects the next cell.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(1u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, PageUpDownForSelectableCells) {
  // Set up the popup.
  CreateAndShowView(
      {PopupItemId::kAutocompleteEntry, PopupItemId::kAutocompleteEntry,
       PopupItemId::kAutocompleteEntry, PopupItemId::kAutocompleteEntry});

  // Select the third row.
  view().SetSelectedCell(CellIndex{2u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(2u, CellType::kContent));

  // Page up selects the first line.
  SimulateKeyPress(ui::VKEY_PRIOR);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(0u, CellType::kContent));

  // Page down selects the last line.
  SimulateKeyPress(ui::VKEY_NEXT);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(3u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, MovingSelectionSkipsSeparator) {
  CreateAndShowView({PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutofillOptions});
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Going one down skips the separator.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(2u, CellType::kContent));

  // And going up does, too.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(0u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, MovingSelectionSkipsInsecureFormWarning) {
  CreateAndShowView({PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                     PopupItemId::kInsecureContextPaymentDisabledMessage});
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Cursor up skips the unselectable form warning when the last item cannot be
  // selected.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // Cursor down selects the first element.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(0u, CellType::kContent));

  // Cursor up leads to no selection because the last item cannot be selected.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_FALSE(view().GetSelectedCell());
}

TEST_F(PopupViewViewsTest, EscClosesSubPopup) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();

  CellIndex cell_content = CellIndex{0, CellType::kContent};
  CellIndex cell_control = CellIndex{0, CellType::kControl};

  view().SetSelectedCell(cell_control, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupCell(), cell_control);

  SimulateKeyPress(ui::VKEY_ESCAPE);
  EXPECT_EQ(view().GetSelectedCell(), cell_content);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupCell(), absl::nullopt);
}

class PopupViewViewsTestKeyboard : public PopupViewViewsTest {
 public:
  void SelectItem(size_t index) {
    CreateAndShowView(
        {PopupItemId::kAddressEntry, PopupItemId::kAutofillOptions});
    // Select the `index`th item.
    view().SetSelectedCell(CellIndex{index, CellType::kContent},
                           PopupCellSelectionSource::kNonUserInput);
    EXPECT_EQ(view().GetSelectedCell(),
              absl::make_optional<CellIndex>(index, CellType::kContent));
  }

  void SelectFirstSuggestion() { SelectItem(0); }
};

// Tests that hitting enter on a suggestion autofills it.
TEST_F(PopupViewViewsTestKeyboard, FillOnEnter) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(), AcceptSuggestion(0, _));
  SimulateKeyPress(ui::VKEY_RETURN);
}

// Tests that hitting tab on a suggestion autofills it.
TEST_F(PopupViewViewsTestKeyboard, FillOnTabPressed) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(), AcceptSuggestion(0, _));
  SimulateKeyPress(ui::VKEY_TAB);
}

// Tests that `tab` together with a modified (other than shift) does not
// autofill a selected suggestion.
TEST_F(PopupViewViewsTestKeyboard, NoFillOnTabPressedWithModifiers) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(), AcceptSuggestion(0, _)).Times(0);
  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/false,
                   /*non_shift_modifier_pressed=*/true);
}

// Verify that pressing the tab key while the "Manage addresses..." entry is
// selected does not trigger "accepting" the entry (which would mean opening
// a tab with the autofill settings).
TEST_F(PopupViewViewsTest, NoAutofillOptionsTriggeredOnTabPressed) {
  // Set up the popup and select the options cell.
  CreateAndShowView({PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutofillOptions});
  view().SetSelectedCell(CellIndex{2u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(2u, CellType::kContent));

  // Because the selected line is PopupItemId::kAutofillOptions, we expect that
  // the tab key does not trigger anything.
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  SimulateKeyPress(ui::VKEY_TAB);
}

// This is a regression test for crbug.com/1309431 to ensure that we don't crash
// when we press tab before a line is selected.
TEST_F(PopupViewViewsTest, TabBeforeSelectingALine) {
  CreateAndShowView({PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutofillOptions});
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // The following should not crash:
  SimulateKeyPress(ui::VKEY_TAB);
}

TEST_F(PopupViewViewsTest, RemoveLine) {
  CreateAndShowView({PopupItemId::kAddressEntry, PopupItemId::kAddressEntry,
                     PopupItemId::kAutofillOptions});

  // If no cell is selected, pressing delete has no effect.
  EXPECT_FALSE(view().GetSelectedCell().has_value());
  EXPECT_CALL(controller(), RemoveSuggestion).Times(0);
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
  Mock::VerifyAndClearExpectations(&controller());

  view().SetSelectedCell(CellIndex{1u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(view().GetSelectedCell(),
            absl::make_optional<CellIndex>(1u, CellType::kContent));

  EXPECT_CALL(controller(), RemoveSuggestion).Times(0);
  // If no shift key is pressed, no suggestion is removed.
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/false);
  Mock::VerifyAndClearExpectations(&controller());

  EXPECT_CALL(controller(), RemoveSuggestion(1));
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
}

TEST_F(PopupViewViewsTest, RemoveAutofillRecordsNoAutocompleteDeletionMetrics) {
  base::HistogramTester histogram_tester;
  CreateAndShowView({PopupItemId::kAddressEntry, PopupItemId::kAddressEntry,
                     PopupItemId::kAutofillOptions});

  view().SetSelectedCell(CellIndex{1u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // No metrics are recorded if the entry is not an Autocomplete entry.
  EXPECT_CALL(controller(), RemoveSuggestion(1)).WillOnce(Return(true));
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.SingleEntryRemovalMethod", 0);
  histogram_tester.ExpectTotalCount("Autocomplete.Events", 0);
}

TEST_F(PopupViewViewsTest, RemoveAutocompleteSuggestionRecordsMetrics) {
  base::HistogramTester histogram_tester;
  CreateAndShowView(
      {PopupItemId::kAutocompleteEntry, PopupItemId::kAutocompleteEntry});

  view().SetSelectedCell(CellIndex{1u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // If deletion fails, no metric is recorded.
  EXPECT_CALL(controller(), RemoveSuggestion(1)).WillOnce(Return(false));
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.SingleEntryRemovalMethod", 0);
  histogram_tester.ExpectTotalCount("Autocomplete.Events", 0);

  EXPECT_CALL(controller(), RemoveSuggestion(1)).WillOnce(Return(true));
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
          kKeyboardShiftDeletePressed,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 1);
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
  suggestion.popup_item_id = PopupItemId::kUsernameEntry;

  // Create autofill menu.
  controller().set_suggestions({suggestion});

  CreateAndShowView();

  // Verify that the accessibility layer gets the right string to read out.
  ui::AXNodeData node_data;
  GetPopupRowViewAt(0).GetContentView().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(voice_over_value,
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PopupViewViewsTest, ExpandableSuggestionA11yMessageTest) {
  // Set up the popup with suggestions.
  std::u16string address_line = u"Address line #1";
  Suggestion suggestion(address_line, PopupItemId::kAddressEntry);
  suggestion.children = {Suggestion(PopupItemId::kFillFullAddress),
                         Suggestion(PopupItemId::kFillFullName)};
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  // Verify that the accessibility layer gets the right string to read out.
  ui::AXNodeData node_data;
  GetPopupRowViewAt(0).GetContentView().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      base::JoinString(
          {address_line,
           l10n_util::GetStringUTF16(
               IDS_AUTOFILL_EXPANDABLE_SUGGESTION_FULL_ADDRESS_A11Y_ADDON),
           l10n_util::GetStringFUTF16(
               IDS_AUTOFILL_EXPANDABLE_SUGGESTION_SUBMENU_HINT,
               l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT))},
          u". "));
}

TEST_F(PopupViewViewsTest, UpdateSuggestionsNoCrash) {
  CreateAndShowView({PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                     PopupItemId::kAutofillOptions});
  UpdateSuggestions({PopupItemId::kAddressEntry});
}

TEST_F(PopupViewViewsTest, SubViewIsShownInChildWidget) {
  CreateAndShowView({PopupItemId::kAddressEntry});
  auto [sub_controller, sub_view] = OpenSubView(view());
  views::Widget* sub_widget = sub_view->GetWidget();

  EXPECT_EQ(view().GetWidget(), sub_widget->parent());
}

// Tests the event retriggering trick from `PopupBaseView::Widget`,
// see `PopupBaseView::Widget::OnMouseEvent()` for details.
TEST_F(PopupViewViewsTest, ChildWidgetRetriggersMouseMovesToParent) {
  // The synthetic events further down originate directly inside the view
  // bounds, this flag prevents ignoring them.
  ON_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck)
      .WillByDefault(Return(true));

  CreateAndShowView({PopupItemId::kAddressEntry});
  auto [sub_controller, sub_view] = OpenSubView(view());

  ASSERT_EQ(view().GetSelectedCell(), absl::nullopt);

  PopupRowView* row = absl::get<PopupRowView*>(test_api(view()).rows()[0]);

  // Mouse move inside parent, selection by MOUSE_ENTERED is expected.
  generator().MoveMouseTo(row->GetBoundsInScreen().CenterPoint());
  EXPECT_NE(view().GetSelectedCell(), absl::nullopt);

  // Mouse move outside parent, unselection by MOUSE_EXITED is expected.
  generator().MoveMouseTo(row->GetBoundsInScreen().origin() -
                          gfx::Vector2d(100, 100));
  EXPECT_EQ(view().GetSelectedCell(), absl::nullopt);
}

TEST_F(PopupViewViewsTest, SubViewIsClosedWithParent) {
  controller().set_suggestions({PopupItemId::kAddressEntry});
  PopupViewViews view(controller().GetWeakPtr());
  views::Widget* widget = CreateTestWidget().release();
  ShowView(view, *widget);

  auto [sub_controller, sub_view] = OpenSubView(view);
  base::WeakPtr<views::Widget> sub_widget = sub_view->GetWidget()->GetWeakPtr();

  ASSERT_FALSE(sub_widget->IsClosed());

  EXPECT_CALL(controller(), ViewDestroyed());

  widget->CloseNow();

  EXPECT_TRUE(!sub_widget || sub_widget->IsClosed())
      << "The sub-widget should be closed as its parent is closed.";
}

TEST_F(PopupViewViewsTest, CellOpensClosesSubPopupWithDelay) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();

  CellIndex cell_0 = CellIndex{0, CellType::kControl};

  view().SetSelectedCell(cell_0, PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupCell(), absl::nullopt)
      << "Should be no sub-popups initially.";

  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupCell(), cell_0)
      << "Selected cell should have a sub-popup after the delay.";

  view().SetSelectedCell(absl::nullopt,
                         PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupCell(), cell_0)
      << "The cell should have no sub-popup by unselecting it.";
}

TEST_F(PopupViewViewsTest, CellSubPopupResetAfterSuggestionsUpdates) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_NE(test_api(view()).GetOpenSubPopupCell(), absl::nullopt)
      << "Openning a sub-popup should happen.";

  UpdateSuggestions({PopupItemId::kAddressEntry});
  EXPECT_EQ(test_api(view()).GetOpenSubPopupCell(), absl::nullopt)
      << "The cell's sub-popup should be closed.";
}

TEST_F(PopupViewViewsTest, NoSubPopupOpenIfNotEligible) {
  controller().set_suggestions({
      // Regular suggestion with no children,
      Suggestion(u"Suggestion #1"),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupCell(), absl::nullopt)
      << "Opening a sub-popup should happen.";
}

TEST_F(PopupViewViewsTest, SubPopupHidingOnNoSelection) {
  ui::MouseEvent fake_event(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                            ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0);
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupCell(), cell);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren({Suggestion(u"Sub Child #1")})});
  view().SetSelectedCell(absl::nullopt,
                         PopupCellSelectionSource::kNonUserInput);
  sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(*sub_view).GetOpenSubPopupCell(), cell);

  auto [sub_sub_controller, sub_sub_view] = OpenSubView(
      *sub_view,
      {CreateSuggestionWithChildren({Suggestion(u"Sub Sub Child #1")})});
  sub_view->SetSelectedCell(absl::nullopt,
                            PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(absl::nullopt,
                                PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->OnMouseExited(fake_event);

  task_environment()->FastForwardBy(
      PopupViewViews::kNoSelectionHideSubPopupDelay);

  EXPECT_EQ(test_api(view()).GetOpenSubPopupCell(), absl::nullopt);
  EXPECT_EQ(test_api(*sub_view).GetOpenSubPopupCell(), absl::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupOwnSelectionPreventsHiding) {
  ui::MouseEvent fake_event(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                            ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0);
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupCell(), cell);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren({Suggestion(u"Sub Child #1")})});
  view().SetSelectedCell(absl::nullopt,
                         PopupCellSelectionSource::kNonUserInput);
  sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(*sub_view).GetOpenSubPopupCell(), cell);

  auto [sub_sub_controller, sub_sub_view] = OpenSubView(
      *sub_view,
      {CreateSuggestionWithChildren({Suggestion(u"Sub Sub Child #1")})});
  sub_view->SetSelectedCell(absl::nullopt,
                            PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(absl::nullopt,
                                PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->OnMouseExited(fake_event);

  // The interrupting selection in the root popup, should prevent
  // its sub-popup from closing, but not the middle one's sub-popup.
  task_environment()->FastForwardBy(base::Milliseconds(1));
  view().OnMouseEntered(fake_event);

  task_environment()->FastForwardBy(
      PopupViewViews::kNoSelectionHideSubPopupDelay);

  EXPECT_NE(test_api(view()).GetOpenSubPopupCell(), absl::nullopt);
  EXPECT_EQ(test_api(*sub_view).GetOpenSubPopupCell(), absl::nullopt);
}

#if defined(MEMORY_SANITIZER) && BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShowClickTest DISABLED_ShowClickTest
#else
#define MAYBE_ShowClickTest ShowClickTest
#endif
// Tests that (only) clickable items trigger an AcceptSuggestion event.
TEST_P(PopupViewViewsTestWithAnyPopupItemId, MAYBE_ShowClickTest) {
  CreateAndShowView({popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion(0, _))
      .Times(IsClickable(popup_item_id()));
  generator().MoveMouseTo(gfx::Point(1000, 1000));
  ASSERT_FALSE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
}

// Tests that after the mouse moves into the popup after display, clicking a
// suggestion triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickablePopupItemId,
       AcceptSuggestionIfUnfocusedAtPaint) {
  CreateAndShowView({popup_item_id()});
  EXPECT_CALL(controller(), AcceptSuggestion(0, _)).Times(1);
  generator().MoveMouseTo(gfx::Point(1000, 1000));
  ASSERT_FALSE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
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
