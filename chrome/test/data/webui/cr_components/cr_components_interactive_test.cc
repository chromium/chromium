// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest CrComponentsFocusTest;

IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, MostVisited) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/most_visited_focus_test.js", "mocha.run()");
}
