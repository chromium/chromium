// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"

typedef ChromeRenderViewHostTestHarness ChromeWebContentsViewDelegateViewsTest;

// Test that makes sure ShowContextMenu does not crash if web_contents() does
// not have a focused frame.
// TODO(crbug.com/40830006): Failing on Mac11.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ContextMenuNoFocusedFrame DISABLED_ContextMenuNoFocusedFrame
#else
#define MAYBE_ContextMenuNoFocusedFrame ContextMenuNoFocusedFrame
#endif
TEST_F(ChromeWebContentsViewDelegateViewsTest,
       MAYBE_ContextMenuNoFocusedFrame) {
  std::unique_ptr<content::WebContentsViewDelegate> delegate_view =
      CreateWebContentsViewDelegate(web_contents());
  EXPECT_FALSE(web_contents()->GetFocusedFrame());
  const content::ContextMenuParams params;
  delegate_view->ShowContextMenu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
}
