// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class OnDeviceTranslationInternalsTest : public WebUIMochaBrowserTest {
 protected:
  OnDeviceTranslationInternalsTest() {
    set_test_loader_host(chrome::kChromeUIOnDeviceTranslationInternalsHost);
  }
};

IN_PROC_BROWSER_TEST_F(OnDeviceTranslationInternalsTest, All) {
  RunTest(
      "on_device_translation_internals/"
      "on_device_translation_internals_test.js",
      "mocha.run()");
}
