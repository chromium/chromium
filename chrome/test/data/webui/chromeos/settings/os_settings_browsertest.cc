// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_names.h"
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
    // chromeos/settings/.
    const std::string path_with_parent_directory = base::StrCat({
        "chromeos/settings/",
        test_path,
    });
    RunTest(path_with_parent_directory, trigger);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kEnableHostnameSetting};
};

/* Start Test Classes */

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

class OSSettingsRevampMochaTestWithExistingUser
    : public OSSettingsRevampMochaTest {
 public:
  OSSettingsRevampMochaTestWithExistingUser() {
    UserDataAuthClient::Get()->InitializeFake();
  }

  void SetUpOnMainThread() override {
    OSSettingsRevampMochaTest::SetUpOnMainThread();
    const auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(
        user_manager::StubAccountId());
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(account_id);
    AddGaiaPassword(account_id, test::kGaiaPassword);
  }

  void AddGaiaPassword(const cryptohome::AccountIdentifier& account_id,
                       std::string password) {
    user_data_auth::AuthFactor auth_factor;
    user_data_auth::AuthInput auth_input;

    auth_factor.set_label(ash::kCryptohomeGaiaKeyLabel);
    auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

    ash::Key key(std::move(password));
    key.Transform(ash::Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                  SystemSaltGetter::ConvertRawSaltToHexString(
                      FakeCryptohomeMiscClient::GetStubSystemSalt()));
    auth_input.mutable_password_input()->set_secret(key.GetSecret());

    // Add the password key to the user.
    FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
        account_id, auth_factor, auth_input);
  }
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestWithExistingUser,
    testing::Bool(),
    OSSettingsRevampMochaTestWithExistingUser::DescribeParams);

class OSSettingsMochaTestRevampEnabled : public OSSettingsMochaTest {
 protected:
  OSSettingsMochaTestRevampEnabled() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kOsSettingsRevampWayfinding}, {});
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

class OSSettingsMochaTestApnRevamp : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{ash::features::kApnRevamp};
};

INSTANTIATE_TEST_SUITE_P(RevampParameterized,
                         OSSettingsMochaTestApnRevamp,
                         testing::Bool(),
                         OSSettingsMochaTestApnRevamp::DescribeParams);

class OSSettingsCrostiniTestRevamp : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsCrostiniTestRevamp() { fake_crostini_features_.SetAll(true); }

 private:
  crostini::FakeCrostiniFeatures fake_crostini_features_;
};

INSTANTIATE_TEST_SUITE_P(RevampParameterized,
                         OSSettingsCrostiniTestRevamp,
                         testing::Bool(),
                         OSSettingsCrostiniTestRevamp::DescribeParams);

class OSSettingsCrostiniTestRevampDisabled
    : public OSSettingsMochaTestRevampDisabled {
 protected:
  OSSettingsCrostiniTestRevampDisabled() {
    fake_crostini_features_.SetAll(true);
  }

 private:
  crostini::FakeCrostiniFeatures fake_crostini_features_;
};

class OSSettingsRevampMochaTestReducedAnimationsEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityReducedAnimations};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestReducedAnimationsEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestReducedAnimationsEnabled::DescribeParams);

class OSSettingsMochaTestMagnifierFollowsChromeVoxEnabled
    : public OSSettingsMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityMagnifierFollowsChromeVox};
};

class OSSettingsMochaTestMagnifierFollowsStsEnabled
    : public OSSettingsMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityMagnifierFollowsSts};
};

class OSSettingsMochaTestOverscrollFeatureEnabled : public OSSettingsMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityOverscrollSettingFeature};
};

class OSSettingsRevampMochaTestMouseKeysEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityMouseKeys};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestMouseKeysEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestMouseKeysEnabled::DescribeParams);

class OSSettingsRevampMochaTestFaceGazeEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityFaceGaze};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestFaceGazeEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestFaceGazeEnabled::DescribeParams);

class OSSettingsRevampMochaTestGraduationEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kGraduation};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestGraduationEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestGraduationEnabled::DescribeParams);

class OSSettingsRevampMochaTestCaretBlinkSettingEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityCaretBlinkIntervalSetting};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestCaretBlinkSettingEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestCaretBlinkSettingEnabled::DescribeParams);

class OSSettingsRevampMochaTestFlashNotificationsEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kAccessibilityFlashScreenFeature};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestFlashNotificationsEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestFlashNotificationsEnabled::DescribeParams);

class OSSettingsRevampMochaTestAppParentalControlsEnabled
    : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsRevampMochaTestAppParentalControlsEnabled() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOnDeviceAppControls,
         features::kForceOnDeviceAppControlsForAllRegions},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampMochaTestAppParentalControlsEnabled,
    testing::Bool(),
    OSSettingsRevampMochaTestAppParentalControlsEnabled::DescribeParams);

