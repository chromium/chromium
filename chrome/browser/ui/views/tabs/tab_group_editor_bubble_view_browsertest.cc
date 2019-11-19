// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"

class TabGroupEditorBubbleViewDialogBrowserTest : public DialogBrowserTest {
 protected:
  void ShowUi(const std::string& name) override {
    TabGroupId group = browser()->tab_strip_model()->AddToNewGroup({0});

    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    TabGroupHeader* header =
        browser_view->tabstrip()->group_headers_[group].get();
    ASSERT_NE(nullptr, header);

    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::PointF(),
                                 gfx::PointF(), base::TimeTicks(), 0, 0);
    header->OnMousePressed(pressed_event);

    ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                  gfx::PointF(), base::TimeTicks(), 0, 0);
    header->OnMouseReleased(released_event);
  }
};

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
