// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

using CommerceInternalsBrowserTest = WebUIMochaBrowserTest;

// Test that the internals page opens.
IN_PROC_BROWSER_TEST_F(CommerceInternalsBrowserTest, InternalsPageOpen) {
  set_test_loader_host(commerce::kChromeUICommerceInternalsHost);
  g_browser_process->local_state()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, true);
  RunTestWithoutTestLoader("commerce_internals/commerce_internals_test.js",
                           "mocha.run()");
}
