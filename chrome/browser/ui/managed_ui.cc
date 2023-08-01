// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

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
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/webui/policy_data_utils.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
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

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#endif

namespace chrome {

namespace {

const policy::CloudPolicyManager* GetUserCloudPolicyManager(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return profile->GetUserCloudPolicyManagerAsh();
#else
  return profile->GetUserCloudPolicyManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

absl::optional<std::string> GetEnterpriseAccountDomain(Profile* profile) {
  if (g_browser_process->profile_manager()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());
    if (entry && !entry->GetHostedDomain().empty() &&
        entry->GetHostedDomain() != kNoHostedDomainFound)
      return entry->GetHostedDomain();
  }

  const std::string domain =
      enterprise_util::GetDomainFromEmail(profile->GetProfileUserName());
  // Heuristic for most common consumer Google domains -- these are not managed.
  if (domain.empty() || domain == "gmail.com" || domain == "googlemail.com")
    return absl::nullopt;
  return domain;
}

bool ShouldDisplayManagedByParentUi(Profile* profile) {
#if !BUILDFLAG(ENABLE_SUPERVISED_USERS) || BUILDFLAG(IS_CHROMEOS)
  // Don't display the managed by parent UI:
  // * on unsupervised platforms
  // * on ChromeOS, because similar UI is displayed at the OS level.
  return false;
#else

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // The EnableManagedByParentUiOnDesktop flag depends on
  // EnableSupervisionOnDesktopAndIOS.
  DCHECK(
      base::FeatureList::IsEnabled(
          supervised_user::kEnableSupervisionOnDesktopAndIOS) ||
      !base::FeatureList::IsEnabled(supervised_user::kEnableManagedByParentUi));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  const auto* const supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  return supervised_user_service &&
         supervised_user_service->IsSubjectToParentalControls() &&
         base::FeatureList::IsEnabled(
             supervised_user::kEnableManagedByParentUi);
#endif  // !BUILDFLAG(ENABLE_SUPERVISED_USERS) || BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

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

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (ShouldDisplayManagedByParentUi(profile)) {
    return GURL(supervised_user::kManagedByParentUiMoreInfoUrl.Get());
  }
#endif

  return GURL();
}

const gfx::VectorIcon& GetManagedUiIcon(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));

  if (enterprise_util::IsBrowserManaged(profile)) {
    return features::IsChromeRefresh2023()
               ? vector_icons::kBusinessChromeRefreshIcon
               : vector_icons::kBusinessIcon;
  }

  CHECK(ShouldDisplayManagedByParentUi(profile));
  return vector_icons::kFamilyLinkIcon;
}

std::u16string GetManagedUiMenuItemLabel(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));
  absl::optional<std::string> manager = GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = GetDeviceManagerIdentity();
  }

  if (enterprise_util::IsBrowserManaged(profile)) {
    int string_id = IDS_MANAGED;
    std::vector<std::u16string> replacements;

    if (manager && !manager->empty()) {
      string_id = IDS_MANAGED_BY;
      replacements.push_back(base::UTF8ToUTF16(*manager));
    }

    return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
  }

  CHECK(ShouldDisplayManagedByParentUi(profile));
  return l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
}

std::string GetManagedUiWebUIIcon(Profile* profile) {
  if (enterprise_util::IsBrowserManaged(profile)) {
    return "cr:domain";
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (ShouldDisplayManagedByParentUi(profile)) {
    // The Family Link "kite" icon.
    return "cr20:kite";
  }
#endif

  // This method can be called even if we shouldn't display the managed UI.
  return std::string();
}

std::u16string GetManagedUiWebUILabel(Profile* profile) {
  absl::optional<std::string> manager = GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = GetDeviceManagerIdentity();
  }

  if (enterprise_util::IsBrowserManaged(profile)) {
    int string_id = IDS_MANAGED_WITH_HYPERLINK;
    std::vector<std::u16string> replacements = {
        base::UTF8ToUTF16(chrome::kChromeUIManagementURL)};
    if (manager && !manager->empty()) {
      string_id = IDS_MANAGED_BY_WITH_HYPERLINK;
      replacements.push_back(base::UTF8ToUTF16(*manager));
    }

    return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (ShouldDisplayManagedByParentUi(profile)) {
    std::vector<std::u16string> replacements = {base::UTF8ToUTF16(
        supervised_user::kManagedByParentUiMoreInfoUrl.Get())};
    return l10n_util::GetStringFUTF16(IDS_MANAGED_BY_PARENT_WITH_HYPERLINK,
                                      replacements, nullptr);
  }
#endif

  // This method can be called even if we shouldn't display the managed UI.
  return std::u16string();
}

std::u16string GetDeviceManagedUiHelpLabel(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ManagementUI::GetManagementPageSubtitle(profile);
#else
  if (enterprise_util::IsBrowserManaged(profile)) {
    absl::optional<std::string> manager = GetAccountManagerIdentity(profile);
    if (!manager &&
        base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
      manager = GetDeviceManagerIdentity();
    }
    return manager && !manager->empty()
               ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                            base::UTF8ToUTF16(*manager))
               : l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE);
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (ShouldDisplayManagedByParentUi(profile)) {
    return l10n_util::GetStringUTF16(IDS_HELP_MANAGED_BY_YOUR_PARENT);
  }
#endif

  return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string GetDeviceManagedUiWebUILabel() {
  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;
  std::vector<std::u16string> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  replacements.push_back(ui::GetChromeOSDeviceName());

  const absl::optional<std::string> device_manager = GetDeviceManagerIdentity();
  if (device_manager && !device_manager->empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(*device_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#endif

absl::optional<std::string> GetDeviceManagerIdentity() {
  if (!policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
    return absl::nullopt;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetEnterpriseDomainManager();
#else
  // The device is managed as
  // `policy::ManagementServiceFactory::GetForPlatform()->IsManaged()` returned
  // true. `policy::GetManagedBy` might return `absl::nullopt` if
  // `policy::CloudPolicyStore` hasn't fully initialized yet.
  return policy::GetManagedBy(g_browser_process->browser_policy_connector()
                                  ->machine_level_user_cloud_policy_manager())
      .value_or(std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
absl::optional<std::string> GetSessionManagerIdentity() {
  if (!policy::PolicyLoaderLacros::IsMainUserManaged())
    return absl::nullopt;
  return policy::PolicyLoaderLacros::main_user_policy_data()->managed_by();
}
#endif

absl::optional<std::string> GetAccountManagerIdentity(Profile* profile) {
  if (!policy::ManagementServiceFactory::GetForProfile(profile)
           ->HasManagementAuthority(
               policy::EnterpriseManagementAuthority::CLOUD))
    return absl::nullopt;

  const absl::optional<std::string> managed_by =
      policy::GetManagedBy(GetUserCloudPolicyManager(profile));
  if (managed_by)
    return *managed_by;

  return GetEnterpriseAccountDomain(profile);
}

}  // namespace chrome
