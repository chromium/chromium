// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/commerce/core/commerce_constants.h"
#include "content/public/test/browser_test.h"

using CommerceInternalsBrowserTest = WebUIMochaBrowserTest;

// Test that the internals page opens.
IN_PROC_BROWSER_TEST_F(CommerceInternalsBrowserTest, InternalsPageOpen) {
  set_test_loader_host(commerce::kChromeUICommerceInternalsHost);
  RunTestWithoutTestLoader("commerce_internals/commerce_internals_test.js",
                           "mocha.run()");
}
