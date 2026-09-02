// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace ai_overlay_dialog {

class AiOverlayNotesBrowserTest : public InProcessBrowserTest {
 public:
  AiOverlayNotesBrowserTest() = default;
  ~AiOverlayNotesBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(AiOverlayNotesBrowserTest, OpenRememberedNotesSubpage) {
  GURL notes_url("chrome-untrusted://ai-overlay-dialog/notes");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), notes_url));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(notes_url, web_contents->GetVisibleURL());
}

}  // namespace ai_overlay_dialog
