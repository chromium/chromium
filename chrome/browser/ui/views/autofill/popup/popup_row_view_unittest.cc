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
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"
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
  MOCK_METHOD(void, SetSelectedCell, (absl::optional<CellIndex>), (override));
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
    row_view_ = widget_->SetContentsView(std::make_unique<PopupRowView>(
        mock_a11y_selection_delegate_, mock_selection_delegate_,
        /*controller=*/nullptr,
        std::make_unique<TestPopupRowStrategy>(line_number, has_control)));
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

  void SimulateKeyPress(int windows_key_code) {
    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    row_view().HandleKeyPressEvent(event);
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
  PopupRowView& row_view() { return *row_view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  raw_ptr<PopupRowView> row_view_ = nullptr;
};

TEST_F(PopupRowViewTest, MouseEnterExitInformsSelectionDelegate) {
  ShowView(2, /*has_control=*/true);

  // Move the mouse of out bounds and force paint to satisfy the check that the
  // mouse has been outside the element before enter/exit events are passed on.
  generator().MoveMouseTo(kOutOfBounds);
  Paint();

  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(absl::make_optional<CellIndex>(2u, CellType::kContent)));
  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());

  // Moving from one cell to another triggers two events, one with
  // `absl::nullopt` as argument and the other with the control cell.
  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(absl::optional<CellIndex>()));
  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(absl::make_optional<CellIndex>(2u, CellType::kControl)));
  ASSERT_TRUE(row_view().GetControlView());
  generator().MoveMouseTo(
      row_view().GetControlView()->GetBoundsInScreen().CenterPoint());

  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(absl::optional<CellIndex>()));
  generator().MoveMouseTo(kOutOfBounds);
}

TEST_F(PopupRowViewTest, SetSelectedCellVerifiesArgumentsNoControl) {
  ShowView(0, /*has_control=*/false);
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
  ShowView(0, /*has_control=*/true);
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

TEST_F(PopupRowViewTest, LeftAndRightKeyEventsAreHandled) {
  ShowView(0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetControlView());
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(*row_view().GetControlView())));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kControl);

  // Hitting right again does not do anything.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kControl);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kContent);
}

TEST_F(PopupRowViewTest, LeftAndRightKeyEventsAreHandledForRTL) {
  base::i18n::SetRTLForTesting(true);
  ShowView(0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetControlView());
  row_view().SetSelectedCell(CellType::kControl);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kContent);

  // Hitting right again does not do anything.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(*row_view().GetControlView())));
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kControl);

  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kControl);
}

TEST_F(PopupRowViewTest, LeftAndRightKeyEventsAreHandledWithoutControl) {
  ShowView(0, /*has_control=*/false);
  ASSERT_FALSE(row_view().GetControlView());
  row_view().SetSelectedCell(CellType::kContent);

  // Hitting right or left does not do anything, since there is only one cell to
  // select.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kContent);
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(*row_view().GetSelectedCell(), CellType::kContent);
}

TEST_F(PopupRowViewTest, ReturnKeyEventsAreHandled) {
  ShowView(0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetControlView());
  row_view().SetSelectedCell(CellType::kContent);

  StrictMock<base::MockCallback<PopupCellView::OnAcceptedCallback>>
      content_callback;
  StrictMock<base::MockCallback<PopupCellView::OnAcceptedCallback>>
      control_callback;

  row_view().GetContentView().SetOnAcceptedCallback(content_callback.Get());
  row_view().GetControlView()->SetOnAcceptedCallback(control_callback.Get());

  EXPECT_CALL(content_callback, Run);
  SimulateKeyPress(ui::VKEY_RETURN);
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_CALL(control_callback, Run);
  SimulateKeyPress(ui::VKEY_RETURN);
}

}  // namespace autofill
