// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_

#include "chrome/browser/ui/webui/settings/chromeos/constants/routes_util.h"

#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"

namespace chromeos {
namespace settings {

const char kOsSignOutSubPage[] = "osSignOut";

// Any changes here need to be kept in sync with chrome_new_window_client.cc
// TODO(khorimoto): Instead of listing out every path, use an enum parameter.
bool IsOSSettingsSubPage(const std::string& sub_page) {
  static const char* const kPaths[] = {
      // Network section.
      chromeos::settings::mojom::kNetworkSectionPath,
      chromeos::settings::mojom::kEthernetDetailsSubpagePath,
      chromeos::settings::mojom::kWifiNetworksSubpagePath,
      chromeos::settings::mojom::kWifiDetailsSubpagePath,
      chromeos::settings::mojom::kKnownNetworksSubpagePath,
      chromeos::settings::mojom::kMobileDataNetworksSubpagePath,
      chromeos::settings::mojom::kCellularDetailsSubpagePath,
      chromeos::settings::mojom::kTetherDetailsSubpagePath,
      chromeos::settings::mojom::kVpnDetailsSubpagePath,

      // Bluetooth section.
      chromeos::settings::mojom::kBluetoothSectionPath,
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath,

      // MultiDevice section.
      chromeos::settings::mojom::kMultiDeviceSectionPath,
      chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath,
      chromeos::settings::mojom::kSmartLockSubpagePath,
      chromeos::settings::mojom::kNearbyShareSubpagePath,

      // People section.
      chromeos::settings::mojom::kPeopleSectionPath,
      chromeos::settings::mojom::kMyAccountsSubpagePath,
      chromeos::settings::mojom::kSyncSubpagePath,
      chromeos::settings::mojom::kSecurityAndSignInSubpagePath,
      chromeos::settings::mojom::kFingerprintSubpagePath,
      chromeos::settings::mojom::kManageOtherPeopleSubpagePath,
      chromeos::settings::mojom::kKerberosAccountsSubpagePath,

      // Device section.
      chromeos::settings::mojom::kDeviceSectionPath,
      chromeos::settings::mojom::kPointersSubpagePath,
      chromeos::settings::mojom::kKeyboardSubpagePath,
      chromeos::settings::mojom::kStylusSubpagePath,
      chromeos::settings::mojom::kDisplaySubpagePath,
      chromeos::settings::mojom::kStorageSubpagePath,
      chromeos::settings::mojom::kExternalStorageSubpagePath,
      chromeos::settings::mojom::kPowerSubpagePath,

      // Personalization section.
      chromeos::settings::mojom::kPersonalizationSectionPath,
      chromeos::settings::mojom::kChangePictureSubpagePath,
      chromeos::settings::mojom::kAmbientModeSubpagePath,

      // Search and Assistant section.
      chromeos::settings::mojom::kSearchAndAssistantSectionPath,
      chromeos::settings::mojom::kAssistantSubpagePath,

      // Apps section.
      chromeos::settings::mojom::kAppsSectionPath,
      chromeos::settings::mojom::kAppManagementSubpagePath,
      chromeos::settings::mojom::kAppDetailsSubpagePath,
      chromeos::settings::mojom::kGooglePlayStoreSubpagePath,
      chromeos::settings::mojom::kPluginVmSharedPathsSubpagePath,

      // Crostini section.
      chromeos::settings::mojom::kCrostiniSectionPath,
      chromeos::settings::mojom::kCrostiniDetailsSubpagePath,
      chromeos::settings::mojom::kCrostiniManageSharedFoldersSubpagePath,
      chromeos::settings::mojom::kCrostiniUsbPreferencesSubpagePath,
      chromeos::settings::mojom::kCrostiniBackupAndRestoreSubpagePath,
      chromeos::settings::mojom::kCrostiniDevelopAndroidAppsSubpagePath,
      chromeos::settings::mojom::kCrostiniPortForwardingSubpagePath,

      // On Startup section.
      chromeos::settings::mojom::kOnStartupSectionPath,

      // Date and Time section.
      chromeos::settings::mojom::kDateAndTimeSectionPath,
      chromeos::settings::mojom::kTimeZoneSubpagePath,

      // Privacy and Security section.
      chromeos::settings::mojom::kPrivacyAndSecuritySectionPath,

      // Languages and Input section.
      chromeos::settings::mojom::kLanguagesAndInputSectionPath,
      chromeos::settings::mojom::kLanguagesAndInputDetailsSubpagePath,
      chromeos::settings::mojom::kManageInputMethodsSubpagePath,
      chromeos::settings::mojom::kSmartInputsSubpagePath,
      chromeos::settings::mojom::kInputMethodOptionsSubpagePath,
      chromeos::settings::mojom::kLanguagesSubpagePath,
      chromeos::settings::mojom::kInputSubpagePath,
      chromeos::settings::mojom::kEditDictionarySubpagePath,

      // Files section.
      chromeos::settings::mojom::kFilesSectionPath,
      chromeos::settings::mojom::kNetworkFileSharesSubpagePath,

      // Printing section.
      chromeos::settings::mojom::kPrintingSectionPath,
      chromeos::settings::mojom::kPrintingDetailsSubpagePath,

      // Accessibility section.
      chromeos::settings::mojom::kAccessibilitySectionPath,
      chromeos::settings::mojom::kManageAccessibilitySubpagePath,
      chromeos::settings::mojom::kTextToSpeechSubpagePath,
      chromeos::settings::mojom::kSwitchAccessOptionsSubpagePath,
      chromeos::settings::mojom::kCaptionsSubpagePath,

      // Reset section.
      chromeos::settings::mojom::kResetSectionPath,

      // About Chrome OS section.
      chromeos::settings::mojom::kAboutChromeOsSectionPath,
      chromeos::settings::mojom::kAboutChromeOsDetailsSubpagePath,
      chromeos::settings::mojom::kDetailedBuildInfoSubpagePath,

      // Kerberos section.
      chromeos::settings::mojom::kKerberosSectionPath,
      chromeos::settings::mojom::kKerberosAccountsV2SubpagePath,
  };

  // Sub-pages may have query parameters, e.g. networkDetail?guid=123456.
  std::string sub_page_without_query = sub_page;
  std::string::size_type input_index = sub_page.find('?');
  if (input_index != std::string::npos)
    sub_page_without_query.resize(input_index);

  for (const char* p : kPaths) {
    std::string path_without_query = p;
    std::string::size_type path_index = sub_page.find('?');
    if (path_index != std::string::npos)
      path_without_query.resize(path_index);

    if (sub_page_without_query == path_without_query)
      return true;
  }

  // Special case - sign-out dialog:
  if (sub_page_without_query == kOsSignOutSubPage)
    return true;

  return false;
}

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
