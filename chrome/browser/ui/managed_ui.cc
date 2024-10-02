// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include <optional>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/webui/policy_data_utils.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif


namespace chrome {

namespace {

enum ManagementStringType : size_t {
  BROWSER_MANAGED = 0,
  BROWSER_MANAGED_BY = 1,
  BROWSER_PROFILE_SAME_MANAGED_BY = 2,
  BROWSER_PROFILE_DIFFERENT_MANAGED_BY = 3,
  BROWSER_MANAGED_PROFILE_MANAGED_BY = 4,
  PROFILE_MANAGED_BY = 5,
  SUPERVISED = 6,
  NOT_MANAGED = 7
};

const char* g_device_manager_for_testing = nullptr;

bool ShouldDisplayManagedByParentUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  // Don't display the managed by parent UI on ChromeOS, because similar UI is
  // displayed at the OS level.
  return false;
#else
  return profile && profile->IsChild();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

ManagementStringType GetManagementStringType(Profile* profile) {
  if (!enterprise_util::IsBrowserManaged(profile) &&
      ShouldDisplayManagedByParentUi(profile)) {
    return SUPERVISED;
  }

  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  bool account_managed = management_service->IsAccountManaged();
  bool device_managed = management_service->IsBrowserManaged();
  bool known_device_manager = device_manager && !device_manager->empty();
  bool known_account_manager = account_manager && !account_manager->empty();

  // TODO (crbug://1227786) Add a PROFILE_MANAGED case, and ensure the following
  // tests are setup so that we do not have a managed account without an account
  // manager:  WebKioskTest.CloseSettingWindowIfOnlyOpen,
  // WebKioskTest.NotExitIfCloseSettingsWindow, WebKioskTest.OpenA11ySettings.
  if (account_managed && !known_account_manager) {
    account_managed = false;
  }

  if (!account_managed && !device_managed) {
    return NOT_MANAGED;
  }

  if (!device_managed) {
    return known_account_manager ? PROFILE_MANAGED_BY : BROWSER_MANAGED;
  }

  if (!account_managed) {
    return known_device_manager ? BROWSER_MANAGED_BY : BROWSER_MANAGED;
  }

  CHECK(known_account_manager);
  if (known_device_manager) {
    return *account_manager == *device_manager
               ? BROWSER_PROFILE_SAME_MANAGED_BY
               : BROWSER_PROFILE_DIFFERENT_MANAGED_BY;
  }

  return BROWSER_MANAGED_PROFILE_MANAGED_BY;
}

}  // namespace

ScopedDeviceManagerForTesting::ScopedDeviceManagerForTesting(
    const char* manager) {
  previous_manager_ = g_device_manager_for_testing;
  g_device_manager_for_testing = manager;
}

ScopedDeviceManagerForTesting::~ScopedDeviceManagerForTesting() {
  g_device_manager_for_testing = previous_manager_;
}

std::optional<std::string> GetEnterpriseAccountDomain(const Profile& profile) {
  if (g_browser_process->profile_manager()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile.GetPath());
    if (entry && !entry->GetHostedDomain().empty() &&
        entry->GetHostedDomain() != kNoHostedDomainFound) {
      return entry->GetHostedDomain();
    }
  }

  const std::string domain =
      enterprise_util::GetDomainFromEmail(profile.GetProfileUserName());
  if (!signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          profile.GetProfileUserName())) {
    return std::nullopt;
  }
  return domain;
}

bool ShouldDisplayManagedUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't show the UI in demo mode.
  if (ash::DemoSession::IsDeviceInDemoMode())
    return false;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Don't show the UI for Family Link accounts.
  if (profile->IsChild())
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  return enterprise_util::IsBrowserManaged(profile) ||
         ShouldDisplayManagedByParentUi(profile);
}

#if !BUILDFLAG(IS_ANDROID)

GURL GetManagedUiUrl(Profile* profile) {
  if (enterprise_util::IsBrowserManaged(profile)) {
    return GURL(kChromeUIManagementURL);
  }

  if (ShouldDisplayManagedByParentUi(profile)) {
    return GURL(supervised_user::kManagedByParentUiMoreInfoUrl);
  }

  return GURL();
}

const gfx::VectorIcon& GetManagedUiIcon(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));

  if (enterprise_util::IsBrowserManaged(profile)) {
    return vector_icons::kBusinessChromeRefreshIcon;
  }

  CHECK(ShouldDisplayManagedByParentUi(profile));
  return vector_icons::kFamilyLinkIcon;
}

std::u16string GetManagedUiMenuItemLabel(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));
  if (!enterprise_util::IsBrowserManaged(profile)) {
    CHECK(ShouldDisplayManagedByParentUi(profile));
  }
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGED);
    case BROWSER_MANAGED_BY:
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_MANAGED_BY,
                                        base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringUTF16(IDS_BROWSER_PROFILE_MANAGED);
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_PROFILE_MANAGED_BY,
                                        base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
    case NOT_MANAGED:
      return std::u16string();
  }
  return std::u16string();
}

std::u16string GetManagedUiMenuItemTooltip(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  switch (GetManagementStringType(profile)) {
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY_TOOLTIP,
          base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY_TOOLTIP,
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED:
    case BROWSER_MANAGED_BY:
    case BROWSER_PROFILE_SAME_MANAGED_BY:
    case PROFILE_MANAGED_BY:
    case SUPERVISED:
    case NOT_MANAGED:
      return std::u16string();
  }
  return std::u16string();
}

