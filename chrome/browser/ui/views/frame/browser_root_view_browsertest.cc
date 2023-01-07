// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_root_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"

class BrowserRootViewBrowserTest : public InProcessBrowserTest {
 public:
  BrowserRootViewBrowserTest() = default;

  BrowserRootViewBrowserTest(const BrowserRootViewBrowserTest&) = delete;
  BrowserRootViewBrowserTest& operator=(const BrowserRootViewBrowserTest&) =
      delete;

  BrowserRootView* browser_root_view() {
    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    return static_cast<BrowserRootView*>(
        browser_view->GetWidget()->GetRootView());
  }
};

// TODO(https://crbug.com/1220680): These tests produces wayland protocol error
// wl_display.error(xdg_surface, 1, "popup parent not constructed") on LaCrOS
// with Exo.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Clear drop info after performing drop. http://crbug.com/838791
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, ClearDropInfo) {
  ui::OSExchangeData data;
  data.SetURL(GURL("http://www.chromium.org/"), std::u16string());
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);
  auto* tab_strip_model = browser()->tab_strip_model();

  EXPECT_EQ(tab_strip_model->count(), 1);

  BrowserRootView* root_view = browser_root_view();
  root_view->OnDragUpdated(event);
  auto drop_cb = root_view->GetDropCallback(event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op);

  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(tab_strip_model->count(), 2);
  EXPECT_FALSE(root_view->drop_info_);
}

// Make sure plain string is droppable. http://crbug.com/838794
// crbug.com/1224945: Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PlainString DISABLED_PlainString
#else
#define MAYBE_PlainString PlainString
#endif
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, MAYBE_PlainString) {
  ui::OSExchangeData data;
  data.SetString(u"Plain string");
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  BrowserRootView* root_view = browser_root_view();
  EXPECT_NE(ui::DragDropTypes::DRAG_NONE, root_view->OnDragUpdated(event));
  auto cb = root_view->GetDropCallback(event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(event, output_drag_op);
  EXPECT_NE(ui::mojom::DragOperation::kNone, output_drag_op);
}

// Clear drop target when the widget is being destroyed.
// http://crbug.com/1001942
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, ClearDropTarget) {
  ui::OSExchangeData data;
  data.SetURL(GURL("http://www.chromium.org/"), std::u16string());
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  browser_root_view()->OnDragUpdated(event);

  // Calling this will cause segmentation fault if |root_view| doesn't clear
  // the target.
  CloseBrowserSynchronously(browser());
}

IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, OnDragEnteredNoTabs) {
  auto* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 1);
  EXPECT_EQ(tab_strip_model->active_index(), 0);
  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(tab_strip_model->count(), 0);
  EXPECT_EQ(tab_strip_model->active_index(), -1);

  ui::OSExchangeData data;
  data.SetURL(GURL("file:///test.txt"), std::u16string());
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  // Ensure OnDragEntered() doesn't crash when there are no active tabs.
  browser_root_view()->OnDragEntered(event);
}
#endif  // #if !BUILDFLAG(IS_CHROMEOS_LACROS)
