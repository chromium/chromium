// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

using LocationInternalsUIBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(LocationInternalsUIBrowserTest,
                       OpenLocationInternalsWebUI) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(content::GetWebUIURL(chrome::kChromeUILocationInternalsHost))));
}
