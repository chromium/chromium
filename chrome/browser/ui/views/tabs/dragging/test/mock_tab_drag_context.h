// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TEST_MOCK_TAB_DRAG_CONTEXT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TEST_MOCK_TAB_DRAG_CONTEXT_H_

#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockTabDragContext : public TabDragContext {
 public:
  MockTabDragContext();
  ~MockTabDragContext() override;
  // TabDragContextBase methods:
  MOCK_METHOD(void,
              UpdateAnimationTarget,
              (TabSlotView*, const gfx::Rect&),
              (override));
  MOCK_METHOD(bool, IsDragSessionActive, (), (const, override));
  MOCK_METHOD(bool, IsAnimatingDragEnd, (), (const, override));
  MOCK_METHOD(void, CompleteEndDragAnimations, (), (override));
  MOCK_METHOD(int, GetTabDragAreaWidth, (), (const, override));

  // TabDragContext methods:
  MOCK_METHOD(Tab*, GetTabAt, (int index), (const, override));
  MOCK_METHOD(std::optional<int>,
              GetIndexOf,
              (const TabSlotView* view),
              (const, override));
  MOCK_METHOD(int, GetTabCount, (), (const, override));
  MOCK_METHOD(bool, IsTabPinned, (const Tab* tab), (const, override));
  MOCK_METHOD(int, GetPinnedTabCount, (), (const, override));
  MOCK_METHOD(TabGroupHeader*,
              GetTabGroupHeader,
              (const tab_groups::TabGroupId& group),
              (const, override));
  MOCK_METHOD(TabStripModel*, GetTabStripModel, (), (override));
  MOCK_METHOD(TabDragController*, GetDragController, (), (override));
  MOCK_METHOD(void,
              OwnDragController,
              (std::unique_ptr<TabDragController> controller),
              (override));
  MOCK_METHOD(views::ScrollView*, GetScrollView, (), (override));
  MOCK_METHOD(std::unique_ptr<TabDragController>,
              ReleaseDragController,
              (),
              (override));
  MOCK_METHOD(void,
              SetDragControllerCallbackForTesting,
              (base::OnceCallback<void(TabDragController*)> callback),
              (override));
  MOCK_METHOD(void, DestroyDragController, (), (override));
  MOCK_METHOD(bool, IsActiveDropTarget, (), (const, override));
  MOCK_METHOD(int, TabDragAreaEndX, (), (const, override));
  MOCK_METHOD(int, TabDragAreaBeginX, (), (const, override));
  MOCK_METHOD(int,
              GetInsertionIndexForDraggedBounds,
              (const gfx::Rect& dragged_bounds,
               std::vector<TabSlotView*> dragged_views,
               int num_dragged_tabs),
              (const, override));
  MOCK_METHOD(std::vector<gfx::Rect>,
              CalculateBoundsForDraggedViews,
              (const std::vector<TabSlotView*>& views),
              (override));
  MOCK_METHOD(void,
              SetBoundsForDrag,
              (const std::vector<TabSlotView*>& views,
               const std::vector<gfx::Rect>& bounds),
              (override));
  MOCK_METHOD(void,
              StartedDragging,
              (const std::vector<TabSlotView*>& views),
              (override));
  MOCK_METHOD(void, DraggedTabsDetached, (), (override));
  MOCK_METHOD(void, StoppedDragging, (), (override));
  MOCK_METHOD(void,
              LayoutDraggedViewsAt,
              (const std::vector<TabSlotView*>& views,
               TabSlotView* source_view,
               const gfx::Point& location,
               bool initial_drag),
              (override));
  MOCK_METHOD(void, ForceLayout, (), (override));
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TEST_MOCK_TAB_DRAG_CONTEXT_H_
