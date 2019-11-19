// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_root_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"

class BrowserRootViewBrowserTest : public InProcessBrowserTest {
 public:
  BrowserRootViewBrowserTest() = default;

  BrowserRootView* browser_root_view() {
    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    return static_cast<BrowserRootView*>(
        browser_view->GetWidget()->GetRootView());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserRootViewBrowserTest);
};

// Clear drop info after performing drop. http://crbug.com/838791
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, ClearDropInfo) {
  ui::OSExchangeData data;
  data.SetURL(GURL("http://www.chromium.org/"), base::string16());
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  BrowserRootView* root_view = browser_root_view();
  root_view->OnDragUpdated(event);
  root_view->OnPerformDrop(event);
  EXPECT_FALSE(root_view->drop_info_);
}

// Make sure plain string is droppable. http://crbug.com/838794
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, PlainString) {
  ui::OSExchangeData data;
  data.SetString(base::ASCIIToUTF16("Plain string"));
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  BrowserRootView* root_view = browser_root_view();
  EXPECT_NE(ui::DragDropTypes::DRAG_NONE, root_view->OnDragUpdated(event));
  EXPECT_NE(ui::DragDropTypes::DRAG_NONE, root_view->OnPerformDrop(event));
}

// Clear drop target when the widget is being destroyed.
// http://crbug.com/1001942
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, ClearDropTarget) {
  ui::OSExchangeData data;
  data.SetURL(GURL("http://www.chromium.org/"), base::string16());
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  browser_root_view()->OnDragUpdated(event);

  // Calling this will cause segmentation fault if |root_view| doesn't clear
  // the target.
  CloseBrowserSynchronously(browser());
}
