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
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/test_popup_row_strategy.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
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

class MockAccessibilitySelectionDelegate
    : public PopupRowView::AccessibilitySelectionDelegate {
 public:
  MOCK_METHOD(void, NotifyAXSelection, (views::View&), (override));
};

class MockSelectionDelegate : public PopupRowView::SelectionDelegate {
 public:
  MOCK_METHOD(absl::optional<CellIndex>, GetSelectedCell, (), (const override));
  MOCK_METHOD(void,
              SetSelectedCell,
              (absl::optional<CellIndex>, PopupCellSelectionSource),
              (override));
};

class MockPopupCellView : public PopupCellView {
 public:
  MOCK_METHOD(bool,
              HandleKeyPressEvent,
              (const content::NativeWebKeyboardEvent& event),
              (override));
};

class MockingTestPopupRowStrategy : public TestPopupRowStrategy {
 public:
  MockingTestPopupRowStrategy(int line_number, bool has_control)
      : TestPopupRowStrategy(line_number, has_control) {}
  ~MockingTestPopupRowStrategy() override = default;

  std::unique_ptr<PopupCellView> CreateContent() override {
    auto content_cell = std::make_unique<NiceMock<MockPopupCellView>>();
    last_created_mock_content_cell_ = content_cell.get();
    return content_cell;
  }

  std::unique_ptr<PopupCellView> CreateControl() override {
    return std::make_unique<NiceMock<MockPopupCellView>>();
  }

  MockPopupCellView* last_created_content_cell() {
    return last_created_mock_content_cell_;
  }

 private:
  raw_ptr<MockPopupCellView> last_created_mock_content_cell_ = nullptr;
};

}  // namespace

class PopupRowViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void ShowView(int line_number, bool has_control) {
    ShowView(std::make_unique<TestPopupRowStrategy>(line_number, has_control));
  }

  void ShowView(std::unique_ptr<TestPopupRowStrategy> strategy) {
    row_view_ = widget_->SetContentsView(std::make_unique<PopupRowView>(
        mock_a11y_selection_delegate_, mock_selection_delegate_,
        mock_controller_.GetWeakPtr(), strategy->GetLineNumber(),
        std::move(strategy)));
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
  ASSERT_TRUE(row_view().GetControlView());
  generator().MoveMouseTo(
      row_view().GetControlView()->GetBoundsInScreen().CenterPoint());

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
  EXPECT_FALSE(row_view().GetControlView());
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
  ASSERT_TRUE(row_view().GetControlView());
  EXPECT_FALSE(row_view().GetSelectedCell().has_value());

  // Selecting the control cell notifies the accessibility system that the
  // respective view has been selected.
  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(*row_view().GetControlView())));
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_EQ(row_view().GetSelectedCell(),
            absl::make_optional<CellType>(CellType::kControl));

  // Selecting it again leads to no notification.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_EQ(row_view().GetSelectedCell(),
            absl::make_optional<CellType>(CellType::kControl));
}

TEST_F(PopupRowViewTest, SetSelectedCellTriggersController) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetControlView());
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
  ASSERT_TRUE(row_view().GetControlView());
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(*row_view().GetControlView())));
  row_view().SetSelectedCell(CellType::kControl);

  // Hitting right again does not do anything.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kControl);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kContent);
}

TEST_F(PopupRowViewTest, ReturnKeyEventsAreHandled) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetControlView());

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

TEST_F(PopupRowViewTest, KeyboardEventsArePassedToControlCell) {
  auto strategy = std::make_unique<MockingTestPopupRowStrategy>(0, true);
  MockingTestPopupRowStrategy* strategy_ref = strategy.get();
  ShowView(std::move(strategy));
  row_view().SetSelectedCell(CellType::kContent);

  ASSERT_TRUE(strategy_ref->last_created_content_cell());

  EXPECT_CALL(*strategy_ref->last_created_content_cell(), HandleKeyPressEvent)
      .WillOnce(Return(true));
  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));

  EXPECT_CALL(*strategy_ref->last_created_content_cell(), HandleKeyPressEvent)
      .WillOnce(Return(false));
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
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
}  // namespace autofill
