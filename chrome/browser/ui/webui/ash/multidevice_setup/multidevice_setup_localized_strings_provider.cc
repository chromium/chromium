// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_localized_strings_provider.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/multidevice_setup_resources.h"
#include "chrome/grit/multidevice_setup_resources_map.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/url_provider.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::multidevice_setup {

namespace {

const char16_t kFootnoteMarker[] = u"*";

constexpr webui::LocalizedString kLocalizedStringsWithoutPlaceholders[] = {
    {"accept", IDS_MULTIDEVICE_SETUP_ACCEPT_LABEL},
    {"back", IDS_MULTIDEVICE_SETUP_BACK_LABEL},
    {"cancel", IDS_CANCEL},
    {"done", IDS_DONE},
    {"noThanks", IDS_NO_THANKS},
    {"passwordPageHeader", IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_HEADER},
    {"enterPassword", IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_ENTER_PASSWORD_LABEL},
    {"wrongPassword", IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_WRONG_PASSWORD_LABEL},
    {"startSetupPageMultipleDeviceHeader",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_MULTIPLE_DEVICE_HEADER},
    {"startSetupPageSingleDeviceHeader",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_SINGLE_DEVICE_HEADER},
    {"startSetupPageOfflineDeviceOption",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_OFFLINE_DEVICE_OPTION},
    {"startSetupPageFootnote", IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_FOOTNOTE},
    {"startSetupPageFeatureMirrorPhoneNotifications",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_MIRROR_PHONE_NOTIFICATIONS},
    {"startSetupPageFeatureWifiSyncTitle",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_WIFI_SYNC_TITLE},
    {"startSetupPageFeatureWifiSyncDescription",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_WIFI_SYNC_DESCRIPTION},
    {"startSetupPageFeatureCameraRoll",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_CAMERA_ROLL},
    {"startSetupPageFeaturePhoneHubTitle",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_PHONE_HUB_TITLE},
    {"startSetupPageFeaturePhoneHubDescription",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_PHONE_HUB_DESCRIPTION},
    {"startSetupPageFeatureInstantTetheringTitle",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_INSTANT_TETHERING_TITLE},
    {"startSetupPageFeatureInstantTetheringDescription",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_INSTANT_TETHERING_DESCRIPTION},
    {"startSetupPageFeatureSmartLockTitle",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_SMART_LOCK_TITLE},
    {"startSetupPageFeatureListInstallApps",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_INSTALL_APPS_DESCRIPTION},
    {"startSetupPageFeatureListAddFeatures",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_ADD_FEATURES},
    {"setupSucceededPageHeader",
     IDS_MULTIDEVICE_SETUP_SETUP_SUCCEEDED_PAGE_HEADER},
    {"setupSucceededPageMessage",
     IDS_MULTIDEVICE_SETUP_SETUP_SUCCEEDED_PAGE_MESSAGE},
    {"startSetupPageHeader", IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_HEADER},
    {"startSetupPageAfterQuickStartHeader",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_AFTER_QUICK_START_HEADER},
    {"tryAgain", IDS_MULTIDEVICE_SETUP_TRY_AGAIN_LABEL},
    {"dialogAccessibilityTitle",
     IDS_MULTIDEVICE_SETUP_DIALOG_ACCESSIBILITY_TITLE},
    {"startSetupPageFeatureListHeader",
     IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_FEATURE_LIST_HEADER}};

struct LocalizedStringWithName {
  LocalizedStringWithName(const char* name,
                          const std::u16string& localized_string)
      : name(name), localized_string(localized_string) {}

  const char* name;
  std::u16string localized_string;
};

const std::vector<LocalizedStringWithName>&
GetLocalizedStringsWithPlaceholders() {
  static const base::NoDestructor<std::vector<LocalizedStringWithName>>
      localized_strings([] {
        std::vector<LocalizedStringWithName> localized_strings;

        // TODO(crbug.com/964547): Refactor so that any change to these strings
        // will surface in both the OOBE and post-OOBE UIs without having to
        // adjust both localization calls separately.
        localized_strings.emplace_back(
            "startSetupPageMessage",
            l10n_util::GetStringFUTF16(
                IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_MESSAGE,
                ui::GetChromeOSDeviceName(), kFootnoteMarker,
                base::UTF8ToUTF16(
                    GetBoardSpecificBetterTogetherSuiteLearnMoreUrl().spec())));

        localized_strings.emplace_back(
            "startSetupPageFeatureListAwm",
            l10n_util::GetStringFUTF16(
                IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_AWM_DESCRIPTION,
                base::UTF8ToUTF16(
                    GetBoardSpecificMessagesLearnMoreUrl().spec())));

        localized_strings.emplace_back(
            "startSetupPageFeatureSmartLockDescription",
            l10n_util::GetStringFUTF16(
                IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_SMART_LOCK_DESCRIPTION,
                ui::GetChromeOSDeviceName()));

        return localized_strings;
      }());

  return *localized_strings;
}

}  //  namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedStrings(kLocalizedStringsWithoutPlaceholders);

  html_source->AddBoolean("phoneHubEnabled",
                          base::FeatureList::IsEnabled(features::kPhoneHub));

  html_source->AddBoolean("wifiSyncEnabled", base::FeatureList::IsEnabled(
                                                 features::kWifiSyncAndroid));

  for (const auto& entry : GetLocalizedStringsWithPlaceholders()) {
    html_source->AddString(entry.name, entry.localized_string);
  }

  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");
}

void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder) {
  for (const auto& entry : kLocalizedStringsWithoutPlaceholders)
    builder->Add(entry.name, entry.id);

  // TODO(crbug.com/964547): Refactor so that any change to these strings will
  // surface in both the OOBE and post-OOBE UIs without having to adjust both
  // localization calls separately.
  builder->AddF("startSetupPageMessage",
                IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_MESSAGE,
                ui::GetChromeOSDeviceName(), kFootnoteMarker,
                base::UTF8ToUTF16(
                    GetBoardSpecificBetterTogetherSuiteLearnMoreUrl().spec()));

  builder->AddF(
      "startSetupPageFeatureListAwm",
      IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_AWM_DESCRIPTION,
      base::UTF8ToUTF16(GetBoardSpecificMessagesLearnMoreUrl().spec()));

  builder->AddF("startSetupPageFeatureSmartLockDescription",
                IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_SMART_LOCK_DESCRIPTION,
                ui::GetChromeOSDeviceName());
}

}  // namespace ash::multidevice_setup
