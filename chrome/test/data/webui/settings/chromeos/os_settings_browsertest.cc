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
                       CrostiniPageBruschettaSubpage) {
  RunSettingsTest("crostini_page/bruschetta_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       CrostiniPageBruschettaSubpageRevamp) {
  RunSettingsTest("crostini_page/bruschetta_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       CrostiniPageCrostiniArcAdb) {
  RunSettingsTest("crostini_page/crostini_arc_adb_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       CrostiniPageCrostiniArcAdbRevamp) {
  RunSettingsTest("crostini_page/crostini_arc_adb_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       CrostiniPageCrostiniExportImport) {
  RunSettingsTest("crostini_page/crostini_export_import_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       CrostiniPageCrostiniExportImportRevamp) {
  RunSettingsTest("crostini_page/crostini_export_import_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       CrostiniPageCrostiniExtraContainersSubpage) {
  RunSettingsTest("crostini_page/crostini_extra_containers_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       CrostiniPageCrostiniExtraContainersSubpageRevamp) {
  RunSettingsTest("crostini_page/crostini_extra_containers_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled, CrostiniPage) {
  RunSettingsTest("crostini_page/crostini_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       CrostiniPageCrostiniPortForwarding) {
  RunSettingsTest("crostini_page/crostini_port_forwarding_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       CrostiniPageCrostiniPortForwardingRevamp) {
  RunSettingsTest("crostini_page/crostini_port_forwarding_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       CrostiniPageCrostiniSettingsCard) {
  RunSettingsTest("crostini_page/crostini_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       AboutPageCrostiniSettingsCardRevamp) {
  RunSettingsTest("crostini_page/crostini_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       CrostiniPageCrostiniSharedUsbDevices) {
  RunSettingsTest("crostini_page/crostini_shared_usb_devices_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       CrostiniPageCrostiniSharedUsbDevicesRevamp) {
  RunSettingsTest("crostini_page/crostini_shared_usb_devices_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled,
                       CrostiniPageCrostiniSubpage) {
  RunSettingsTest("crostini_page/crostini_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampEnabled,
                       CrostiniPageCrostiniSubpageRevamp) {
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

class OSSettingsDeviceTestPeripheralAndSplitEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestPeripheralAndSplitEnabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeButtonRow) {
  RunSettingsTest("device_page/customize_button_row_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DevicePageCustomizeButtonSelect) {
  RunSettingsTest("device_page/customize_button_select_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeButtonsSubsection) {
  RunSettingsTest("device_page/customize_buttons_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeMouseButtonsSubpage) {
  RunSettingsTest("device_page/customize_mouse_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizePenButtonsSubpage) {
  RunSettingsTest("device_page/customize_pen_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
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

class OSSettingsDeviceTestSplitAndAltAndFKeyEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestSplitAndAltAndFKeyEnabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitAndAltAndFKeyEnabled,
                       DevicePageFKeyRow) {
  RunSettingsTest("device_page/fkey_row_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageGraphicsTabletSubpage) {
  RunSettingsTest("device_page/graphics_tablet_subpage_test.js");
}

class OSSettingsDeviceTestSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestSplitEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kInputDeviceSettingsSplit);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabled,
                       DevicePageInputDeviceMojoInterfaceProvider) {
  RunSettingsTest("device_page/input_device_mojo_interface_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageKeyCombinationInputDialog) {
  RunSettingsTest("device_page/key_combination_input_dialog_test.js");
}

class OSSettingsDeviceTestPeripheralEnabledSplitDisabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestPeripheralEnabledSplitDisabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralEnabledSplitDisabled,
                       DevicePageKeyboard) {
  RunSettingsTest("device_page/keyboard_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabled,
                       DevicePageKeyboardSixPackKeyRow) {
  RunSettingsTest("device_page/keyboard_six_pack_key_row_test.js");
}

class OSSettingsDeviceTestSplitEnabledRevampDisabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestSplitEnabledRevampDisabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabledRevampDisabled,
                       DevicePagePerDeviceKeyboard) {
  RunSettingsTest("device_page/per_device_keyboard_test.js");
}

class OSSettingsDeviceTestRevampAndSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestRevampAndSplitEnabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestRevampAndSplitEnabled,
                       DevicePagePerDeviceKeyboardRevamp) {
  RunSettingsTest("device_page/per_device_keyboard_test.js");
}

class OSSettingsDeviceTestAltClickAndSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestAltClickAndSplitEnabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestAltClickAndSplitEnabled,
                       DevicePagePerDeviceKeyboardRemapKeys) {
  RunSettingsTest("device_page/per_device_keyboard_remap_keys_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestAltClickAndSplitEnabled,
                       DevicePagePerDeviceKeyboardSubsection) {
  RunSettingsTest("device_page/per_device_keyboard_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabled,
                       DevicePagePerDeviceMouse) {
  RunSettingsTest("device_page/per_device_mouse_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePagePerDeviceMouseSubsection) {
  RunSettingsTest("device_page/per_device_mouse_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabled,
                       DevicePagePerDevicePointingStick) {
  RunSettingsTest("device_page/per_device_pointing_stick_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabled,
                       DevicePagePerDevicePointingStickSubsection) {
  RunSettingsTest("device_page/per_device_pointing_stick_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabled,
                       DevicePagePerDeviceTouchpad) {
  RunSettingsTest("device_page/per_device_touchpad_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestSplitEnabled,
                       DevicePagePerDeviceTouchpadSubsection) {
  RunSettingsTest("device_page/per_device_touchpad_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralEnabledSplitDisabled,
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

class OSSettingsDeviceTestPeripheralAndSplitAndRevampEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsDeviceTestPeripheralAndSplitAndRevampEnabled() {
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitAndRevampEnabled,
                       DevicePageStorageRevamp) {
  RunSettingsTest("device_page/storage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageStylus) {
  RunSettingsTest("device_page/stylus_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageCellularSetupDialog) {
  RunSettingsTest("internet_page/cellular_setup_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, GuestOsSharedPaths) {
  RunSettingsTest("guest_os/guest_os_shared_paths_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, GuestOsSharedUsbDevices) {
  RunSettingsTest("guest_os/guest_os_shared_usb_devices_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       InternetPageEsimInstallErrorDialog) {
  RunSettingsTest("internet_page/esim_install_error_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageEsimRenameDialog) {
  RunSettingsTest("internet_page/esim_rename_dialog_test.js");
}

class OSSettingsInternetTestHotspotEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsInternetTestHotspotEnabled() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kHotspot);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsInternetTestHotspotEnabled,
                       InternetPageHotspotConfigDialog) {
  RunSettingsTest("internet_page/hotspot_config_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsInternetTestHotspotEnabled,
                       InternetPageHotspotSubpage) {
  RunSettingsTest("internet_page/hotspot_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsInternetTestHotspotEnabled,
                       InternetPageHotspotSummaryItem) {
  RunSettingsTest("internet_page/hotspot_summary_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageInternetConfig) {
  RunSettingsTest("internet_page/internet_config_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageInternetDetailMenu) {
  RunSettingsTest("internet_page/internet_detail_menu_test.js");
}

class OSSettingsInternetTestApnAndPasspointEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsInternetTestApnAndPasspointEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kApnRevamp,
            ash::features::kPasspointSettings,
            ash::features::kPasspointARCSupport,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsInternetTestApnAndPasspointEnabled,
                       InternetPageInternetDetailSubpage) {
  RunSettingsTest("internet_page/internet_detail_subpage_test.js");
}

class OSSettingsInternetTestPasspointEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsInternetTestPasspointEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kPasspointSettings,
            ash::features::kPasspointARCSupport,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsInternetTestPasspointEnabled,
                       InternetPageInternetKnownNetworksSubpage) {
  RunSettingsTest("internet_page/internet_known_networks_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageInternetSubpageMenu) {
  RunSettingsTest("internet_page/internet_subpage_menu_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageInternetSubpage) {
  RunSettingsTest("internet_page/internet_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageNetworkAlwaysOnVpn) {
  RunSettingsTest("internet_page/network_always_on_vpn_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       InternetPageNetworkDeviceInfoDialog) {
  RunSettingsTest("internet_page/network_device_info_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageNetworkProxySection) {
  RunSettingsTest("internet_page/network_proxy_section_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageNetworkSummary) {
  RunSettingsTest("internet_page/network_summary_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageNetworkSummaryItem) {
  RunSettingsTest("internet_page/network_summary_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsInternetTestPasspointEnabled,
                       InternetPagePasspointSubpage) {
  RunSettingsTest("internet_page/passpoint_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsInternetTestPasspointEnabled,
                       InternetPagePasspointRemoveDialog) {
  RunSettingsTest("internet_page/passpoint_remove_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       InternetPageSettingsTrafficCounters) {
  RunSettingsTest("internet_page/settings_traffic_counters_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       InternetPageTetherConnectionDialog) {
  RunSettingsTest("internet_page/tether_connection_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, KerberosPage) {
  RunSettingsTest("kerberos_page/kerberos_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       KerberosPageKerberosAccountsSubpage) {
  RunSettingsTest("kerberos_page/kerberos_accounts_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       KerberosPageKerberosAddAccountDialog) {
  RunSettingsTest("kerberos_page/kerberos_add_account_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, KeyboardShortcutBanner) {
  RunSettingsTest("keyboard_shortcut_banner/keyboard_shortcut_banner_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, LockScreenSubpage) {
  RunSettingsTest("lock_screen_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, MainPageContainer) {
  RunSettingsTest("main_page_container/main_page_container_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       MainPageContainerRevamp) {
  RunSettingsTest("main_page_container/main_page_container_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, MainPageContainerPageDisplayer) {
  RunSettingsTest("main_page_container/page_displayer_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       MainPageContainerRouteNavigation) {
  RunSettingsTest("main_page_container/route_navigation_test.js");
}
}  // namespace ash::settings
