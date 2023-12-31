// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

class OSSettingsMochaTest : public WebUIMochaBrowserTest {
 protected:
  OSSettingsMochaTest() {
    set_test_loader_host(chrome::kChromeUIOSSettingsHost);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kEnableHostnameSetting};
};

class OSSettingsCrostiniRevampEnabledTest : public OSSettingsMochaTest {
 protected:
  OSSettingsCrostiniRevampEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOsSettingsRevampWayfinding);

    SetCrostiniFeatures();
  }

 private:
  void SetCrostiniFeatures() { fake_crostini_features_.SetAll(true); }

  crostini::FakeCrostiniFeatures fake_crostini_features_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OSSettingsCrostiniRevampDisabledTest : public OSSettingsMochaTest {
 protected:
  OSSettingsCrostiniRevampDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kOsSettingsRevampWayfinding);
    SetCrostiniFeatures();
  }

 private:
  void SetCrostiniFeatures() { fake_crostini_features_.SetAll(true); }

  crostini::FakeCrostiniFeatures fake_crostini_features_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageBruschettaSubpageTest) {
  RunTest("settings/chromeos/crostini_page/bruschetta_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampEnabledTest,
                       OSSettingsCrostiniPageBruschettaSubpageRevampTest) {
  RunTest("settings/chromeos/crostini_page/bruschetta_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageCrostiniArcAdbTest) {
  RunTest("settings/chromeos/crostini_page/crostini_arc_adb_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampEnabledTest,
                       OSSettingsCrostiniPageCrostiniArcAdbRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_arc_adb_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageCrostiniExportImportTest) {
  RunTest("settings/chromeos/crostini_page/crostini_export_import_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampEnabledTest,
                       OSSettingsCrostiniPageCrostiniExportImportRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_export_import_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniRevampDisabledTest,
    OSSettingsCrostiniPageCrostiniExtraContainersSubpageTest) {
  RunTest(
      "settings/chromeos/crostini_page/"
      "crostini_extra_containers_subpage_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniRevampEnabledTest,
    OSSettingsCrostiniPageCrostiniExtraContainersSubpageRevampTest) {
  RunTest(
      "settings/chromeos/crostini_page/"
      "crostini_extra_containers_subpage_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageTest) {
  RunTest("settings/chromeos/crostini_page/crostini_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageCrostiniPortForwardingTest) {
  RunTest("settings/chromeos/crostini_page/crostini_port_forwarding_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampEnabledTest,
                       OSSettingsCrostiniPageCrostiniPortForwardingRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_port_forwarding_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageCrostiniSettingsCardTest) {
  RunTest("settings/chromeos/crostini_page/crostini_settings_card_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampEnabledTest,
                       OSSettingsAboutPageCrostiniSettingsCardRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_settings_card_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageCrostiniSharedUsbDevicesTest) {
  RunTest("settings/chromeos/crostini_page/crostini_shared_usb_devices_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniRevampEnabledTest,
    OSSettingsCrostiniPageCrostiniSharedUsbDevicesRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_shared_usb_devices_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampDisabledTest,
                       OSSettingsCrostiniPageCrostiniSubpageTest) {
  RunTest("settings/chromeos/crostini_page/crostini_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniRevampEnabledTest,
                       OSSettingsCrostiniPageCrostiniSubpageRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_subpage_test.js",
          "mocha.run()");
}
}  // namespace ash::settings
