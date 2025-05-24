// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class BatchUploadViewBrowserTest : public WebUIMochaBrowserTest {
 public:
  BatchUploadViewBrowserTest() {
    set_test_loader_host(chrome::kChromeUIBatchUploadHost);
  }
};

IN_PROC_BROWSER_TEST_F(BatchUploadViewBrowserTest, MainView) {
  RunTest("signin/batch_upload_view_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BatchUploadViewBrowserTest, DataSections) {
  RunTest("signin/batch_upload_data_sections_test.js", "mocha.run()");
}
