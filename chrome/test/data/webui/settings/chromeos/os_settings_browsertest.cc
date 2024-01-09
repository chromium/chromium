// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"

namespace ash::settings {

class OSSettingsMochaTest : public WebUIMochaBrowserTest {
 protected:
  OSSettingsMochaTest() {
    set_test_loader_host(chrome::kChromeUIOSSettingsHost);
  }

  void RunSettingsTest(const std::string& current_path) {
    // All OS Settings test files are located in the directory
    // settings/chromeos/.
    const std::string path_with_parent_directory =
        base::StrCat({std::string("settings/chromeos/"), current_path});
    RunTest(path_with_parent_directory, "mocha.run()");
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
  RunSettingsTest("apn_detail_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppLanguageSelectionDialog) {
  RunSettingsTest(
      "common/app_language_selection_dialog/"
      "app_language_selection_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppLanguageSelectionItem) {
  RunSettingsTest(
      "common/app_language_selection_dialog/"
      "app_language_selection_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppManagementFileHandlingItem) {
  RunSettingsTest("app_management/file_handling_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppManagementManagedApps) {
  RunSettingsTest("app_management/managed_apps_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, AppManagementToggleRow) {
  RunSettingsTest("app_management/toggle_row_test.js");
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
  RunSettingsTest("crostini_page/bruschetta_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageBruschettaSubpageRevampTest) {
  RunSettingsTest("crostini_page/bruschetta_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniArcAdbTest) {
  RunSettingsTest("crostini_page/crostini_arc_adb_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniArcAdbRevampTest) {
  RunSettingsTest("crostini_page/crostini_arc_adb_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniExportImportTest) {
  RunSettingsTest("crostini_page/crostini_export_import_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniExportImportRevampTest) {
  RunSettingsTest("crostini_page/crostini_export_import_test.js");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniTestRevampDisabled,
    OSSettingsCrostiniPageCrostiniExtraContainersSubpageTest) {
  RunSettingsTest("crostini_page/crostini_extra_containers_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniTestRevampEnabled,
    OSSettingsCrostiniPageCrostiniExtraContainersSubpageRevampTest) {
  RunSettingsTest("crostini_page/crostini_extra_containers_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageTest) {
  RunSettingsTest("crostini_page/crostini_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniPortForwardingTest) {
  RunSettingsTest("crostini_page/crostini_port_forwarding_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniPortForwardingRevampTest) {
  RunSettingsTest("crostini_page/crostini_port_forwarding_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniSettingsCardTest) {
  RunSettingsTest("crostini_page/crostini_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsAboutPageCrostiniSettingsCardRevampTest) {
  RunSettingsTest("crostini_page/crostini_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniSharedUsbDevicesTest) {
  RunSettingsTest("crostini_page/crostini_shared_usb_devices_test.js");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsCrostiniTestRevampEnabled,
    OSSettingsCrostiniPageCrostiniSharedUsbDevicesRevampTest) {
  RunSettingsTest("crostini_page/crostini_shared_usb_devices_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       OSSettingsCrostiniPageCrostiniSubpageTest) {
  RunSettingsTest("crostini_page/crostini_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       OSSettingsCrostiniPageCrostiniSubpageRevampTest) {
  RunSettingsTest("crostini_page/crostini_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DateTimePage) {
  RunSettingsTest("date_time_page/date_time_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       DateTimePageDateTimeSettingsCard) {
  RunSettingsTest("date_time_page/date_time_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DateTimePageTimezoneSelector) {
  RunSettingsTest("date_time_page/timezone_selector_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DateTimePageTimezoneSubpage) {
  RunSettingsTest("date_time_page/timezone_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DevicePageAudioPage) {
  RunSettingsTest("device_page/audio_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       DevicePageCustomizeButtonDropdownItem) {
  RunSettingsTest("device_page/customize_button_dropdown_item_test.js");
}

class OSSettingsDevicePeripheralAndSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDevicePeripheralAndSplitEnabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageCustomizeButtonRow) {
  RunSettingsTest("device_page/customize_button_row_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DevicePageCustomizeButtonSelect) {
  RunSettingsTest("device_page/customize_button_select_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageCustomizeButtonsSubsection) {
  RunSettingsTest("device_page/customize_buttons_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageCustomizeMouseButtonsSubpage) {
  RunSettingsTest("device_page/customize_mouse_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageCustomizePenButtonsSubpage) {
  RunSettingsTest("device_page/customize_pen_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageCustomizeTabletButtonsSubpage) {
  RunSettingsTest("device_page/customize_tablet_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       DevicePageDisplayPage) {
  RunSettingsTest("device_page/display_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       DevicePageDisplayPageRevamp) {
  RunSettingsTest("device_page/display_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       DevicePageDisplaySettingsMojoInterfaceProvider) {
  RunSettingsTest(
      "device_page/display_settings_mojo_interface_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageDragAndDropManager) {
  RunSettingsTest("device_page/drag_and_drop_manager_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DevicePageFakeCrosAudioConfig) {
  RunSettingsTest("device_page/fake_cros_audio_config_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       DevicePageFakeInputDeviceSettingsProvider) {
  RunSettingsTest("device_page/fake_input_device_settings_provider_test.js");
}

class OSSettingsDeviceFKeyRowTest : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceFKeyRowTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kInputDeviceSettingsSplit,
            ash::features::kAltClickAndSixPackCustomization,
            ::features::kSupportF11AndF12KeyShortcuts,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceFKeyRowTest, DevicePageFKeyRow) {
  RunSettingsTest("device_page/fkey_row_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageGraphicsTabletSubpage) {
  RunSettingsTest("device_page/graphics_tablet_subpage_test.js");
}

class OSSettingsDeviceSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceSplitEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kInputDeviceSettingsSplit);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabled,
                       DevicePageInputDeviceMojoInterfaceProvider) {
  RunSettingsTest("device_page/input_device_mojo_interface_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageKeyCombinationInputDialog) {
  RunSettingsTest("device_page/key_combination_input_dialog_test.js");
}

class OSSettingsDevicePeripheralEnabledSplitDisabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDevicePeripheralEnabledSplitDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kPeripheralCustomization,
        },
        /*disabled=*/{
            ash::features::kInputDeviceSettingsSplit,
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralEnabledSplitDisabled,
                       DevicePageKeyboard) {
  RunSettingsTest("device_page/keyboard_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabled,
                       DevicePageKeyboardSixPackKeyRow) {
  RunSettingsTest("device_page/keyboard_six_pack_key_row_test.js");
}

class OSSettingsDeviceSplitEnabledRevampDisabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceSplitEnabledRevampDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kInputDeviceSettingsSplit,
        },
        /*disabled=*/{
            ash::features::kOsSettingsRevampWayfinding,
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabledRevampDisabled,
                       DevicePagePerDeviceKeyboard) {
  RunSettingsTest("device_page/per_device_keyboard_test.js");
}

class OSSettingsDeviceRevampAndSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceRevampAndSplitEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kInputDeviceSettingsSplit,
            ash::features::kOsSettingsRevampWayfinding,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceRevampAndSplitEnabled,
                       DevicePagePerDeviceKeyboardRevamp) {
  RunSettingsTest("device_page/per_device_keyboard_test.js");
}

class OSSettingsDeviceAltClickAndSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceAltClickAndSplitEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kAltClickAndSixPackCustomization,
            ash::features::kInputDeviceSettingsSplit,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceAltClickAndSplitEnabled,
                       DevicePagePerDeviceKeyboardRemapKeys) {
  RunSettingsTest("device_page/per_device_keyboard_remap_keys_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceAltClickAndSplitEnabled,
                       DevicePagePerDeviceKeyboardSubsection) {
  RunSettingsTest("device_page/per_device_keyboard_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabled, DevicePagePerDeviceMouse) {
  RunSettingsTest("device_page/per_device_mouse_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePagePerDeviceMouseSubsection) {
  RunSettingsTest("device_page/per_device_mouse_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabled,
                       DevicePagePerDevicePointingStick) {
  RunSettingsTest("device_page/per_device_pointing_stick_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabled,
                       DevicePagePerDevicePointingStickSubsection) {
  RunSettingsTest("device_page/per_device_pointing_stick_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabled,
                       DevicePagePerDeviceTouchpad) {
  RunSettingsTest("device_page/per_device_touchpad_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceSplitEnabled,
                       DevicePagePerDeviceTouchpadSubsection) {
  RunSettingsTest("device_page/per_device_touchpad_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralEnabledSplitDisabled,
                       DevicePagePointers) {
  RunSettingsTest("device_page/pointers_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, DevicePagePower) {
  RunSettingsTest("device_page/power_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       DevicePagePowerRevamp) {
  RunSettingsTest("device_page/power_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       DevicePagePrintingSettingsCard) {
  RunSettingsTest("os_printing_page/printing_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       DevicePageInputSettings) {
  RunSettingsTest("device_page/device_page_input_settings_test.js");
}

class OSSettingsDevicePeripheralAndSplitEnabledRevampDisabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDevicePeripheralAndSplitEnabledRevampDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kPeripheralCustomization,
            ash::features::kInputDeviceSettingsSplit,
        },
        /*disabled=*/{ash::features::kOsSettingsRevampWayfinding});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabledRevampDisabled,
                       DevicePageStorage) {
  RunSettingsTest("device_page/storage_test.js");
}

class OSSettingsDevicePeripheralAndSplitAndRevampEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDevicePeripheralAndSplitAndRevampEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kPeripheralCustomization,
            ash::features::kInputDeviceSettingsSplit,
            ash::features::kOsSettingsRevampWayfinding,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitAndRevampEnabled,
                       DevicePageStorageRevamp) {
  RunSettingsTest("device_page/storage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabled,
                       DevicePageStylus) {
  RunSettingsTest("device_page/stylus_test.js");
}
}  // namespace ash::settings
