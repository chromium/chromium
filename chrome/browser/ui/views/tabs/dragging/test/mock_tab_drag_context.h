// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TEST_MOCK_TAB_DRAG_CONTEXT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TEST_MOCK_TAB_DRAG_CONTEXT_H_

#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"

class MockTabDragContext : public TabDragContext {
  METADATA_HEADER(MockTabDragContext, TabDragContext)

 public:
  MockTabDragContext();
  ~MockTabDragContext() override;
  // TabDragContext methods:
  MOCK_METHOD(TabDragContext*,
              GetContextForNewBrowser,
              (BrowserView * browser_view),
              (const, override));
  MOCK_METHOD(views::View*,
              GetTabForContents,
              (content::WebContents * contents),
              (override));
  MOCK_METHOD(content::WebContents*,
              GetContentsForTab,
              (views::View * view),
              (override));
  MOCK_METHOD(bool,
              IsTabDetachable,
              (const views::View* view),
              (const, override));
  MOCK_METHOD(void, SetTabDetached, (views::View * view), (override));
  MOCK_METHOD(views::View*,
              GetTabGroupHeader,
              (const tab_groups::TabGroupId& group),
              (override));
  MOCK_METHOD(std::optional<tab_groups::TabGroupId>,
              GetTabGroupForHeader,
              (const views::View* header_view),
              (const, override));
  MOCK_METHOD(TabStripModel*, GetTabStripModel, (), (override));
  MOCK_METHOD(TabDragController*, GetDragController, (), (override));
  MOCK_METHOD(void,
              OwnDragController,
              (std::unique_ptr<TabDragController> controller),
              (override));
  MOCK_METHOD(std::unique_ptr<TabDragController>,
              ReleaseDragController,
              (),
              (override));
  MOCK_METHOD(void,
              SetDragControllerCallbackForTesting,
              (base::OnceCallback<void(TabDragController*)> callback),
              (override));
  MOCK_METHOD(void, DestroyDragController, (), (override));
  MOCK_METHOD(void,
              StartedDragging,
              (const std::vector<views::View*>& views),
              (override));
  MOCK_METHOD(void, DraggedTabsDetached, (), (override));
  MOCK_METHOD(void, StoppedDragging, (), (override));
  MOCK_METHOD(TabDragPositioningDelegate*,
              GetPositioningDelegate,
              (),
              (override));
  MOCK_METHOD(bool,
              NotifyCustomEvent,
              (ui::CustomElementEventType event_type, views::View* view),
              (override));
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TEST_MOCK_TAB_DRAG_CONTEXT_H_
