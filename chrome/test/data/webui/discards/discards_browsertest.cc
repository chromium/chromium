// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

using DiscardsTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(DiscardsTest, App) {
  set_test_loader_host(chrome::kChromeUIDiscardsHost);
  g_browser_process->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);
  RunTest("discards/discards_test.js", "mocha.run()");
}
