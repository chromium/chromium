// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using KeyValuePairViewerTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(KeyValuePairViewerTest, KeyValuePairViewer) {
  set_test_loader_host(chrome::kChromeUISystemInfoHost);
  RunTest("key_value_pair_viewer/key_value_pair_viewer_test.js", "mocha.run()");
}
