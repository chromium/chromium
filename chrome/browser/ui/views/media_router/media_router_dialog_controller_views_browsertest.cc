// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace media_router {

class MediaRouterDialogControllerViewsTest : public InProcessBrowserTest {
 public:
  MediaRouterDialogControllerViewsTest() = default;
  ~MediaRouterDialogControllerViewsTest() override = default;

  void OpenMediaRouterDialog();

 protected:
  WebContents* initiator_;
  MediaRouterDialogControllerViews* dialog_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerViewsTest);
};

void MediaRouterDialogControllerViewsTest::OpenMediaRouterDialog() {
  // Start with one window with one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator contents.
  initiator_ = browser()->tab_strip_model()->GetActiveWebContents();

  dialog_controller_ = static_cast<MediaRouterDialogControllerViews*>(
      MediaRouterDialogController::GetOrCreateForWebContents(initiator_));
  ASSERT_TRUE(dialog_controller_);

  // Show the media router dialog for the initiator.
  dialog_controller_->ShowMediaRouterDialog();
  ASSERT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());
}

// Create/Get a media router dialog for initiator.
IN_PROC_BROWSER_TEST_F(MediaRouterDialogControllerViewsTest,
                       OpenCloseMediaRouterDialog) {
  OpenMediaRouterDialog();
  views::Widget* widget = CastDialogView::GetCurrentDialogWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->HasObserver(dialog_controller_));
  dialog_controller_->CloseMediaRouterDialog();
  EXPECT_FALSE(dialog_controller_->IsShowingMediaRouterDialog());
  EXPECT_EQ(CastDialogView::GetCurrentDialogWidget(), nullptr);
}

}  // namespace media_router
