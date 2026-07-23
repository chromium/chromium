// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/notebooks/public/notebooks_constants.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

using NotebooksInternalsBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(NotebooksInternalsBrowserTest, InternalsPageOpen) {
  set_test_loader_host(notebooks::kChromeUINotebooksInternalsHost);
  g_browser_process->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);
  RunTestWithoutTestLoader("notebooks_internals/notebooks_internals_test.js",
                           "mocha.run()");
}
