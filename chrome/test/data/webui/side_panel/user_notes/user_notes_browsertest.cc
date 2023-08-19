// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/test/browser_test.h"

class SidePanelUserNotesTest : public WebUIMochaBrowserTest {
 protected:
  SidePanelUserNotesTest() {
    set_test_loader_host(chrome::kChromeUIUserNotesSidePanelHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{user_notes::kUserNotes};
};

IN_PROC_BROWSER_TEST_F(SidePanelUserNotesTest, App) {
  RunTest("side_panel/user_notes/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelUserNotesTest, OverviewsList) {
  RunTest("side_panel/user_notes/user_note_overviews_list_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelUserNotesTest, List) {
  RunTest("side_panel/user_notes/user_notes_list_test.js", "mocha.run()");
}
