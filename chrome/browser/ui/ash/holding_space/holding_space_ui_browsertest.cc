// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/test/bind_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Performs a mouse drag between `from` and `to`.
void MouseDrag(const views::View* from, const views::View* to) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(from->GetBoundsInScreen().CenterPoint());
  event_generator.PressLeftButton();
  event_generator.MoveMouseTo(to->GetBoundsInScreen().CenterPoint());
  event_generator.ReleaseLeftButton();
}

// DropTargetView --------------------------------------------------------------

class DropTargetView : public views::WidgetDelegateView {
 public:
  DropTargetView(const DropTargetView&) = delete;
  DropTargetView& operator=(const DropTargetView&) = delete;
  ~DropTargetView() override = default;

  static DropTargetView* Create(aura::Window* context) {
    return new DropTargetView(context);
  }

  const base::FilePath& copied_file_path() const { return copied_file_path_; }

 private:
  explicit DropTargetView(aura::Window* context) { InitWidget(context); }

  // views::WidgetDelegateView:
  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    *formats = ui::OSExchangeData::FILE_NAME;
    return true;
  }

  bool CanDrop(const ui::OSExchangeData& data) override { return true; }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_COPY;
  }

  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    EXPECT_TRUE(event.data().GetFilename(&copied_file_path_));
    return ui::DragDropTypes::DRAG_COPY;
  }

  void InitWidget(aura::Window* context) {
    views::Widget::InitParams params;
    params.accept_events = true;
    params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
    params.context = context;
    params.delegate = this;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.wants_mouse_events_when_inactive = true;

    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));
  }

  base::FilePath copied_file_path_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceUiBrowserTest = HoldingSpaceBrowserTestBase;

// Verifies that drag-and-drop of holding space items works.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, DragAndDrop) {
  HoldingSpaceItem* const pinned_file = AddPinnedFile();
  HoldingSpaceItem* const screenshot_file = AddScreenshotFile();

  Show();
  ASSERT_TRUE(IsShowing());

  std::vector<views::View*> chips = GetPinnedFileChips();
  ASSERT_EQ(1u, chips.size());

  std::vector<views::View*> screenshots = GetScreenshotViews();
  ASSERT_EQ(1u, screenshots.size());

  auto* drop_target_view = DropTargetView::Create(GetRootWindowForNewWindows());
  drop_target_view->GetWidget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  drop_target_view->GetWidget()->ShowInactive();

  MouseDrag(/*from=*/chips[0], /*to=*/drop_target_view);
  EXPECT_EQ(pinned_file->file_path(), drop_target_view->copied_file_path());

  MouseDrag(/*from=*/screenshots[0], /*to=*/drop_target_view);
  EXPECT_EQ(screenshot_file->file_path(), drop_target_view->copied_file_path());

  drop_target_view->GetWidget()->Close();
}

}  // namespace ash