class OSSettingsDeviceTestPeripheralAndSplitAndRevampEnabled
    : public OSSettingsMochaTestRevampEnabled {
 protected:
  OSSettingsDeviceTestPeripheralAndSplitAndRevampEnabled() {
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

class OSSettingsRevampDeviceTestPeripheralAndSplitEnabled
    : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsRevampDeviceTestPeripheralAndSplitEnabled() {
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

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
    testing::Bool(),
    OSSettingsRevampDeviceTestPeripheralAndSplitEnabled::DescribeParams);

class OSSettingsRevampDeviceTestSplitAndAltAndFKeyEnabled
    : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsRevampDeviceTestSplitAndAltAndFKeyEnabled() {
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

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampDeviceTestSplitAndAltAndFKeyEnabled,
    testing::Bool(),
    OSSettingsRevampDeviceTestSplitAndAltAndFKeyEnabled::DescribeParams);

class OSSettingsRevampMochaTestSplitEnabled : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kInputDeviceSettingsSplit};
};

INSTANTIATE_TEST_SUITE_P(RevampParameterized,
                         OSSettingsRevampMochaTestSplitEnabled,
                         testing::Bool(),
                         OSSettingsRevampMochaTestSplitEnabled::DescribeParams);

class OSSettingsRevampDeviceTestPeripheralEnabledSplitDisabled
    : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsRevampDeviceTestPeripheralEnabledSplitDisabled() {
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

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampDeviceTestPeripheralEnabledSplitDisabled,
    testing::Bool(),
    OSSettingsRevampDeviceTestPeripheralEnabledSplitDisabled::DescribeParams);

