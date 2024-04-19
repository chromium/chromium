// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chromeos/lacros_only_mocha_browser_test.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"

namespace ash::settings {

class OSSettingsMochaTest : public WebUIMochaBrowserTest {
 protected:
  OSSettingsMochaTest() {
    set_test_loader_host(chrome::kChromeUIOSSettingsHost);
  }

  // Runs the specified test.
  // - test_path: The path to the test file within the CrOS Settings test root
  //              directory.
  // - trigger: A JS string used to trigger the tests, defaults to
  //            "mocha.run()".
  void RunSettingsTest(
      const std::string& test_path,
      const std::string& trigger = std::string("mocha.run()")) {
    // All OS Settings test files are located in the directory
    // settings/chromeos/.
    const std::string path_with_parent_directory = base::StrCat({
        "settings/chromeos/",
        test_path,
    });
    RunTest(path_with_parent_directory, trigger);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kEnableHostnameSetting};
};

// This class parameterizes the tests to run once with
// OSSettingsRevampWayfinding feature enabled and once disabled.
class OSSettingsRevampMochaTest : public OSSettingsMochaTest,
                                  public testing::WithParamInterface<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "OsSettingsRevampWayfindingEnabled"
                      : "OsSettingsRevampWayfindingDisabled";
  }

 protected:
  OSSettingsRevampMochaTest() {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kOsSettingsRevampWayfinding,
        /*enabled=*/GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(RevampParameterized,
                         OSSettingsRevampMochaTest,
                         testing::Bool(),
                         OSSettingsRevampMochaTest::DescribeParams);

class OSSettingsMochaTestRevampEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestRevampEnabled() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kOsSettingsRevampWayfinding,
         ash::features::kFasterSplitScreenSetup},
        {});
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

class OSSettingsMochaTestApnRevampEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestApnRevampEnabled() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kApnRevamp);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OSSettingsMochaTestReducedAnimationsEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestReducedAnimationsEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityReducedAnimations);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OSSettingsMochaTestMouseKeysEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestMouseKeysEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMouseKeys);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OSSettingsMochaTestCaretBlinkSettingEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestCaretBlinkSettingEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityCaretBlinkIntervalSetting);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OSSettingsMochaTestLacrosOnlyEnabled : public LacrosOnlyMochaBrowserTest {
 protected:
  OSSettingsMochaTestLacrosOnlyEnabled() : LacrosOnlyMochaBrowserTest() {
    set_test_loader_host(chrome::kChromeUIOSSettingsHost);
  }

  void RunSettingsTest(const std::string& test_path) {
    // All OS Settings test files are located in the directory
    // settings/chromeos/.
    const std::string path_with_parent_directory = base::StrCat({
        "settings/chromeos/",
        test_path,
    });
    RunTest(path_with_parent_directory, "mocha.run()");
  }
};

