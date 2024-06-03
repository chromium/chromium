// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest CrComponentsFocusTest;

IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, MostVisited) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/most_visited_focus_test.js", "mocha.run()");
}

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
IN_PROC_BROWSER_TEST_F(CrComponentsFocusTest, CertificateManagerV2) {
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/certificate_manager_v2_focus_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