std::string GetManagedUiWebUIIcon(Profile* profile) {
  if (enterprise_util::IsBrowserManaged(profile)) {
    return "cr:domain";
  }

  if (ShouldDisplayManagedByParentUi(profile)) {
    // The Family Link "kite" icon.
    return "cr20:kite";
  }

  // This method can be called even if we shouldn't display the managed UI.
  return std::string();
}

std::u16string GetManagedUiWebUILabel(Profile* profile) {
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();

  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringFUTF16(IDS_MANAGED_WITH_HYPERLINK,
                                        chrome::kChromeUIManagementURL16);
    case BROWSER_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_MANAGED_BY_WITH_HYPERLINK,
                                        chrome::kChromeUIManagementURL16,
                                        base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_SAME_MANAGED_BY_WITH_HYPERLINK,
          chrome::kChromeUIManagementURL16, base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY_WITH_HYPERLINK,
          chrome::kChromeUIManagementURL16, base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY_WITH_HYPERLINK,
          chrome::kChromeUIManagementURL16,
          base::UTF8ToUTF16(*account_manager));
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_PROFILE_MANAGED_BY_WITH_HYPERLINK,
                                        chrome::kChromeUIManagementURL16,
                                        base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGED_BY_PARENT_WITH_HYPERLINK,
          base::UTF8ToUTF16(supervised_user::kManagedByParentUiMoreInfoUrl));
    case NOT_MANAGED:
      return std::u16string();
  }
  return std::u16string();
}

std::u16string GetDeviceManagedUiHelpLabel(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ManagementUI::GetManagementPageSubtitle(profile);
#else
  if (enterprise_util::IsBrowserManaged(profile)) {
    std::optional<std::string> manager = GetAccountManagerIdentity(profile);
    if (!manager &&
        base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
      manager = GetDeviceManagerIdentity();
    }
    return manager && !manager->empty()
               ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                            base::UTF8ToUTF16(*manager))
               : l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE);
  }

  if (ShouldDisplayManagedByParentUi(profile)) {
    return l10n_util::GetStringUTF16(IDS_HELP_MANAGED_BY_YOUR_PARENT);
  }

  return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string GetDeviceManagedUiWebUILabel() {
  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;
  std::vector<std::u16string> replacements;
  replacements.push_back(chrome::kChromeUIManagementURL16);
  replacements.push_back(ui::GetChromeOSDeviceName());

  const std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  if (device_manager && !device_manager->empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(*device_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#else
std::u16string GetManagementPageSubtitle(Profile* profile) {
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();

  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE);
    case BROWSER_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                        base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_SAME_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY,
          base::UTF8ToUTF16(*account_manager));
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_PROFILE_MANAGED_BY,
          base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
    case NOT_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
  }
  return std::u16string();
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
std::u16string GetManagementBubbleTitle(Profile* profile) {
  // TODO(347245819): Use EnterpriseCustomLabel for the managers.
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();

  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED);
    case BROWSER_MANAGED_BY:
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringUTF16(
          IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY_MULTIPLE_ORGANIZATIONS);
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_DIALOG_PROFILE_MANAGED_BY,
          base::UTF8ToUTF16(*GetAccountManagerIdentity(profile)));
    case SUPERVISED:
    case NOT_MANAGED:
      NOTREACHED();
  }
}
#endif

bool AreProfileAndBrowserManagedBySameEntity(Profile* profile) {
  return GetManagementStringType(profile) == BROWSER_PROFILE_SAME_MANAGED_BY;
}

std::optional<std::string> GetDeviceManagerIdentity() {
  if (g_device_manager_for_testing) {
    return g_device_manager_for_testing;
  }

  if (!policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
    return std::nullopt;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetEnterpriseDomainManager();
#else
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(
          features::kEnterpriseManagementDisclaimerUsesCustomLabel)) {
    std::string custom_management_label =
        g_browser_process->local_state()
            ? g_browser_process->local_state()->GetString(
                  prefs::kEnterpriseCustomLabel)
            : std::string();
    if (!custom_management_label.empty()) {
      return custom_management_label;
    }
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // The device is managed as
  // `policy::ManagementServiceFactory::GetForPlatform()->IsManaged()` returned
  // true. `policy::GetManagedBy` might return `std::nullopt` if
  // `policy::CloudPolicyStore` hasn't fully initialized yet.
  return policy::GetManagedBy(g_browser_process->browser_policy_connector()
                                  ->machine_level_user_cloud_policy_manager())
      .value_or(std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
std::optional<std::string> GetSessionManagerIdentity() {
  if (!policy::PolicyLoaderLacros::IsMainUserManaged())
    return std::nullopt;
  return policy::PolicyLoaderLacros::main_user_policy_data()->managed_by();
}
#endif

std::optional<std::string> GetAccountManagerIdentity(Profile* profile) {
  if (!policy::ManagementServiceFactory::GetForProfile(profile)
           ->HasManagementAuthority(
               policy::EnterpriseManagementAuthority::CLOUD))
    return std::nullopt;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(
          features::kEnterpriseManagementDisclaimerUsesCustomLabel)) {
    std::string custom_management_label =
        profile->GetPrefs()->GetString(prefs::kEnterpriseCustomLabel);
    if (!custom_management_label.empty()) {
      return custom_management_label;
    }
  }
#endif

  const std::optional<std::string> managed_by =
      policy::GetManagedBy(profile->GetCloudPolicyManager());
  if (managed_by)
    return *managed_by;

  if (profile->GetProfilePolicyConnector()->IsUsingLocalTestPolicyProvider()) {
    return "Local Test Policies";
  }

  return GetEnterpriseAccountDomain(*profile);
}

}  // namespace chrome