class OSSettingsMochaTestAppParentalControlsEnabled
    : public OSSettingsMochaTestRevampEnabled {
 protected:
  OSSettingsMochaTestAppParentalControlsEnabled() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOnDeviceAppControls,
         features::kForceOnDeviceAppControlsForAllRegions},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, AppLanguageSelectionDialog) {
  RunSettingsTest(
      "common/app_language_selection_dialog/"
      "app_language_selection_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, AppLanguageSelectionItem) {
  RunSettingsTest(
      "common/app_language_selection_dialog/"
      "app_language_selection_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       AppManagementFileHandlingItem) {
  RunSettingsTest("app_management/file_handling_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, AppManagementManagedApps) {
  RunSettingsTest("app_management/managed_apps_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, AppManagementToggleRow) {
  RunSettingsTest("app_management/toggle_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, ControlledButton) {
  RunSettingsTest("controls/controlled_button_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, ControlledRadioButton) {
  RunSettingsTest("controls/controlled_radio_button_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       ExtensionControlledIndicator) {
  RunSettingsTest("controls/extension_controlled_indicator_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, DropdownMenu) {
  RunSettingsTest("controls/dropdown_menu_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsSlider) {
  RunSettingsTest("controls/settings_slider_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsToggleButton) {
  RunSettingsTest("controls/settings_toggle_button_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, PrefControlMixinInternal) {
  RunSettingsTest("controls/v2/pref_control_mixin_internal_test.js");
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
        /*disabled=*/{
            ash::features::kOsSettingsRevampWayfinding,
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabledRevampDisabled,
                       DevicePage) {
  RunSettingsTest("device_page/device_page_test.js");
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
                       DevicePageRevamp) {
  RunSettingsTest("device_page/device_page_test.js");
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

class OSSettingsMochaTestSplitEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestSplitEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kInputDeviceSettingsSplit);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
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

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
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
            ash::features::kEnableKeyboardBacklightControlInSettings,
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
            ash::features::kEnableKeyboardBacklightControlInSettings,
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
            ash::features::kEnableKeyboardBacklightControlInSettings,
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

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
                       DevicePagePerDeviceMouse) {
  RunSettingsTest("device_page/per_device_mouse_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePagePerDeviceMouseSubsection) {
  RunSettingsTest("device_page/per_device_mouse_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
                       DevicePagePerDevicePointingStick) {
  RunSettingsTest("device_page/per_device_pointing_stick_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
                       DevicePagePerDevicePointingStickSubsection) {
  RunSettingsTest("device_page/per_device_pointing_stick_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
                       DevicePagePerDeviceTouchpad) {
  RunSettingsTest("device_page/per_device_touchpad_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
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

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageInputSettings) {
  RunSettingsTest("device_page/device_page_input_settings_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDevicePeripheralAndSplitEnabledRevampDisabled,
                       DevicePageStorage) {
  RunSettingsTest("device_page/storage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitAndRevampEnabled,
                       DevicePageStorageRevamp) {
  RunSettingsTest("device_page/storage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitEnabled,
                       DevicePageStylus) {
  RunSettingsTest("device_page/stylus_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestApnRevampEnabled,
                       InternetPageApnSubpage) {
  RunSettingsTest("internet_page/apn_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, InternetPageApnDetailDialog) {
  RunSettingsTest("internet_page/apn_detail_dialog_test.js");
}

class OSSettingsMochaTestApnAndHotspotAndPasspointEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestApnAndHotspotAndPasspointEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kApnRevamp,
            ash::features::kHotspot,
            ash::features::kPasspointSettings,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestApnAndHotspotAndPasspointEnabled,
                       InternetPage) {
  RunSettingsTest("internet_page/internet_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageCellularNetworksList) {
  RunSettingsTest("internet_page/cellular_networks_list_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageCellularRoamingToggleButton) {
  RunSettingsTest("internet_page/cellular_roaming_toggle_button_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, InternetPageCellularSetupDialog) {
  RunSettingsTest("internet_page/cellular_setup_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       InternetPageEsimRemoveProfileDialog) {
  RunSettingsTest("internet_page/esim_remove_profile_dialog_test.js");
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

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, MultidevicePage) {
  RunSettingsTest("multidevice_page/multidevice_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       MultidevicePageRevamp) {
  RunSettingsTest("multidevice_page/multidevice_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultideviceFeatureItem) {
  RunSettingsTest("multidevice_page/multidevice_feature_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultideviceFeatureToggle) {
  RunSettingsTest("multidevice_page/multidevice_feature_toggle_test.js");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsMochaTest,
    MultidevicePageMultideviceNotificationAccessSetupDialog) {
  RunSettingsTest(
      "multidevice_page/multidevice_notification_access_setup_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultidevicePermissionsSetupDialog) {
  RunSettingsTest(
      "multidevice_page/multidevice_permissions_setup_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultideviceSmartlockItem) {
  RunSettingsTest("multidevice_page/multidevice_smartlock_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, MultidevicePageMultideviceSubPage) {
  RunSettingsTest("multidevice_page/multidevice_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultiDevicePageMultideviceCombinedSetupItem) {
  RunSettingsTest("multidevice_page/multidevice_combined_setup_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultideviceTaskContinuationDisabledLink) {
  RunSettingsTest(
      "multidevice_page/multidevice_task_continuation_disabled_link_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultideviceTaskContinuationItem) {
  RunSettingsTest(
      "multidevice_page/multidevice_task_continuation_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultideviceWifiSyncDisabledLink) {
  RunSettingsTest(
      "multidevice_page/multidevice_wifi_sync_disabled_link_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       MultidevicePageMultideviceWifiSyncItem) {
  RunSettingsTest("multidevice_page/multidevice_wifi_sync_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       NearbySharePageNearbyShareConfirmPage) {
  RunSettingsTest("nearby_share_page/nearby_share_confirm_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       NearbySharePageNearbyShareHighVisibilityPage) {
  RunSettingsTest(
      "nearby_share_page/nearby_share_high_visibility_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       NearbySharePageNearbyShareReceiveDialog) {
  RunSettingsTest("nearby_share_page/nearby_share_receive_dialog_test.js");
}

class OSSettingsNearbyShareTestSharingEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsNearbyShareTestSharingEnabled() {
    scoped_feature_list_.InitAndEnableFeature(::features::kNearbySharing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsNearbyShareTestSharingEnabled,
                       NearbySharePageNearbyShareSubpage) {
  RunSettingsTest("nearby_share_page/nearby_share_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OncMojoTest) {
  RunSettingsTest("onc_mojo_test.js");
}

class OSSettingsOsA11yTestPdfOcrEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsOsA11yTestPdfOcrEnabled() {
    scoped_feature_list_.InitAndEnableFeature(::features::kPdfOcr);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsOsA11yTestPdfOcrEnabled, OsA11yPage) {
  RunSettingsTest("os_a11y_page/os_a11y_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsOsA11yTestPdfOcrEnabled,
                       OsA11yPageLiveCaptionSection) {
  RunSettingsTest("os_a11y_page/live_caption_section_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsA11yPageAudioAndCaptionsPage) {
  RunSettingsTest("os_a11y_page/audio_and_captions_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsA11yPageChromeVoxSubpage) {
  RunSettingsTest("os_a11y_page/chromevox_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsA11yPageCursorAndTouchpadPage) {
  RunSettingsTest("os_a11y_page/cursor_and_touchpad_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestMouseKeysEnabled,
                       OsA11yPageCursorAndTouchpadPage) {
  RunSettingsTest("os_a11y_page/cursor_and_touchpad_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsA11yPageChangeDictationLocaleDialog) {
  RunSettingsTest("os_a11y_page/change_dictation_locale_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsA11yPageDisplayAndMagnificationSubpage) {
  RunSettingsTest("os_a11y_page/display_and_magnification_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestReducedAnimationsEnabled,
                       OsA11yPageDisplayAndMagnificationSubpage) {
  RunSettingsTest("os_a11y_page/display_and_magnification_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsA11yPageKeyboardAndTextInputPage) {
  RunSettingsTest("os_a11y_page/keyboard_and_text_input_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestCaretBlinkSettingEnabled,
                       OsA11yPageKeyboardAndTextInputPage) {
  RunSettingsTest("os_a11y_page/keyboard_and_text_input_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsA11yPageKioskMode) {
  RunSettingsTest("os_a11y_page/os_a11y_page_kiosk_mode_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsA11yPageSelectToSpeakSubpage) {
  RunSettingsTest("os_a11y_page/select_to_speak_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsA11yPageSwitchAccessActionAssignmentDialog) {
  RunSettingsTest(
      "os_a11y_page/switch_access_action_assignment_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsA11yPageSwitchAccessSetupGuideDialog) {
  RunSettingsTest("os_a11y_page/switch_access_setup_guide_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsA11yPageSwitchAccessSubpage) {
  RunSettingsTest("os_a11y_page/switch_access_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsOsA11yTestPdfOcrEnabled,
                       OsA11yPageTextToSpeechSubpage) {
  RunSettingsTest("os_a11y_page/text_to_speech_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsA11yPageTtsVoiceSubpage) {
  RunSettingsTest("os_a11y_page/tts_voice_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsAboutPage_AllBuilds) {
  RunSettingsTest("os_about_page/os_about_page_test.js",
                  "runMochaSuite('<os-about-page> AllBuilds')");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsAboutPage_AllBuilds) {
  RunSettingsTest("os_about_page/os_about_page_test.js",
                  "runMochaSuite('<os-about-page> AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsAboutPage_OfficialBuild) {
  RunSettingsTest("os_about_page/os_about_page_test.js",
                  "runMochaSuite('<os-about-page> OfficialBuild')");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsAboutPage_OfficialBuild) {
  RunSettingsTest("os_about_page/os_about_page_test.js",
                  "runMochaSuite('<os-about-page> OfficialBuild')");
}
#endif

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsAboutPageChannelSwitcherDialog) {
  RunSettingsTest("os_about_page/channel_switcher_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAboutPageConsumerAutoUpdateToggleDialog) {
  RunSettingsTest("os_about_page/consumer_auto_update_toggle_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAboutPageDetailedBuildInfoSubpage) {
  RunSettingsTest("os_about_page/detailed_build_info_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsAboutPageEditHostnameDialog) {
  RunSettingsTest("os_about_page/edit_hostname_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsAppsPage) {
  RunSettingsTest("os_apps_page/os_apps_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled, OsAppsPageRevamp) {
  RunSettingsTest("os_apps_page/os_apps_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestAppParentalControlsEnabled,
                       OsAppsPageWithAppParentalControls) {
  RunSettingsTest("os_apps_page/os_apps_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageAppDetailsItem) {
  RunSettingsTest("os_apps_page/app_management_page/app_details_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageAppDetailView) {
  RunSettingsTest("os_apps_page/app_management_page/app_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageAppItem) {
  RunSettingsTest("os_apps_page/app_management_page/app_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageAppLanguageItem) {
  RunSettingsTest("os_apps_page/app_management_page/app_language_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsAppsPageAppManagementPage) {
  RunSettingsTest(
      "os_apps_page/app_management_page/app_management_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageArcDetailView) {
  RunSettingsTest("os_apps_page/app_management_page/arc_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageBorealisDetailView) {
  RunSettingsTest(
      "os_apps_page/app_management_page/borealis_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageChromeAppDetailView) {
  RunSettingsTest(
      "os_apps_page/app_management_page/chrome_app_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageDomSwitch) {
  RunSettingsTest("os_apps_page/app_management_page/dom_switch_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageMainView) {
  RunSettingsTest("os_apps_page/app_management_page/main_view_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPagePinToShelfItem) {
  RunSettingsTest("os_apps_page/app_management_page/pin_to_shelf_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPagePluginVmDetailView) {
  RunSettingsTest(
      "os_apps_page/app_management_page/plugin_vm_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPagePwaDetailView) {
  RunSettingsTest("os_apps_page/app_management_page/pwa_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageReducers) {
  RunSettingsTest("os_apps_page/app_management_page/reducers_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageResizeLockItem) {
  RunSettingsTest("os_apps_page/app_management_page/resize_lock_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageSupportedLinksItem) {
  RunSettingsTest(
      "os_apps_page/app_management_page/supported_links_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPagePermissionItem) {
  RunSettingsTest("os_apps_page/app_management_page/permission_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageFileHandlingItem) {
  RunSettingsTest(
      "os_apps_page/app_management_page/file_handling_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppManagementPageUninstallButton) {
  RunSettingsTest("os_apps_page/app_management_page/uninstall_button_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsAppsPageAppNotificationsPageAppNotificationRow) {
  RunSettingsTest(
      "os_apps_page/app_notifications_page/app_notification_row_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsAppsPageAppNotificationsSubpage) {
  RunSettingsTest(
      "os_apps_page/app_notifications_page/app_notifications_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsAppsPageAppNotificationsSubpageRevamp) {
  RunSettingsTest(
      "os_apps_page/app_notifications_page/app_notifications_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsMochaTestRevampEnabled,
    OsAppsPageAppNotificationsPageAppNotificationsManagerSubpage) {
  RunSettingsTest(
      "os_apps_page/app_notifications_page/"
      "app_notifications_manager_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(
    OSSettingsMochaTest,
    OsAppsPageManageIsolatedWebAppsPageManageIsolatedWebAppsSubpage) {
  RunSettingsTest(
      "os_apps_page/manage_isolated_web_apps_page/"
      "manage_isolated_web_apps_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsBluetoothPage) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsBluetoothChangeDeviceNameDialog) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_change_device_name_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestSplitEnabled,
                       OsBluetoothPageOsBluetoothDeviceDetailSubpage) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_device_detail_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsBluetoothDevicesSubpage) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_devices_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsBluetoothPairingDialog) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_pairing_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsBluetoothSavedDevicesList) {
  RunSettingsTest("os_bluetooth_page/os_saved_devices_list_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsBluetoothSavedDevicesSubpage) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_saved_devices_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsBluetoothPageOsBluetoothSummary) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_summary_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsBluetoothTrueWirelessImages) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_true_wireless_images_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsPairedBluetoothList) {
  RunSettingsTest("os_bluetooth_page/os_paired_bluetooth_list_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsBluetoothPageOsPairedBluetoothListItem) {
  RunSettingsTest("os_bluetooth_page/os_paired_bluetooth_list_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsFilesPage) {
  RunSettingsTest("os_files_page/os_files_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsFilesPageFilesSettingsCard) {
  RunSettingsTest("os_files_page/files_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsFilesPageGoogleDrivePage) {
  RunSettingsTest("os_files_page/google_drive_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsFilesPageOneDrivePage) {
  RunSettingsTest("os_files_page/one_drive_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsFilesPageOfficePage) {
  RunSettingsTest("os_files_page/office_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsFilesPageSmbSharesPage) {
  RunSettingsTest("os_files_page/smb_shares_page_test.js");
}

class OSSettingsFilesTestCrosComponentsAndJellyEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsFilesTestCrosComponentsAndJellyEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            chromeos::features::kCrosComponents,
            chromeos::features::kJelly,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsFilesTestCrosComponentsAndJellyEnabled,
                       OsFilesPageSmbSharesPageJelly) {
  RunSettingsTest("os_files_page/smb_shares_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsLanguagesPageAppLanguagesPage) {
  RunSettingsTest("os_languages_page/app_languages_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsLanguagesPageInputMethodOptionsPage) {
  RunSettingsTest("os_languages_page/input_method_options_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsLanguagesPageInputPage) {
  RunSettingsTest("os_languages_page/input_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsLanguagesPageLanguageSettingsCard) {
  RunSettingsTest("os_languages_page/language_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsLanguagesPageOsClearPersonalizationDataPage) {
  RunSettingsTest(
      "os_languages_page/os_clear_personalization_data_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsLanguagesPageV2) {
  RunSettingsTest("os_languages_page/os_languages_page_v2_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsLanguagesPageOsEditDictionaryPage) {
  RunSettingsTest("os_languages_page/os_edit_dictionary_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsPageAvailability) {
  RunSettingsTest("os_page_availability_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsPageAvailabilityRevamp) {
  RunSettingsTest("os_page_availability_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsPeoplePage) {
  RunSettingsTest("os_people_page/os_people_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled, OsPeoplePageRevamp) {
  RunSettingsTest("os_people_page/os_people_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPeoplePageAddUserDialog) {
  RunSettingsTest("os_people_page/add_user_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsPeoplePageFingerprintListSubpage) {
  RunSettingsTest("os_people_page/fingerprint_list_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPeoplePageOsSyncControlsSubpage) {
  RunSettingsTest("os_people_page/os_sync_controls_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsPeoplePagePersonalizationOptions) {
  RunSettingsTest("os_people_page/personalization_options_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrintingPage) {
  RunSettingsTest("os_printing_page/os_printing_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsPrintingPagePrintingSettingsCard) {
  RunSettingsTest("os_printing_page/printing_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrintingPageCupsPrintServer) {
  RunSettingsTest("os_printing_page/cups_print_server_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrintingPageCupsPrinterDialog) {
  RunSettingsTest("os_printing_page/cups_printer_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsPrintingPageCupsPrinterLandingPage) {
  RunSettingsTest("os_printing_page/cups_printer_landing_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrintingPageCupsPrintersEntry) {
  RunSettingsTest("os_printing_page/cups_printers_entry_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrintingPageCupsPrinterPage) {
  RunSettingsTest("os_printing_page/cups_printer_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrintingPagePrinterStatus) {
  RunSettingsTest("os_printing_page/printer_status_test.js");
}

//  TODO(https://crbug.com/329315636): test is flaky.
IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, DISABLED_OsPrivacyPage) {
  RunSettingsTest("os_privacy_page/os_privacy_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrivacyPageManageUsersSubpage) {
  RunSettingsTest("os_privacy_page/manage_users_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsPrivacyPagePrivacyHubAppPermissionRow) {
  RunSettingsTest("os_privacy_page/privacy_hub_app_permission_row_test.js");
}

class OSSettingsPrivacyTestPrivacyHubV0AndPermissionsEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsPrivacyTestPrivacyHubV0AndPermissionsEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kCrosPrivacyHubAppPermissions,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsPrivacyTestPrivacyHubV0AndPermissionsEnabled,
                       OsPrivacyPagePrivacyHubCameraSubpage) {
  RunSettingsTest("os_privacy_page/privacy_hub_camera_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsPrivacyTestPrivacyHubV0AndPermissionsEnabled,
                       OsPrivacyPagePrivacyHubMicrophoneSubpage) {
  RunSettingsTest("os_privacy_page/privacy_hub_microphone_subpage_test.js");
}

class OSSettingsPrivacyTestPrivacyHubAndV0Enabled : public OSSettingsMochaTest {
 protected:
  OSSettingsPrivacyTestPrivacyHubAndV0Enabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kCrosPrivacyHub,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsPrivacyTestPrivacyHubAndV0Enabled,
                       OsPrivacyPagePrivacyHubGeolocationSubpage) {
  RunSettingsTest("os_privacy_page/privacy_hub_geolocation_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsPrivacyTestPrivacyHubAndV0Enabled,
                       OsPrivacyPagePrivacyHubGeolocationAdvancedSubpage) {
  RunSettingsTest(
      "os_privacy_page/privacy_hub_geolocation_advanced_subpage_test.js");
}

class OSSettingsPrivacyTestPrivacyHubV0Enabled : public OSSettingsMochaTest {};

using OSSettingsPrivacyPageTestPrivacyHubSubpage =
    OSSettingsPrivacyTestPrivacyHubV0Enabled;

IN_PROC_BROWSER_TEST_F(OSSettingsPrivacyPageTestPrivacyHubSubpage, AllBuilds) {
  RunSettingsTest("os_privacy_page/privacy_hub_subpage_test.js",
                  "runMochaSuite('<settings-privacy-hub-subpage> AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(OSSettingsPrivacyPageTestPrivacyHubSubpage,
                       OfficialBuild) {
  RunSettingsTest("os_privacy_page/privacy_hub_subpage_test.js",
                  "runMochaSuite('<os-settings-privacy-page> OfficialBuild')");
}
#endif

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrivacyPageSecureDnsInput) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SettingsSecureDnsInput')");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrivacyPageSecureDns) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SettingsSecureDns')");
}

class OSSettingsPrivacyTestDeprecateDnsDialogEnabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsPrivacyTestDeprecateDnsDialogEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOsSettingsDeprecateDnsDialog);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsPrivacyTestDeprecateDnsDialogEnabled,
                       OsPrivacyPageDeprecateDnsDialog) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SecureDnsDialog')");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsPrivacyPageSecureDnsDialog) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SecureDnsDialog')");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsPrivacyPageSecureDnsDialog) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SecureDnsDialog')");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsPrivacyPageSmartPrivacySubpage) {
  RunSettingsTest("os_privacy_page/smart_privacy_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsResetPage) {
  RunSettingsTest("os_reset_page/os_reset_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsResetPageResetSettingsCard) {
  RunSettingsTest("os_reset_page/reset_settings_card_test.js");
}

class OSSettingsResetTestSanitizeEnabledRevampDisabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsResetTestSanitizeEnabledRevampDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kSanitize,
        },
        /*disabled=*/{
            ash::features::kOsSettingsRevampWayfinding,
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsResetTestSanitizeEnabledRevampDisabled,
                       OsResetPageResetSettingsCardWithSanitize) {
  RunSettingsTest("os_reset_page/reset_settings_card_test.js");
}

class OSSettingsResetTestSanitizeAndRevampDisabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsResetTestSanitizeAndRevampDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {},
        /*disabled=*/{
            ash::features::kSanitize,
            ash::features::kOsSettingsRevampWayfinding,
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsResetTestSanitizeAndRevampDisabled,
                       OsResetPageResetSettingsCardWithoutSanitize) {
  RunSettingsTest("os_reset_page/reset_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsSearchPage) {
  RunSettingsTest("os_search_page/os_search_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsSearchPageGoogleAssistantSubpage) {
  RunSettingsTest("os_search_page/google_assistant_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSearchPageSearchAndAssistantSettingsCard) {
  RunSettingsTest("os_search_page/search_and_assistant_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSearchPageSearchEngine) {
  RunSettingsTest("os_search_page/search_engine_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSearchPageSearchEngineRevamp) {
  RunSettingsTest("os_search_page/search_engine_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsSearchPageSearchSubpage) {
  RunSettingsTest("os_search_page/search_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsSettingsMain) {
  RunSettingsTest("os_settings_main/os_settings_main_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled, OsSettingsMainRevamp) {
  RunSettingsTest("os_settings_main/os_settings_main_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsSettingsMainManagedFootnote) {
  RunSettingsTest("os_settings_main/managed_footnote_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsSettingsMenu) {
  RunSettingsTest("os_settings_menu/os_settings_menu_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled, OsSettingsMenuRevamp) {
  RunSettingsTest("os_settings_menu/os_settings_menu_revamp_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled, OsSettingsMenuItem) {
  RunSettingsTest("os_settings_menu/menu_item_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, OsSettingsRoutes) {
  RunSettingsTest("os_settings_routes_test.js");
}

using OSSettingsTestSearchBox = OSSettingsMochaTest;

IN_PROC_BROWSER_TEST_F(OSSettingsTestSearchBox, AllBuilds) {
  RunSettingsTest("os_settings_search_box/os_settings_search_box_test.js",
                  "runMochaSuite('<os-settings-search-box> AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(OSSettingsTestSearchBox, OfficialBuild) {
  RunSettingsTest("os_settings_search_box/os_settings_search_box_test.js",
                  "runMochaSuite('<os-settings-search-box> OfficialBuild')");
}
#endif

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsSettingsUi) {
  RunSettingsTest("os_settings_ui/os_settings_ui_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled, OsSettingsUiRevamp) {
  RunSettingsTest("os_settings_ui/os_settings_ui_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSettingsUiAboutPage) {
  RunSettingsTest("os_settings_ui/os_settings_ui_about_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsSettingsUiHats) {
  RunSettingsTest("os_settings_ui/os_settings_ui_hats_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSettingsUiHatsRevamp) {
  RunSettingsTest("os_settings_ui/os_settings_ui_hats_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsSettingsUiMenu) {
  RunSettingsTest("os_settings_ui/os_settings_ui_menu_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSettingsUiMenuRevamp) {
  RunSettingsTest("os_settings_ui/os_settings_ui_menu_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSettingsUiPageAvailability) {
  RunSettingsTest("os_settings_ui/os_settings_ui_page_availability_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSettingsUiPageAvailabilityRevamp) {
  RunSettingsTest("os_settings_ui/os_settings_ui_page_availability_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSettingsUiPageVisibilityRevamp) {
  RunSettingsTest(
      "os_settings_ui/os_settings_ui_page_visibility_revamp_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSettingsUiScrollRestoration) {
  RunSettingsTest("os_settings_ui/scroll_restoration_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled, OsSettingsUiToolbar) {
  RunSettingsTest("os_settings_ui/os_settings_ui_toolbar_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSettingsUiToolbarRevamp) {
  RunSettingsTest("os_settings_ui/os_settings_ui_toolbar_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSettingsUiUserActionRecorder) {
  RunSettingsTest("os_settings_ui/user_action_recorder_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSettingsUiUserActionRecorderRevamp) {
  RunSettingsTest("os_settings_ui/user_action_recorder_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsPeoplePageAccountManagerSettingsCard) {
  RunSettingsTest("os_people_page/account_manager_settings_card_test.js");
}

class OSSettingsMochaTestRevampAndLacrosOnlyDisabled
    : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestRevampAndLacrosOnlyDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            ash::standalone_browser::features::kLacrosOnly,
            ash::features::kOsSettingsRevampWayfinding,
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampAndLacrosOnlyDisabled,
                       OsPeoplePageAccountManagerSubpage) {
  RunSettingsTest("os_people_page/account_manager_subpage_test.js");
}

class OSSettingsMochaTestLacrosEnabledRevampDisabled
    : public OSSettingsMochaTestLacrosOnlyEnabled {
 protected:
  OSSettingsMochaTestLacrosEnabledRevampDisabled() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kOsSettingsRevampWayfinding);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OSSettingsMochaTestLacrosEnabledRevampDisabled,
    OsPeoplePageAccountManagerSubpageWithArcAccountRestrictionsEnabled) {
  RunSettingsTest("os_people_page/account_manager_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsPeoplePageAdditionalAccountsSettingsCard) {
  RunSettingsTest("os_people_page/additional_accounts_settings_card_test.js");
}

class OSSettingsMochaTestLacrosAndRevampEnabled
    : public OSSettingsMochaTestLacrosOnlyEnabled {
 protected:
  OSSettingsMochaTestLacrosAndRevampEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOsSettingsRevampWayfinding);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OSSettingsMochaTestLacrosAndRevampEnabled,
    OsPeoplePageAdditionalAccountsSettingsCardWithLacrosEnabled) {
  RunSettingsTest("os_people_page/additional_accounts_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, ParentalControlsPage) {
  RunSettingsTest("parental_controls_page/parental_controls_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, ParentalControlsSettingsCard) {
  RunSettingsTest(
      "parental_controls_page/parental_controls_settings_card_test.js");
}

class OSSettingsRevampMochaTestFasterSplitScreenEnabled
    : public OSSettingsRevampMochaTest {
 public:
  OSSettingsRevampMochaTestFasterSplitScreenEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/{ash::features::kFasterSplitScreenSetup},
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestFasterSplitScreenEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestFasterSplitScreenEnabled::DescribeParams);

class OSSettingsRevampMochaTestFasterSplitScreenDisabled
    : public OSSettingsRevampMochaTest {
 public:
  OSSettingsRevampMochaTestFasterSplitScreenDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/{},
        /*disabled=*/{ash::features::kFasterSplitScreenSetup});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestFasterSplitScreenDisabled,
    testing::Bool(),
    OSSettingsRevampMochaTestFasterSplitScreenDisabled::DescribeParams);

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestFasterSplitScreenEnabled,
                       PersonalizationPageWithPersonalizationHub) {
  RunSettingsTest(
      "personalization_page/"
      "personalization_page_with_personalization_hub_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestFasterSplitScreenDisabled,
                       PersonalizationPageWithPersonalizationHub) {
  RunSettingsTest(
      "personalization_page/"
      "personalization_page_with_personalization_hub_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       PersonalizationPageWithPersonalizationHub) {
  RunSettingsTest(
      "personalization_page/"
      "personalization_page_with_personalization_hub_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled, Router) {
  RunSettingsTest("router_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest, SettingsSchedulerSlider) {
  RunSettingsTest(
      "settings_scheduler_slider/settings_scheduler_slider_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPage) {
  RunSettingsTest("system_preferences_page/system_preferences_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageDateTimeSettingsCard) {
  RunSettingsTest("date_time_page/date_time_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageFilesSettingsCard) {
  RunSettingsTest("os_files_page/files_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageLanguageSettingsCard) {
  RunSettingsTest("os_languages_page/language_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageResetSettingsCard) {
  RunSettingsTest("os_reset_page/reset_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageSearchAndAssistantSettingsCard) {
  RunSettingsTest("os_search_page/search_and_assistant_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageStartupSettingsCard) {
  RunSettingsTest("system_preferences_page/startup_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageMultitaskingSettingsCard) {
  RunSettingsTest("system_preferences_page/multitasking_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       SystemPreferencesPageStorageAndPowerSettingsCard) {
  RunSettingsTest(
      "system_preferences_page/storage_and_power_settings_card_test.js");
}

}  // namespace ash::settings
