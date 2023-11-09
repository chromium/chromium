// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://accessory-update.
 */
namespace ash {

namespace {

// TODO(b/309953643): Configure test support for firmware update and update code
// to use same const as prod.
constexpr char kFirmwareUpdateHost[] = "accessory-update";

class FirmwareUpdateAppBrowserTest : public WebUIMochaBrowserTest {
 public:
  FirmwareUpdateAppBrowserTest() { set_test_loader_host(kFirmwareUpdateHost); }
};

IN_PROC_BROWSER_TEST_F(FirmwareUpdateAppBrowserTest, FakeUpdateController) {
  RunTest("chromeos/firmware_update/fake_update_controller_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FirmwareUpdateAppBrowserTest, FakeUpdateProvider) {
  RunTest("chromeos/firmware_update/fake_update_provider_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FirmwareUpdateAppBrowserTest, FirmwareUpdateDialog) {
  RunTest("chromeos/firmware_update/firmware_update_dialog_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FirmwareUpdateAppBrowserTest, FirmwareUpdateApp) {
  RunTest("chromeos/firmware_update/firmware_update_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FirmwareUpdateAppBrowserTest, FirmwareUpdateUtils) {
  RunTest("chromeos/firmware_update/firmware_update_utils_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FirmwareUpdateAppBrowserTest, PeripheralUpdatesList) {
  RunTest("chromeos/firmware_update/peripheral_updates_list_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FirmwareUpdateAppBrowserTest, UpdateCard) {
  RunTest("chromeos/firmware_update/update_card_test.js", "mocha.run()");
}

}  // namespace

}  // namespace ash
