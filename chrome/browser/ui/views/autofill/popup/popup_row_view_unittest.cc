// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::StrictMock;
using CellType = PopupRowView::CellType;
using CellIndex = PopupRowView::SelectionDelegate::CellIndex;

constexpr gfx::Point kOutOfBounds{1000, 1000};

}  // namespace

class PopupRowViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    ON_CALL(mock_controller_, GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
  }

  void ShowView(int line_number, bool has_control) {
    std::vector<Suggestion> suggestions(line_number + 1);
    suggestions[line_number].popup_item_id = PopupItemId::kAddressEntry;
    suggestions[line_number].main_text = Suggestion::Text(u"Suggestion");
    if (has_control) {
      suggestions[line_number].children = {Suggestion()};
    }
    ShowView(line_number, std::move(suggestions));
  }

  void ShowView(int line_number, std::vector<Suggestion> suggestions) {
    mock_controller_.set_suggestions(suggestions);
    ShowView(line_number);
  }

  void ShowView(int line_number, std::vector<PopupItemId> suggestions) {
    mock_controller_.set_suggestions(suggestions);
    ShowView(line_number);
  }

  void ShowView(int line_number) {
    row_view_ = widget_->SetContentsView(CreatePopupRowView(
        mock_controller_.GetWeakPtr(), mock_a11y_selection_delegate_,
        mock_selection_delegate_, line_number));
    ON_CALL(mock_selection_delegate_, SetSelectedCell)
        .WillByDefault([this](absl::optional<CellIndex> cell,
                              PopupCellSelectionSource) {
          row_view().SetSelectedCell(
              cell ? absl::optional<CellType>{cell->second} : absl::nullopt);
        });
    widget_->Show();
  }

  void TearDown() override {
    row_view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void Paint() {
    views::View& paint_view = *widget().GetRootView();
    SkBitmap bitmap;
    gfx::Size size = paint_view.size();
    ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                     false);
    paint_view.Paint(
        views::PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
  }

  // Simulates the keyboard event and returns whether the event was handled.
  bool SimulateKeyPress(int windows_key_code) {
    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    return row_view().HandleKeyPressEvent(event);
  }

 protected:
  ui::test::EventGenerator& generator() { return *generator_; }
  views::Widget& widget() { return *widget_; }
  MockAccessibilitySelectionDelegate& a11y_selection_delegate() {
    return mock_a11y_selection_delegate_;
  }
  MockSelectionDelegate& selection_delegate() {
    return mock_selection_delegate_;
  }
  MockAutofillPopupController& controller() { return mock_controller_; }
  PopupRowView& row_view() { return *row_view_; }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  NiceMock<MockAutofillPopupController> mock_controller_;
  raw_ptr<PopupRowView> row_view_ = nullptr;
};

TEST_F(PopupRowViewTest, MouseEnterExitInformsSelectionDelegate) {
  ShowView(/*line_number=*/2, /*has_control=*/true);

  // Move the mouse of out bounds and force paint to satisfy the check that the
  // mouse has been outside the element before enter/exit events are passed on.
  generator().MoveMouseTo(kOutOfBounds);
  Paint();

  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(absl::make_optional<CellIndex>(2u, CellType::kContent),
                      PopupCellSelectionSource::kMouse));
  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());

  // Moving from one cell to another triggers two events, one with
  // `absl::nullopt` as argument and the other with the control cell.
  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(absl::optional<CellIndex>(),
                              PopupCellSelectionSource::kMouse));
  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(absl::make_optional<CellIndex>(2u, CellType::kControl),
                      PopupCellSelectionSource::kMouse));
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  generator().MoveMouseTo(row_view()
                              .GetExpandChildSuggestionsView()
                              ->GetBoundsInScreen()
                              .CenterPoint());

  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(absl::optional<CellIndex>(),
                              PopupCellSelectionSource::kMouse));
  generator().MoveMouseTo(kOutOfBounds);
}

// Gestures are not supported on MacOS.
#if !BUILDFLAG(IS_MAC)
TEST_F(PopupRowViewTest, GestureEvents) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(absl::make_optional<CellIndex>(0u, CellType::kContent),
                      PopupCellSelectionSource::kMouse));
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().GestureTapAt(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
}

