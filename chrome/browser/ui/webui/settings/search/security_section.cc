// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/security_section.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"

namespace settings_search {

namespace {

using mojom::SettingId;

constexpr int kPrivacyBreadcrumbs[] = {IDS_SETTINGS_PRIVACY};
constexpr int kSecurityBreadcrumbs[] = {IDS_SETTINGS_PRIVACY,
                                        IDS_SETTINGS_SECURITY};
constexpr int kSecurityKeysBreadcrumbs[] = {IDS_SETTINGS_PRIVACY,
                                            IDS_SETTINGS_SECURITY,
                                            IDS_SETTINGS_SECURITY_KEYS_TITLE};

// Makes an item for a control on the Security page.
std::unique_ptr<SettingsSearchItem> MakeItem(
    SettingId id,
    int title_id,
    std::optional<int> synonyms_id = std::nullopt) {
  return MakeSearchItem(id, chrome::kSecuritySubPage, title_id,
                        kSecurityBreadcrumbs, synonyms_id);
}

// Makes an item for a control on the Security keys subpage.
std::unique_ptr<SettingsSearchItem> MakeSecurityKeysItem(SettingId id,
                                                         int title_id) {
  return MakeSearchItem(id, chrome::kSecurityKeysSubPage, title_id,
                        kSecurityKeysBreadcrumbs);
}

}  // namespace

SecuritySection::SecuritySection(SettingsSearchRegistry* registry,
                                 Profile* profile)
    : SettingsSection(registry) {
  CHECK(profile);
  RegisterSearchItems(profile);
}

SecuritySection::~SecuritySection() = default;

void SecuritySection::RegisterSearchItems(Profile* profile) {
  // Mirrors page_visibility.ts: guests don't get the Privacy and security
  // pages outside ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  if (settings_utils::IsGuest(profile)) {
    return;
  }
#endif
  // Represents the Security subpage as well as the "Security" link row on
  // Privacy and security.
  registry()->AddItem(MakeSearchItem(
      SettingId::kSecurity, chrome::kSecuritySubPage, IDS_SETTINGS_SECURITY,
      kPrivacyBreadcrumbs, IDS_SETTINGS_TAG_SECURITY));
  registry()->AddItem(MakeItem(SettingId::kSafeBrowsing,
                               IDS_SETTINGS_SAFEBROWSING_SECTION_LABEL,
                               IDS_SETTINGS_TAG_SAFE_BROWSING));
  // Shown in both layouts that loadTimeData
  // "enableHttpsFirstModeNewSettings" chooses between.
  registry()->AddItem(MakeItem(SettingId::kAlwaysUseSecureConnections,
                               IDS_SETTINGS_HTTPS_ONLY_MODE,
                               IDS_SETTINGS_TAG_HTTPS_ONLY_MODE));
  registry()->AddItem(MakeItem(SettingId::kPasswordLeakDetection,
                               IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_LABEL,
                               IDS_SETTINGS_TAG_PASSWORDS_LEAK_DETECTION));
#if BUILDFLAG(IS_CHROMEOS)
  // Mirrors loadTimeData "showSecureDnsSettingLink".
  registry()->AddItem(
      MakeItem(SettingId::kOpenChromeOsSecureDnsSettings,
               IDS_SETTINGS_SECURE_DNS_OPEN_CHROME_OS_SETTINGS_LABEL));
#else
  // Mirrors loadTimeData "showSecureDnsSetting".
  registry()->AddItem(MakeItem(SettingId::kSecureDns, IDS_SETTINGS_SECURE_DNS,
                               IDS_SETTINGS_TAG_SECURE_DNS));
#endif
  registry()->AddItem(
      MakeItem(SettingId::kJavascriptOptimization,
               IDS_SETTINGS_SECURITY_JAVASCRIPT_OPTIMIZATION_LINK_TITLE));
  // Mirrors loadTimeData "enableSecurityKeysSubpage" and
  // "enableSecurityKeysBioEnrollment".
  if (settings_utils::IsSecurityKeysSubpageEnabled()) {
    // The "Manage security keys" row opens the subpage itself.
    registry()->AddItem(
        MakeSearchItem(SettingId::kSecurityKeys, chrome::kSecurityKeysSubPage,
                       IDS_SETTINGS_SECURITY_KEYS_TITLE, kSecurityBreadcrumbs,
                       IDS_SETTINGS_TAG_SECURITY_KEYS));
    registry()->AddItem(MakeSecurityKeysItem(
        SettingId::kSecurityKeysPin, IDS_SETTINGS_SECURITY_KEYS_SET_PIN));
    registry()->AddItem(MakeSecurityKeysItem(
        SettingId::kSecurityKeysSignInData,
        IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_LABEL));
    registry()->AddItem(MakeSecurityKeysItem(
        SettingId::kSecurityKeysFingerprints,
        IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_SUBPAGE_LABEL));
    registry()->AddItem(MakeSecurityKeysItem(SettingId::kSecurityKeysReset,
                                             IDS_SETTINGS_SECURITY_KEYS_RESET));
  }
  registry()->AddItem(MakeItem(SettingId::kManageCertificates,
                               IDS_SETTINGS_MANAGE_CERTIFICATES,
                               IDS_SETTINGS_TAG_MANAGE_CERTIFICATES));
  registry()->AddItem(MakeItem(SettingId::kAdvancedProtectionProgram,
                               IDS_SETTINGS_ADVANCED_PROTECTION_PROGRAM));
}

}  // namespace settings_search
