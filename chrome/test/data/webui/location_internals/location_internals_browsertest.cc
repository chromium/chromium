// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

class LocationInternalsTest : public WebUIMochaBrowserTest {
 protected:
  LocationInternalsTest() {
    set_test_loader_host(chrome::kChromeUIUsbInternalsHost);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
    // Enables the MojoJSTest bindings which are used in
    // location_internals_test.ts.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "MojoJSTest");
  }
};

IN_PROC_BROWSER_TEST_F(LocationInternalsTest, All) {
  set_test_loader_host(chrome::kChromeUILocationInternalsHost);
  RunTestWithoutTestLoader("location_internals/location_internals_test.js",
                           "mocha.run()");
}