TEST_F(PopupRowViewTest, NoCrashOnGestureAcceptingWithInvalidatedController) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  controller().InvalidateWeakPtrs();
  generator().GestureTapAt(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(PopupRowViewTest, SetSelectedCellVerifiesArgumentsNoControl) {
  ShowView(/*line_number=*/0, /*has_control=*/false);
  EXPECT_FALSE(row_view().GetExpandChildSuggestionsView());
  EXPECT_FALSE(row_view().GetSelectedCell().has_value());

  // Selecting the content cell notifies the accessibility system that the
  // respective view has been selected.
  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));
  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_EQ(row_view().GetSelectedCell(),
            absl::make_optional<CellType>(CellType::kContent));

  // Selecting it again leads to no notification.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_EQ(row_view().GetSelectedCell(),
            absl::make_optional<CellType>(CellType::kContent));

  // Setting the cell type to control leads to no selected cell when there is no
  // control surface.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_FALSE(row_view().GetSelectedCell().has_value());
}

TEST_F(PopupRowViewTest, SetSelectedCellVerifiesArgumentsWithControl) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  EXPECT_FALSE(row_view().GetSelectedCell().has_value());

  base::MockRepeatingCallback<void(const ui::AXPlatformNodeDelegate*,
                                   const ax::mojom::Event)>
      a11y_callback;

  row_view()
      .GetExpandChildSuggestionsView()
      ->GetViewAccessibility()
      .set_accessibility_events_callback(a11y_callback.Get());

  // Selecting the control cell notifies the accessibility system that the
  // respective view has been selected.
  EXPECT_CALL(a11y_callback, Run(_, ax::mojom::Event::kFocus));
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_EQ(row_view().GetSelectedCell(),
            absl::make_optional<CellType>(CellType::kControl));

  // Selecting it again leads to no notification.
  EXPECT_CALL(a11y_callback, Run).Times(0);
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_EQ(row_view().GetSelectedCell(),
            absl::make_optional<CellType>(CellType::kControl));
}

TEST_F(PopupRowViewTest, SetSelectedCellTriggersController) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  ASSERT_FALSE(row_view().GetSelectedCell().has_value());

  EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>(0)));
  row_view().SetSelectedCell(CellType::kContent);

  // No selection triggering if trying to set already selected content.
  EXPECT_CALL(controller(), SelectSuggestion).Times(0);
  row_view().SetSelectedCell(CellType::kContent);

  // Deselection of selected content.
  EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>()));
  row_view().SetSelectedCell(CellType::kControl);

  EXPECT_CALL(controller(), SelectSuggestion(absl::optional<size_t>(0)));
  row_view().SetSelectedCell(CellType::kContent);
}

TEST_F(PopupRowViewTest, NotifyAXSelectionCalledOnChangesOnly) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  row_view().SetSelectedCell(CellType::kContent);

  base::MockRepeatingCallback<void(const ui::AXPlatformNodeDelegate*,
                                   const ax::mojom::Event)>
      a11y_callback;

  row_view()
      .GetExpandChildSuggestionsView()
      ->GetViewAccessibility()
      .set_accessibility_events_callback(a11y_callback.Get());

  EXPECT_CALL(a11y_callback, Run(_, ax::mojom::Event::kFocus));
  row_view().SetSelectedCell(CellType::kControl);

  // Hitting right again does not do anything.
  EXPECT_CALL(a11y_callback, Run).Times(0);
  row_view().SetSelectedCell(CellType::kControl);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kContent);
}

TEST_F(PopupRowViewTest, ReturnKeyEventsAreHandled) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());

  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_CALL(controller(), AcceptSuggestion);
  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RETURN));

  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RETURN));

  row_view().SetSelectedCell(CellType::kContent);
  controller().InvalidateWeakPtrs();
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RETURN));
}

TEST_F(PopupRowViewTest,
       ShouldIgnoreMouseObservedOutsideItemBoundsCheckIsFalse_IgnoreClick) {
  ShowView(/*line_number=*/0, /*has_control=*/false);

  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  Paint();
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  generator().ClickLeftButton();

  generator().MoveMouseTo(kOutOfBounds);
  Paint();
  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  // If the mouse has been outside before, the accept click is passed through.
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().ClickLeftButton();
}