class OSSettingsRevampDeviceTestSplitAndBacklightEnabled
    : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsRevampDeviceTestSplitAndBacklightEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::features::kInputDeviceSettingsSplit,
            ash::features::kEnableKeyboardBacklightControlInSettings,
        },
        /*disabled=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampDeviceTestSplitAndBacklightEnabled,
    testing::Bool(),
    OSSettingsRevampDeviceTestSplitAndBacklightEnabled::DescribeParams);

class OSSettingsRevampDeviceTestAltAndSplitAndBacklightEnabled
    : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsRevampDeviceTestAltAndSplitAndBacklightEnabled() {
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

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampDeviceTestAltAndSplitAndBacklightEnabled,
    testing::Bool(),
    OSSettingsRevampDeviceTestAltAndSplitAndBacklightEnabled::DescribeParams);

class OSSettingsRevampNearbyShareTestSharingEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kNearbySharing};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampNearbyShareTestSharingEnabled,
    testing::Bool(),
    OSSettingsRevampNearbyShareTestSharingEnabled::DescribeParams);

class OSSettingsRevampOsA11yTestMainNodeAnnotationsEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kMainNodeAnnotations};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampOsA11yTestMainNodeAnnotationsEnabled,
    testing::Bool(),
    OSSettingsRevampOsA11yTestMainNodeAnnotationsEnabled::DescribeParams);

class OSSettingsRevampFilesTestCrosComponentsAndJellyEnabled
    : public OSSettingsRevampMochaTest {
 protected:
  OSSettingsRevampFilesTestCrosComponentsAndJellyEnabled() {
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

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampFilesTestCrosComponentsAndJellyEnabled,
    testing::Bool(),
    OSSettingsRevampFilesTestCrosComponentsAndJellyEnabled::DescribeParams);

class OSSettingsRevampPrivacyTestPrivacyHubV0AndPermissionsEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kCrosPrivacyHubAppPermissions};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampPrivacyTestPrivacyHubV0AndPermissionsEnabled,
    testing::Bool(),
    OSSettingsRevampPrivacyTestPrivacyHubV0AndPermissionsEnabled::
        DescribeParams);

class OSSettingsRevampPrivacyTestPrivacyHubAndV0Enabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kCrosPrivacyHub};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampPrivacyTestPrivacyHubAndV0Enabled,
    testing::Bool(),
    OSSettingsRevampPrivacyTestPrivacyHubAndV0Enabled::DescribeParams);

class OSSettingsRevampPrivacyTestDeprecateDnsDialogEnabled
    : public OSSettingsRevampMochaTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kOsSettingsDeprecateDnsDialog};
};

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampPrivacyTestDeprecateDnsDialogEnabled,
    testing::Bool(),
    OSSettingsRevampPrivacyTestDeprecateDnsDialogEnabled::DescribeParams);

using OSSettingsRevampPrivacyPageTestPrivacyHubSubpage =
    OSSettingsRevampMochaTest;

INSTANTIATE_TEST_SUITE_P(
    RevampParameterized,
    OSSettingsRevampPrivacyPageTestPrivacyHubSubpage,
    testing::Bool(),
    OSSettingsRevampPrivacyPageTestPrivacyHubSubpage::DescribeParams);

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

using OSSettingsTestSearchBox = OSSettingsRevampMochaTest;

INSTANTIATE_TEST_SUITE_P(RevampParameterized,
                         OSSettingsTestSearchBox,
                         testing::Bool(),
                         OSSettingsTestSearchBox::DescribeParams);

using OSSettingsRevampTestOsAboutPage = OSSettingsRevampMochaTest;

INSTANTIATE_TEST_SUITE_P(RevampParameterized,
                         OSSettingsRevampTestOsAboutPage,
                         testing::Bool(),
                         OSSettingsRevampTestOsAboutPage::DescribeParams);

/* End Test Classes */

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

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, BaseRowMixin) {
  RunSettingsTest("controls/v2/base_row_mixin_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, PrefControlMixinInternal) {
  RunSettingsTest("controls/v2/pref_control_mixin_internal_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsDropdownRow) {
  RunSettingsTest("controls/v2/settings_dropdown_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsDropdownV2) {
  RunSettingsTest("controls/v2/settings_dropdown_v2_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsRow) {
  RunSettingsTest("controls/v2/settings_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsSliderRow) {
  RunSettingsTest("controls/v2/settings_slider_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsSliderV2) {
  RunSettingsTest("controls/v2/settings_slider_v2_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsToggleV2) {
  RunSettingsTest("controls/v2/settings_toggle_v2_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageBruschettaSubpage) {
  RunSettingsTest("crostini_page/bruschetta_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageCrostiniArcAdb) {
  RunSettingsTest("crostini_page/crostini_arc_adb_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageCrostiniExportImport) {
  RunSettingsTest("crostini_page/crostini_export_import_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageCrostiniExtraContainersSubpage) {
  RunSettingsTest("crostini_page/crostini_extra_containers_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsCrostiniTestRevampDisabled, CrostiniPage) {
  RunSettingsTest("crostini_page/crostini_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageCrostiniPortForwarding) {
  RunSettingsTest("crostini_page/crostini_port_forwarding_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageCrostiniSettingsCard) {
  RunSettingsTest("crostini_page/crostini_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageCrostiniSharedUsbDevices) {
  RunSettingsTest("crostini_page/crostini_shared_usb_devices_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsCrostiniTestRevamp,
                       CrostiniPageCrostiniSubpage) {
  RunSettingsTest("crostini_page/crostini_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, DateTimePage) {
  RunSettingsTest("date_time_page/date_time_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       DateTimePageDateTimeSettingsCard) {
  RunSettingsTest("date_time_page/date_time_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       DateTimePageTimezoneSelector) {
  RunSettingsTest("date_time_page/timezone_selector_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, DateTimePageTimezoneSubpage) {
  RunSettingsTest("date_time_page/timezone_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePage) {
  RunSettingsTest("device_page/device_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, DevicePageAudioPage) {
  RunSettingsTest("device_page/audio_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       DevicePageCustomizeButtonDropdownItem) {
  RunSettingsTest("device_page/customize_button_dropdown_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeButtonRow) {
  RunSettingsTest("device_page/customize_button_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeButtonSelect) {
  RunSettingsTest("device_page/customize_button_select_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeButtonsSubsection) {
  RunSettingsTest("device_page/customize_buttons_subsection_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeMouseButtonsSubpage) {
  RunSettingsTest("device_page/customize_mouse_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizePenButtonsSubpage) {
  RunSettingsTest("device_page/customize_pen_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageCustomizeTabletButtonsSubpage) {
  RunSettingsTest("device_page/customize_tablet_buttons_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageDisplayPage) {
  RunSettingsTest("device_page/display_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       DevicePageDisplaySettingsMojoInterfaceProvider) {
  RunSettingsTest(
      "device_page/display_settings_mojo_interface_provider_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageDragAndDropManager) {
  RunSettingsTest("device_page/drag_and_drop_manager_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       DevicePageFakeCrosAudioConfig) {
  RunSettingsTest("device_page/fake_cros_audio_config_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       DevicePageFakeInputDeviceSettingsProvider) {
  RunSettingsTest("device_page/fake_input_device_settings_provider_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestSplitAndAltAndFKeyEnabled,
                       DevicePageFKeyRow) {
  RunSettingsTest("device_page/fkey_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageGraphicsTabletSubpage) {
  RunSettingsTest("device_page/graphics_tablet_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       DevicePageInputDeviceMojoInterfaceProvider) {
  RunSettingsTest("device_page/input_device_mojo_interface_provider_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageKeyCombinationInputDialog) {
  RunSettingsTest("device_page/key_combination_input_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralEnabledSplitDisabled,
                       DevicePageKeyboard) {
  RunSettingsTest("device_page/keyboard_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       DevicePageKeyboardSixPackKeyRow) {
  RunSettingsTest("device_page/keyboard_six_pack_key_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePagePerDeviceAppInstalledRow) {
  RunSettingsTest("device_page/per_device_app_installed_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePagePerDeviceInstallRow) {
  RunSettingsTest("device_page/per_device_install_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestSplitAndBacklightEnabled,
                       DevicePagePerDeviceKeyboard) {
  RunSettingsTest("device_page/per_device_keyboard_test.js");
}

// TODO(b/367799335): Re-enable this test.
IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestAltAndSplitAndBacklightEnabled,
                       DISABLED_DevicePagePerDeviceKeyboardRemapKeys) {
  RunSettingsTest("device_page/per_device_keyboard_remap_keys_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestAltAndSplitAndBacklightEnabled,
                       DevicePagePerDeviceKeyboardSubsection) {
  RunSettingsTest("device_page/per_device_keyboard_subsection_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       DevicePagePerDeviceMouse) {
  RunSettingsTest("device_page/per_device_mouse_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePagePerDeviceMouseSubsection) {
  RunSettingsTest("device_page/per_device_mouse_subsection_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       DevicePagePerDevicePointingStick) {
  RunSettingsTest("device_page/per_device_pointing_stick_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       DevicePagePerDevicePointingStickSubsection) {
  RunSettingsTest("device_page/per_device_pointing_stick_subsection_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePagePerDeviceSubsectionHeader) {
  RunSettingsTest("device_page/per_device_subsection_header_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       DevicePagePerDeviceTouchpad) {
  RunSettingsTest("device_page/per_device_touchpad_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       DevicePagePerDeviceTouchpadSubsection) {
  RunSettingsTest("device_page/per_device_touchpad_subsection_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralEnabledSplitDisabled,
                       DevicePagePointers) {
  RunSettingsTest("device_page/pointers_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, DevicePagePower) {
  RunSettingsTest("device_page/power_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, DevicePagePowerRevamp) {
  RunSettingsTest("device_page/power_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       DevicePagePrintingSettingsCard) {
  RunSettingsTest("os_printing_page/printing_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsDeviceTestPeripheralAndSplitAndRevampEnabled,
                       DevicePageInputSettings) {
  RunSettingsTest("device_page/device_page_input_settings_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageStorage) {
  RunSettingsTest("device_page/storage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampDeviceTestPeripheralAndSplitEnabled,
                       DevicePageStylus) {
  RunSettingsTest("device_page/stylus_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, GuestOsSharedPaths) {
  RunSettingsTest("guest_os/guest_os_shared_paths_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, GuestOsSharedUsbDevices) {
  RunSettingsTest("guest_os/guest_os_shared_usb_devices_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsMochaTestApnRevamp, InternetPageApnSubpage) {
  RunSettingsTest("internet_page/apn_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, InternetPageApnDetailDialog) {
  RunSettingsTest("internet_page/apn_detail_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsMochaTestApnRevamp, InternetPage) {
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

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageCellularSetupDialog) {
  RunSettingsTest("internet_page/cellular_setup_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageEsimRemoveProfileDialog) {
  RunSettingsTest("internet_page/esim_remove_profile_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageEsimInstallErrorDialog) {
  RunSettingsTest("internet_page/esim_install_error_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageEsimRenameDialog) {
  RunSettingsTest("internet_page/esim_rename_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageHotspotConfigDialog) {
  RunSettingsTest("internet_page/hotspot_config_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, InternetPageHotspotSubpage) {
  RunSettingsTest("internet_page/hotspot_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageHotspotSummaryItem) {
  RunSettingsTest("internet_page/hotspot_summary_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, InternetPageInternetConfig) {
  RunSettingsTest("internet_page/internet_config_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageInternetDetailMenu) {
  RunSettingsTest("internet_page/internet_detail_menu_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsMochaTestApnRevamp,
                       InternetPageInternetDetailSubpage) {
  RunSettingsTest("internet_page/internet_detail_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageInternetKnownNetworksSubpage) {
  RunSettingsTest("internet_page/internet_known_networks_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageInternetSubpageMenu) {
  RunSettingsTest("internet_page/internet_subpage_menu_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, InternetPageInternetSubpage) {
  RunSettingsTest("internet_page/internet_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageNetworkAlwaysOnVpn) {
  RunSettingsTest("internet_page/network_always_on_vpn_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageNetworkDeviceInfoDialog) {
  RunSettingsTest("internet_page/network_device_info_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageNetworkProxySection) {
  RunSettingsTest("internet_page/network_proxy_section_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, InternetPageNetworkSummary) {
  RunSettingsTest("internet_page/network_summary_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageNetworkSummaryItem) {
  RunSettingsTest("internet_page/network_summary_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPagePasspointSubpage) {
  RunSettingsTest("internet_page/passpoint_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPagePasspointRemoveDialog) {
  RunSettingsTest("internet_page/passpoint_remove_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageSettingsTrafficCounters) {
  RunSettingsTest("internet_page/settings_traffic_counters_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       InternetPageTetherConnectionDialog) {
  RunSettingsTest("internet_page/tether_connection_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, KerberosPage) {
  RunSettingsTest("kerberos_page/kerberos_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       KerberosPageKerberosAccountsSubpage) {
  RunSettingsTest("kerberos_page/kerberos_accounts_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       KerberosPageKerberosAddAccountDialog) {
  RunSettingsTest("kerberos_page/kerberos_add_account_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, KeyboardShortcutBanner) {
  RunSettingsTest("keyboard_shortcut_banner/keyboard_shortcut_banner_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, LockScreenSubpage) {
  RunSettingsTest("lock_screen_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, MainPageContainer) {
  RunSettingsTest("main_page_container/main_page_container_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MainPageContainerPageDisplayer) {
  RunSettingsTest("main_page_container/page_displayer_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       MainPageContainerRouteNavigation) {
  RunSettingsTest("main_page_container/route_navigation_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, MultidevicePage) {
  RunSettingsTest("multidevice_page/multidevice_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceFeatureItem) {
  RunSettingsTest("multidevice_page/multidevice_feature_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceFeatureToggle) {
  RunSettingsTest("multidevice_page/multidevice_feature_toggle_test.js");
}

IN_PROC_BROWSER_TEST_P(
    OSSettingsRevampMochaTest,
    MultidevicePageMultideviceNotificationAccessSetupDialog) {
  RunSettingsTest(
      "multidevice_page/multidevice_notification_access_setup_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultidevicePermissionsSetupDialog) {
  RunSettingsTest(
      "multidevice_page/multidevice_permissions_setup_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceSmartlockItem) {
  RunSettingsTest("multidevice_page/multidevice_smartlock_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceSubPage) {
  RunSettingsTest("multidevice_page/multidevice_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultiDevicePageMultideviceCombinedSetupItem) {
  RunSettingsTest("multidevice_page/multidevice_combined_setup_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceTaskContinuationDisabledLink) {
  RunSettingsTest(
      "multidevice_page/multidevice_task_continuation_disabled_link_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceTaskContinuationItem) {
  RunSettingsTest(
      "multidevice_page/multidevice_task_continuation_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceWifiSyncDisabledLink) {
  RunSettingsTest(
      "multidevice_page/multidevice_wifi_sync_disabled_link_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       MultidevicePageMultideviceWifiSyncItem) {
  RunSettingsTest("multidevice_page/multidevice_wifi_sync_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       NearbySharePageNearbyShareConfirmPage) {
  RunSettingsTest("nearby_share_page/nearby_share_confirm_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       NearbySharePageNearbyShareHighVisibilityPage) {
  RunSettingsTest(
      "nearby_share_page/nearby_share_high_visibility_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       NearbySharePageNearbyShareReceiveDialog) {
  RunSettingsTest("nearby_share_page/nearby_share_receive_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampNearbyShareTestSharingEnabled,
                       NearbySharePageNearbyShareSubpage) {
  RunSettingsTest("nearby_share_page/nearby_share_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OncMojoTest) {
  RunSettingsTest("onc_mojo_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsA11yPage) {
  RunSettingsTest("os_a11y_page/os_a11y_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageLiveCaptionSection) {
  RunSettingsTest("os_a11y_page/live_caption_section_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageLiveTranslateSection) {
  RunSettingsTest("os_a11y_page/live_translate_section_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageAudioAndCaptionsPage) {
  RunSettingsTest("os_a11y_page/audio_and_captions_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestFlashNotificationsEnabled,
                       OsA11yPageAudioAndCaptionsPage) {
  RunSettingsTest("os_a11y_page/audio_and_captions_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsA11yPageChromeVoxSubpage) {
  RunSettingsTest("os_a11y_page/chromevox_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageCursorAndTouchpadPage) {
  RunSettingsTest("os_a11y_page/cursor_and_touchpad_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestMouseKeysEnabled,
                       OsA11yPageCursorAndTouchpadPage) {
  RunSettingsTest("os_a11y_page/cursor_and_touchpad_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestOverscrollFeatureEnabled,
                       OsA11yPageCursorAndTouchpadPage) {
  RunSettingsTest("os_a11y_page/cursor_and_touchpad_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestFaceGazeEnabled,
                       OsA11yPageFaceGazeSubpage) {
  RunSettingsTest("os_a11y_page/facegaze_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestFaceGazeEnabled,
                       OsA11yPageFaceGazeCursorCard) {
  RunSettingsTest("os_a11y_page/facegaze_cursor_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestFaceGazeEnabled,
                       OsA11yPageFaceGazeActionsCard) {
  RunSettingsTest("os_a11y_page/facegaze_actions_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestFaceGazeEnabled,
                       OsA11yPageFaceGazeActionsAddDialog) {
  RunSettingsTest("os_a11y_page/facegaze_actions_add_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageChangeDictationLocaleDialog) {
  RunSettingsTest("os_a11y_page/change_dictation_locale_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageDisplayAndMagnificationSubpage) {
  RunSettingsTest("os_a11y_page/display_and_magnification_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestReducedAnimationsEnabled,
                       OsA11yPageDisplayAndMagnificationSubpage) {
  RunSettingsTest("os_a11y_page/display_and_magnification_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestMagnifierFollowsChromeVoxEnabled,
                       OsA11yPageDisplayAndMagnificationSubpage) {
  RunSettingsTest("os_a11y_page/display_and_magnification_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestMagnifierFollowsStsEnabled,
                       OsA11yPageDisplayAndMagnificationSubpage) {
  RunSettingsTest("os_a11y_page/display_and_magnification_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageKeyboardAndTextInputPage) {
  RunSettingsTest("os_a11y_page/keyboard_and_text_input_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestCaretBlinkSettingEnabled,
                       OsA11yPageKeyboardAndTextInputPageCaret) {
  RunSettingsTest("os_a11y_page/keyboard_and_text_input_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsA11yPageKioskMode) {
  RunSettingsTest("os_a11y_page/os_a11y_page_kiosk_mode_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageSelectToSpeakSubpage) {
  RunSettingsTest("os_a11y_page/select_to_speak_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageSwitchAccessActionAssignmentDialog) {
  RunSettingsTest(
      "os_a11y_page/switch_access_action_assignment_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageSwitchAccessSetupGuideDialog) {
  RunSettingsTest("os_a11y_page/switch_access_setup_guide_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageSwitchAccessSubpage) {
  RunSettingsTest("os_a11y_page/switch_access_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsA11yPageTextToSpeechSubpage) {
  RunSettingsTest("os_a11y_page/text_to_speech_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampOsA11yTestMainNodeAnnotationsEnabled,
                       OsA11yPageAxAnnotationsSection) {
  RunSettingsTest("os_a11y_page/ax_annotations_section_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsA11yPageTtsVoiceSubpage) {
  RunSettingsTest("os_a11y_page/tts_voice_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampTestOsAboutPage, AllBuilds) {
  RunSettingsTest("os_about_page/os_about_page_test.js",
                  "runMochaSuite('<os-about-page> AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_P(OSSettingsRevampTestOsAboutPage, OfficialBuild) {
  RunSettingsTest("os_about_page/os_about_page_test.js",
                  "runMochaSuite('<os-about-page> OfficialBuild')");
}
#endif

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAboutPageChannelSwitcherDialog) {
  RunSettingsTest("os_about_page/channel_switcher_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAboutPageConsumerAutoUpdateToggleDialog) {
  RunSettingsTest("os_about_page/consumer_auto_update_toggle_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAboutPageDetailedBuildInfoSubpage) {
  RunSettingsTest("os_about_page/detailed_build_info_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAboutPageEditHostnameDialog) {
  RunSettingsTest("os_about_page/edit_hostname_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsAppsPage) {
  RunSettingsTest("os_apps_page/os_apps_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestAppParentalControlsEnabled,
                       OsAppsPageWithAppParentalControls) {
  RunSettingsTest("os_apps_page/os_apps_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageAppDetailsItem) {
  RunSettingsTest("os_apps_page/app_management_page/app_details_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageAppDetailView) {
  RunSettingsTest("os_apps_page/app_management_page/app_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageAppItem) {
  RunSettingsTest("os_apps_page/app_management_page/app_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageAppLanguageItem) {
  RunSettingsTest("os_apps_page/app_management_page/app_language_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsAppsPageAppManagementPage) {
  RunSettingsTest(
      "os_apps_page/app_management_page/app_management_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageArcDetailView) {
  RunSettingsTest("os_apps_page/app_management_page/arc_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageBorealisDetailView) {
  RunSettingsTest(
      "os_apps_page/app_management_page/borealis_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageChromeAppDetailView) {
  RunSettingsTest(
      "os_apps_page/app_management_page/chrome_app_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageDomSwitch) {
  RunSettingsTest("os_apps_page/app_management_page/dom_switch_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageMainView) {
  RunSettingsTest("os_apps_page/app_management_page/main_view_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPagePinToShelfItem) {
  RunSettingsTest("os_apps_page/app_management_page/pin_to_shelf_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPagePluginVmDetailView) {
  RunSettingsTest(
      "os_apps_page/app_management_page/plugin_vm_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPagePwaDetailView) {
  RunSettingsTest("os_apps_page/app_management_page/pwa_detail_view_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageReducers) {
  RunSettingsTest("os_apps_page/app_management_page/reducers_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageResizeLockItem) {
  RunSettingsTest("os_apps_page/app_management_page/resize_lock_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageSupportedLinksItem) {
  RunSettingsTest(
      "os_apps_page/app_management_page/supported_links_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPagePermissionItem) {
  RunSettingsTest("os_apps_page/app_management_page/permission_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageFileHandlingItem) {
  RunSettingsTest(
      "os_apps_page/app_management_page/file_handling_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppManagementPageUninstallButton) {
  RunSettingsTest("os_apps_page/app_management_page/uninstall_button_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppNotificationsPageAppNotificationRow) {
  RunSettingsTest(
      "os_apps_page/app_notifications_page/app_notification_row_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsAppsPageAppNotificationsSubpage) {
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

IN_PROC_BROWSER_TEST_P(
    OSSettingsRevampMochaTestAppParentalControlsEnabled,
    OsAppsPageAppParentalControlsPageAppParentalControlsSubpage) {
  RunSettingsTest(
      "os_apps_page/app_parental_controls_page/"
      "app_parental_controls_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestAppParentalControlsEnabled,
                       OsAppsPageAppParentalControlsPageBlockAppItem) {
  RunSettingsTest(
      "os_apps_page/app_parental_controls_page/"
      "block_app_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestAppParentalControlsEnabled,
                       OsAppsPageAppParentalControlsPageAppSetupPinDialog) {
  RunSettingsTest(
      "os_apps_page/app_parental_controls_page/"
      "app_setup_pin_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(
    OSSettingsRevampMochaTestAppParentalControlsEnabled,
    OsAppsPageAppParentalControlsPageAppVerifyPinDialogTest) {
  RunSettingsTest(
      "os_apps_page/app_parental_controls_page/"
      "app_verify_pin_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(
    OSSettingsRevampMochaTest,
    OsAppsPageManageIsolatedWebAppsPageManageIsolatedWebAppsSubpage) {
  RunSettingsTest(
      "os_apps_page/manage_isolated_web_apps_page/"
      "manage_isolated_web_apps_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsBluetoothPage) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsBluetoothChangeDeviceNameDialog) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_change_device_name_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestSplitEnabled,
                       OsBluetoothPageOsBluetoothDeviceDetailSubpage) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_device_detail_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsBluetoothDevicesSubpage) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_devices_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsBluetoothPairingDialog) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_pairing_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsBluetoothSavedDevicesList) {
  RunSettingsTest("os_bluetooth_page/os_saved_devices_list_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsBluetoothSavedDevicesSubpage) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_saved_devices_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsBluetoothSummary) {
  RunSettingsTest("os_bluetooth_page/os_bluetooth_summary_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsBluetoothTrueWirelessImages) {
  RunSettingsTest(
      "os_bluetooth_page/os_bluetooth_true_wireless_images_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsPairedBluetoothList) {
  RunSettingsTest("os_bluetooth_page/os_paired_bluetooth_list_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsBluetoothPageOsPairedBluetoothListItem) {
  RunSettingsTest("os_bluetooth_page/os_paired_bluetooth_list_item_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsFilesPage) {
  RunSettingsTest("os_files_page/os_files_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsFilesPageFilesSettingsCard) {
  RunSettingsTest("os_files_page/files_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsFilesPageGoogleDrivePage) {
  RunSettingsTest("os_files_page/google_drive_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsFilesPageOneDrivePage) {
  RunSettingsTest("os_files_page/one_drive_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsFilesPageOfficePage) {
  RunSettingsTest("os_files_page/office_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsFilesPageSmbSharesPage) {
  RunSettingsTest("os_files_page/smb_shares_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampFilesTestCrosComponentsAndJellyEnabled,
                       OsFilesPageSmbSharesPageJelly) {
  RunSettingsTest("os_files_page/smb_shares_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsLanguagesPageAppLanguagesPage) {
  RunSettingsTest("os_languages_page/app_languages_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsLanguagesPageInputMethodOptionsPage) {
  RunSettingsTest("os_languages_page/input_method_options_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsLanguagesPageInputPage) {
  RunSettingsTest("os_languages_page/input_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsLanguagesPageLanguageSettingsCard) {
  RunSettingsTest("os_languages_page/language_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsLanguagesPageOsClearPersonalizationDataPage) {
  RunSettingsTest(
      "os_languages_page/os_clear_personalization_data_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsLanguagesPageV2) {
  RunSettingsTest("os_languages_page/os_languages_page_v2_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsLanguagesPageOsEditDictionaryPage) {
  RunSettingsTest("os_languages_page/os_edit_dictionary_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsPageAvailability) {
  RunSettingsTest("os_page_availability_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsPeoplePage) {
  RunSettingsTest("os_people_page/os_people_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestGraduationEnabled,
                       OsPeoplePage) {
  RunSettingsTest("os_people_page/os_people_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPeoplePageAccountManagerSettingsCard) {
  RunSettingsTest("os_people_page/account_manager_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsPeoplePageAccountManagerSubpage) {
  RunSettingsTest("os_people_page/account_manager_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTest,
                       OsPeoplePageAdditionalAccountsSettingsCard) {
  RunSettingsTest("os_people_page/additional_accounts_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsPeoplePageAddUserDialog) {
  RunSettingsTest("os_people_page/add_user_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPeoplePageFingerprintListSubpage) {
  RunSettingsTest("os_people_page/fingerprint_list_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestGraduationEnabled,
                       OsPeoplePageGraduationSettingsCard) {
  RunSettingsTest("os_people_page/graduation_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPeoplePageOsSyncControlsSubpage) {
  RunSettingsTest("os_people_page/os_sync_controls_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPeoplePagePersonalizationOptions) {
  RunSettingsTest("os_people_page/personalization_options_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsPrintingPage) {
  RunSettingsTest("os_printing_page/os_printing_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsPrintingPagePrintingSettingsCard) {
  RunSettingsTest("os_printing_page/printing_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrintingPageCupsPrintServer) {
  RunSettingsTest("os_printing_page/cups_print_server_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrintingPageCupsPrinterDialog) {
  RunSettingsTest("os_printing_page/cups_printer_dialog_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrintingPageCupsPrinterLandingPage) {
  RunSettingsTest("os_printing_page/cups_printer_landing_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrintingPageCupsPrintersEntry) {
  RunSettingsTest("os_printing_page/cups_printers_entry_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrintingPageCupsPrinterPage) {
  RunSettingsTest("os_printing_page/cups_printer_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsPrintingPagePrinterStatus) {
  RunSettingsTest("os_printing_page/printer_status_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTestWithExistingUser,
                       OsPrivacyPage) {
  RunSettingsTest("os_privacy_page/os_privacy_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrivacyPageManageUsersSubpage) {
  RunSettingsTest("os_privacy_page/manage_users_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrivacyPagePrivacyHubAppPermissionRow) {
  RunSettingsTest("os_privacy_page/privacy_hub_app_permission_row_test.js");
}

IN_PROC_BROWSER_TEST_P(
    OSSettingsRevampPrivacyTestPrivacyHubV0AndPermissionsEnabled,
    OsPrivacyPagePrivacyHubCameraSubpage) {
  RunSettingsTest("os_privacy_page/privacy_hub_camera_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(
    OSSettingsRevampPrivacyTestPrivacyHubV0AndPermissionsEnabled,
    OsPrivacyPagePrivacyHubMicrophoneSubpage) {
  RunSettingsTest("os_privacy_page/privacy_hub_microphone_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampPrivacyTestPrivacyHubAndV0Enabled,
                       OsPrivacyPagePrivacyHubGeolocationSubpage) {
  RunSettingsTest("os_privacy_page/privacy_hub_geolocation_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampPrivacyTestPrivacyHubAndV0Enabled,
                       OsPrivacyPagePrivacyHubGeolocationAdvancedSubpage) {
  RunSettingsTest(
      "os_privacy_page/privacy_hub_geolocation_advanced_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampPrivacyPageTestPrivacyHubSubpage,
                       AllBuilds) {
  RunSettingsTest("os_privacy_page/privacy_hub_subpage_test.js",
                  "runMochaSuite('<settings-privacy-hub-subpage> AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_P(OSSettingsRevampPrivacyPageTestPrivacyHubSubpage,
                       OfficialBuild) {
  RunSettingsTest("os_privacy_page/privacy_hub_subpage_test.js",
                  "runMochaSuite('<os-settings-privacy-page> OfficialBuild')");
}
#endif

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsPrivacyPageSecureDnsInput) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SettingsSecureDnsInput')");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsPrivacyPageSecureDns) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SettingsSecureDns')");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampPrivacyTestDeprecateDnsDialogEnabled,
                       OsPrivacyPageDeprecateDnsDialog) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SecureDnsDialog')");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrivacyPageSecureDnsDialog) {
  RunSettingsTest("os_privacy_page/secure_dns_test.js",
                  "runMochaSuite('SecureDnsDialog')");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsPrivacyPageSmartPrivacySubpage) {
  RunSettingsTest("os_privacy_page/smart_privacy_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsResetPage) {
  RunSettingsTest("os_reset_page/os_reset_page_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsResetPageResetSettingsCard) {
  RunSettingsTest("os_reset_page/reset_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsResetTestSanitizeEnabledRevampDisabled,
                       OsResetPageResetSettingsCardWithSanitize) {
  RunSettingsTest("os_reset_page/reset_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsResetTestSanitizeAndRevampDisabled,
                       OsResetPageResetSettingsCardWithoutSanitize) {
  RunSettingsTest("os_reset_page/reset_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSearchPage) {
  RunSettingsTest("os_search_page/os_search_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsSearchPageGoogleAssistantSubpage) {
  RunSettingsTest("os_search_page/google_assistant_subpage_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSearchPageSearchAndAssistantSettingsCard) {
  RunSettingsTest("os_search_page/search_and_assistant_settings_card_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSearchPageSearchEngine) {
  RunSettingsTest("os_search_page/search_engine_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSearchPageSearchSubpage) {
  RunSettingsTest("os_search_page/search_subpage_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSettingsMain) {
  RunSettingsTest("os_settings_main/os_settings_main_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsSettingsMainManagedFootnote) {
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

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSettingsRoutes) {
  RunSettingsTest("os_settings_routes_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsTestSearchBox, AllBuilds) {
  RunSettingsTest("os_settings_search_box/os_settings_search_box_test.js",
                  "runMochaSuite('<os-settings-search-box> AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_P(OSSettingsTestSearchBox, OfficialBuild) {
  RunSettingsTest("os_settings_search_box/os_settings_search_box_test.js",
                  "runMochaSuite('<os-settings-search-box> OfficialBuild')");
}
#endif

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSettingsUi) {
  RunSettingsTest("os_settings_ui/os_settings_ui_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       OsSettingsUiAboutPage) {
  RunSettingsTest("os_settings_ui/os_settings_ui_about_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSettingsUiHats) {
  RunSettingsTest("os_settings_ui/os_settings_ui_hats_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSettingsUiMenu) {
  RunSettingsTest("os_settings_ui/os_settings_ui_menu_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsSettingsUiPageAvailability) {
  RunSettingsTest("os_settings_ui/os_settings_ui_page_availability_test.js");
}

IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampEnabled,
                       OsSettingsUiPageVisibilityRevamp) {
  RunSettingsTest(
      "os_settings_ui/os_settings_ui_page_visibility_revamp_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSettingsUiPrefSync) {
  RunSettingsTest("os_settings_ui/os_settings_ui_pref_sync_test.js");
}

// TODO(b/354464273) Fix this flaky test.
IN_PROC_BROWSER_TEST_F(OSSettingsMochaTestRevampDisabled,
                       DISABLED_OsSettingsUiScrollRestoration) {
  RunSettingsTest("os_settings_ui/scroll_restoration_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, OsSettingsUiToolbar) {
  RunSettingsTest("os_settings_ui/os_settings_ui_toolbar_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       OsSettingsUiUserActionRecorder) {
  RunSettingsTest("os_settings_ui/user_action_recorder_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, ParentalControlsPage) {
  RunSettingsTest("parental_controls_page/parental_controls_page_test.js");
}

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest,
                       ParentalControlsSettingsCard) {
  RunSettingsTest(
      "parental_controls_page/parental_controls_settings_card_test.js");
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

IN_PROC_BROWSER_TEST_P(OSSettingsRevampMochaTest, SettingsSchedulerSlider) {
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
