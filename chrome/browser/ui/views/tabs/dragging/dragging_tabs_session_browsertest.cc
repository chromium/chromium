// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragging/dragging_tabs_session.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/view.h"

class DraggingTabsSessionBrowserTest : public InProcessBrowserTest {
 public:
  DraggingTabsSessionBrowserTest() = default;
  ~DraggingTabsSessionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    model_ = browser()->GetTabStripModel();
    view_ = browser()->GetBrowserView().tab_strip_view();
  }

  void TearDownOnMainThread() override {
    view_->GetDragContext()->StoppedDragging();
    model_ = nullptr;
    view_ = nullptr;
  }

 protected:
  std::tuple<tabs::TabInterface*, Tab*> AddTab(int index, bool foreground) {
    chrome::AddTabAt(browser(), GURL("about:blank"), index, foreground);
    view_->StopAnimating();
    return std::make_tuple(model_->GetTabAtIndex(index),
                           view_->GetTabAnchorViewAt(index));
  }

  // Sets up model and view state, and populates a DragSessionData, to drag the
  // tabs at `tab_indices`. The tab at `tab_indices[source_index]` is the
  // source view for the drag session. Mirrors TabDragController::AttachImpl().
  // TODO(382754501): Extend this to work with header drags.
  DragSessionData StartDragging(std::vector<int> tab_indices,
                                int source_index) {
    ui::ListSelectionModel selection;
    selection.SetSelectedIndex(tab_indices[source_index]);
    for (int tab_index : tab_indices) {
      selection.AddIndexToSelection(tab_index);
    }
    model_->SetSelectionFromModel(selection);

    DragSessionData drag_data;
    for (int tab_index : tab_indices) {
      Tab* const tab_view = view_->GetTabAnchorViewAt(tab_index);
      drag_data.tab_drag_data_.emplace_back(view_->GetDragContext(), tab_view);
      drag_data.tab_drag_data_.back().attached_view = tab_view;
    }
    drag_data.source_view_index_ = source_index;

    view_->GetDragContext()->StartedDragging(drag_data.attached_views());

    return drag_data;
  }

  raw_ptr<TabStripModel> model_;
  raw_ptr<TabStripViewInterface> view_;
};

// Flaky. http://crbug.com/417465013
#if BUILDFLAG(IS_WIN)
#define MAYBE_BasicTest DISABLED_BasicTest
#else
#define MAYBE_BasicTest BasicTest
#endif
IN_PROC_BROWSER_TEST_F(DraggingTabsSessionBrowserTest, MAYBE_BasicTest) {
  // Open two tabs.
  auto [tab_0, tab_0_view] = AddTab(0, true);
  auto [tab_1, tab_1_view] = AddTab(1, false);

  // Set up drag session.
  DragSessionData drag_data = StartDragging({0}, 0);
  const gfx::Point start_point = tab_0_view->GetBoundsInScreen().CenterPoint();
  DraggingTabsSession session(drag_data, view_->GetDragContext(), 0.5, true,
                              start_point);

  // Swap them.
  const gfx::Point target_point =
      tab_1_view->GetBoundsInScreen().CenterPoint() + gfx::Vector2d(10, 0);
  session.MoveAttached(target_point);

  // They should have been swapped in the model.
  EXPECT_EQ(tab_0, model_->GetTabAtIndex(1));
  EXPECT_EQ(tab_1, model_->GetTabAtIndex(0));

  // The dragged tab should be positioned with its center point under the
  // cursor.
  EXPECT_EQ(tab_0_view->GetBoundsInScreen().CenterPoint(), target_point);
}

// TODO(crbug.com/382754501): Add more tests, and delete tests that are made
// redundant in tab_drag_controller_interactive_uitest.cc.