TEST_F(PopupRowViewTest,
       ShouldIgnoreMouseObservedOutsideItemBoundsCheckIsTrue_DoNotIgnoreClick) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  Paint();
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().ClickLeftButton();
}

TEST_F(PopupRowViewTest, NoCrashOnMouseAcceptingWithInvalidatedController) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  controller().InvalidateWeakPtrs();
  generator().ClickLeftButton();
}

TEST_F(PopupRowViewTest, SelectSuggestionOnFocusedContent) {
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(absl::make_optional<CellIndex>(0u, CellType::kContent),
                      PopupCellSelectionSource::kKeyboard));

  row_view().GetContentView().RequestFocus();
}

TEST_F(PopupRowViewTest, ContentViewA11yAttributes) {
  ShowView(/*line_number=*/0,
           {Suggestion("dummy_value", "dummy_label", Suggestion::Icon::kNoIcon,
                       PopupItemId::kAddressEntry)});

  views::ViewAccessibility& accessibility =
      row_view().GetContentView().GetViewAccessibility();

  ui::AXNodeData node_data;
  accessibility.GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kListBoxOption);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "dummy_value dummy_label");
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet), 1);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize), 1);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

struct PosInSetTestdata {
  // The popup item ids of the suggestions to be shown.
  std::vector<PopupItemId> popup_item_ids;
  // The index of the suggestion to be tested.
  int line_number;
  // The number of (non-separator) entries and the 1-indexed position of the
  // entry with `line_number` inside them.
  int set_size;
  int set_index;
};

const PosInSetTestdata kPosInSetTestcases[] = {
    PosInSetTestdata{
        .popup_item_ids = {PopupItemId::kAddressEntry,
                           PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                           PopupItemId::kAutofillOptions},
        .line_number = 1,
        .set_size = 3,
        .set_index = 2,
    },
    PosInSetTestdata{
        .popup_item_ids = {PopupItemId::kPasswordEntry,
                           PopupItemId::kAccountStoragePasswordEntry,
                           PopupItemId::kSeparator,
                           PopupItemId::kAllSavedPasswordsEntry},
        .line_number = 0,
        .set_size = 3,
        .set_index = 1,
    },
    PosInSetTestdata{
        .popup_item_ids = {PopupItemId::kAddressEntry,
                           PopupItemId::kAddressEntry, PopupItemId::kSeparator,
                           PopupItemId::kAutofillOptions},
        .line_number = 3,
        .set_size = 3,
        .set_index = 3,
    },
    PosInSetTestdata{
        .popup_item_ids = {PopupItemId::kAutocompleteEntry,
                           PopupItemId::kAutocompleteEntry,
                           PopupItemId::kAutocompleteEntry},
        .line_number = 1,
        .set_size = 3,
        .set_index = 2,
    },
    PosInSetTestdata{
        .popup_item_ids = {PopupItemId::kCompose},
        .line_number = 0,
        .set_size = 1,
        .set_index = 1,
    }};

class PopupRowPosInSetViewTest
    : public PopupRowViewTest,
      public ::testing::WithParamInterface<PosInSetTestdata> {};

TEST_P(PopupRowPosInSetViewTest, All) {
  const PosInSetTestdata kTestdata = GetParam();

  ShowView(kTestdata.line_number, kTestdata.popup_item_ids);

  ui::AXNodeData node_data;
  row_view().GetContentView().GetViewAccessibility().GetAccessibleNodeData(
      &node_data);

  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            kTestdata.set_size);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            kTestdata.set_index);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupRowPosInSetViewTest,
                         ::testing::ValuesIn(kPosInSetTestcases));

TEST_F(PopupRowViewTest, ChildSuggestionsExpandingControlA11yCheckedState) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());

  ui::AXNodeData node_data;
  row_view().GetExpandChildSuggestionsView()->GetAccessibleNodeData(&node_data);
  ASSERT_EQ(node_data.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  row_view().SetChildSuggestionsDisplayed(true);
  row_view().GetExpandChildSuggestionsView()->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetCheckedState(), ax::mojom::CheckedState::kTrue);
}

}  // namespace autofill
