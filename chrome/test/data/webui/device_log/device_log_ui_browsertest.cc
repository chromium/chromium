// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using DeviceLogUITest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(DeviceLogUITest, App) {
  set_test_loader_host(chrome::kChromeUIDeviceLogHost);
  RunTest("device_log/device_log_ui_test.js", "mocha.run()");
}
