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

class OSSettingsMochaTestRevampEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestRevampEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOsSettingsRevampWayfinding);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OSSettingsMochaTestRevampDisabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestRevampDisabled() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kOsSettingsRevampWayfinding);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, ApnDetailDialog) {
  RunTest("settings/chromeos/apn_detail_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppLanguageSelectionDialog) {
  RunTest(
      "settings/chromeos/common/app_language_selection_dialog/"
      "app_language_selection_dialog_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppLanguageSelectionItem) {
  RunTest(
      "settings/chromeos/common/app_language_selection_dialog/"
      "app_language_selection_item_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppManagementFileHandlingItem) {
  RunTest("settings/chromeos/app_management/file_handling_item_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppManagementManagedApps) {
  RunTest("settings/chromeos/app_management/managed_apps_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppManagementToggleRow) {
  RunTest("settings/chromeos/app_management/toggle_row_test.js", "mocha.run()");
}

class OSSettingsCrostiniTestRevampEnabled
    : public OSSettingsMochaTestRevampEnabled {
 protected:
  OSSettingsCrostiniTestRevampEnabled() {
    fake_crostini_features_.SetAll(true);
  }

 private:
  crostini::FakeCrostiniFeatures fake_crostini_features_;
};

class OSSettingsCrostiniTestRevampDisabled
    : public OSSettingsMochaTestRevampDisabled {
 protected:
  OSSettingsCrostiniTestRevampDisabled() {
    fake_crostini_features_.SetAll(true);
  }

 private:
  crostini::FakeCrostiniFeatures fake_crostini_features_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageBruschettaSubpageTest) {
  RunTest("settings/chromeos/crostini_page/bruschetta_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageBruschettaSubpageRevampTest) {
  RunTest("settings/chromeos/crostini_page/bruschetta_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniArcAdbTest) {
  RunTest("settings/chromeos/crostini_page/crostini_arc_adb_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniArcAdbRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_arc_adb_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniExportImportTest) {
  RunTest("settings/chromeos/crostini_page/crostini_export_import_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniExportImportRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_export_import_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniTestRevampDisabled,
    OSSettingsCrostiniPageCrostiniExtraContainersSubpageTest) {
  RunTest(
      "settings/chromeos/crostini_page/"
      "crostini_extra_containers_subpage_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniTestRevampEnabled,
    OSSettingsCrostiniPageCrostiniExtraContainersSubpageRevampTest) {
  RunTest(
      "settings/chromeos/crostini_page/"
      "crostini_extra_containers_subpage_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageTest) {
  RunTest("settings/chromeos/crostini_page/crostini_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniPortForwardingTest) {
  RunTest("settings/chromeos/crostini_page/crostini_port_forwarding_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniPortForwardingRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_port_forwarding_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniSettingsCardTest) {
  RunTest("settings/chromeos/crostini_page/crostini_settings_card_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsAboutPageCrostiniSettingsCardRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_settings_card_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniSharedUsbDevicesTest) {
  RunTest("settings/chromeos/crostini_page/crostini_shared_usb_devices_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniTestRevampEnabled,
    OSSettingsCrostiniPageCrostiniSharedUsbDevicesRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_shared_usb_devices_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniSubpageTest) {
  RunTest("settings/chromeos/crostini_page/crostini_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniSubpageRevampTest) {
  RunTest("settings/chromeos/crostini_page/crostini_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DateTimePage) {
  RunTest("settings/chromeos/date_time_page/date_time_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       DateTimePageDateTimeSettingsCard) {
  RunTest("settings/chromeos/date_time_page/date_time_settings_card_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DateTimePageTimezoneSelector) {
  RunTest("settings/chromeos/date_time_page/timezone_selector_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DateTimePageTimezoneSubpage) {
  RunTest("settings/chromeos/date_time_page/timezone_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DevicePageAudioPage) {
  RunTest("settings/chromeos/device_page/audio_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       DevicePageCustomizeButtonDropdownItem) {
  RunTest(
      "settings/chromeos/device_page/customize_button_dropdown_item_test.js",
      "mocha.run()");
}

class OSSettingsDevicePeripheralAndInputEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDevicePeripheralAndInputEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kPeripheralCustomization,
            ash::features::kInputDeviceSettingsSplit,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndInputEnabled,
                       DevicePageCustomizeButtonRow) {
  RunTest("settings/chromeos/device_page/customize_button_row_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DevicePageCustomizeButtonSelect) {
  RunTest("settings/chromeos/device_page/customize_button_select_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndInputEnabled,
                       DevicePageCustomizeButtonsSubsection) {
  RunTest("settings/chromeos/device_page/customize_buttons_subsection_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndInputEnabled,
                       DevicePageCustomizeMouseButtonsSubpage) {
  RunTest(
      "settings/chromeos/device_page/customize_mouse_buttons_subpage_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndInputEnabled,
                       DevicePageCustomizePenButtonsSubpage) {
  RunTest("settings/chromeos/device_page/customize_pen_buttons_subpage_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndInputEnabled,
                       DevicePageCustomizeTabletButtonsSubpage) {
  RunTest(
      "settings/chromeos/device_page/customize_tablet_buttons_subpage_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       DevicePageDisplayPage) {
  RunTest("settings/chromeos/device_page/display_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       DevicePageDisplayPageRevamp) {
  RunTest("settings/chromeos/device_page/display_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       DevicePageDisplaySettingsMojoInterfaceProvider) {
  RunTest(
      "settings/chromeos/device_page/"
      "display_settings_mojo_interface_provider_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndInputEnabled,
                       DevicePageDragAndDropManager) {
  RunTest("settings/chromeos/device_page/drag_and_drop_manager_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DevicePageFakeCrosAudioConfig) {
  RunTest("settings/chromeos/device_page/fake_cros_audio_config_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       DevicePageFakeInputDeviceSettingsProvider) {
  RunTest(
      "settings/chromeos/device_page/"
      "fake_input_device_settings_provider_test.js",
      "mocha.run()");
}
}  // namespace ash::settings
