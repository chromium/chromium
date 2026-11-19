// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/security_section.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/user_manager.h"
#else
#include "base/values.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "device/fido/public/features.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/webauthn_api.h"
#endif

namespace settings_search {

namespace {

using mojom::SettingId;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

class SecuritySectionTest : public testing::Test {
 protected:
  SecuritySectionTest() {
#if !BUILDFLAG(IS_CHROMEOS)
    stub_resolver_config_reader_.OverrideParentalControlsForTesting(
        /*parental_controls_override=*/false);
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        &stub_resolver_config_reader_);
#endif
#if BUILDFLAG(IS_WIN)
    // The Security keys subpage is hidden when the native Windows WebAuthn API
    // is available. Makes the API unavailable by default so that the subpage
    // items are registered regardless of the machine running the test. Tests
    // that cover the hidden case make the API available.
    fake_webauthn_api_.set_available(/*available=*/false);
#endif
  }

  ~SecuritySectionTest() override {
#if !BUILDFLAG(IS_CHROMEOS)
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        nullptr);
#endif
  }

  void RegisterItems() {
    section_.reset();
    registry_.emplace();
    section_ = std::make_unique<SecuritySection>(&*registry_, &profile_);
  }

  std::vector<SettingId> GetIds() const {
    std::vector<SettingId> ids;
    for (const raw_ptr<SettingsSearchItem>& item : registry_->GetItems()) {
      ids.push_back(item->id());
    }
    return ids;
  }

  const SettingsSearchItem& GetItem(SettingId id) const {
    for (const raw_ptr<SettingsSearchItem>& item : registry_->GetItems()) {
      if (item->id() == id) {
        return *item;
      }
    }
    NOTREACHED();
  }

  TestingProfile& profile() { return profile_; }

#if BUILDFLAG(IS_CHROMEOS)
  ash::FakeChromeUserManager* user_manager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
#else
  TestingPrefServiceSimple* local_state() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }

  void SetSecureDns(std::string_view mode) {
    local_state()->SetString(prefs::kDnsOverHttpsMode, mode);
  }
#endif

#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi& fake_webauthn_api() { return fake_webauthn_api_; }
#endif

 private:
  content::BrowserTaskEnvironment task_environment_;
#if !BUILDFLAG(IS_CHROMEOS)
  StubResolverConfigReader stub_resolver_config_reader_{
      TestingBrowserProcess::GetGlobal()->local_state()};
#endif
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain_{/*is_enrollee=*/false};
  device::FakeWinWebAuthnApi fake_webauthn_api_;
  device::WinWebAuthnApi::ScopedOverride webauthn_api_override_{
      &fake_webauthn_api_};
#endif
  TestingProfile profile_;
  std::optional<SettingsSearchRegistry> registry_{std::in_place};
  std::unique_ptr<SecuritySection> section_;
};

TEST_F(SecuritySectionTest, RegistersSecurityItems) {
#if !BUILDFLAG(IS_CHROMEOS)
  SetSecureDns(SecureDnsConfig::kModeOff);
#endif
  RegisterItems();

  // Keep in sync with SecuritySection::RegisterSearchItems(), in the order the
  // items are registered there.
  std::vector<SettingId> expected = {
      SettingId::kSecurity,
      SettingId::kSafeBrowsing,
      SettingId::kAlwaysUseSecureConnections,
      SettingId::kPasswordLeakDetection,
#if BUILDFLAG(IS_CHROMEOS)
      SettingId::kOpenChromeOsSecureDnsSettings,
#else
      SettingId::kSecureDns,
#endif
      SettingId::kJavascriptOptimization,
      SettingId::kSecurityKeys,
      SettingId::kSecurityKeysPin,
      SettingId::kSecurityKeysSignInData,
      SettingId::kSecurityKeysFingerprints,
      SettingId::kSecurityKeysReset,
      SettingId::kManageCertificates,
      SettingId::kAdvancedProtectionProgram,
  };
  EXPECT_THAT(GetIds(), UnorderedElementsAreArray(expected));
}

TEST_F(SecuritySectionTest, PageItemUsesSecurityRoute) {
  RegisterItems();

  const SettingsSearchItem& item = GetItem(SettingId::kSecurity);
  EXPECT_EQ("/security", item.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SECURITY), item.GetTitle());
  EXPECT_THAT(item.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY)));
  EXPECT_THAT(item.GetSynonyms(),
              ElementsAreArray(GetSynonyms(IDS_SETTINGS_TAG_SECURITY)));
}

// Items without a tag string have no synonyms.
TEST_F(SecuritySectionTest, ItemWithoutTagStringHasNoSynonyms) {
  RegisterItems();

  EXPECT_THAT(GetItem(SettingId::kAdvancedProtectionProgram).GetSynonyms(),
              IsEmpty());
}

TEST_F(SecuritySectionTest, SecurityItemHasSecurityBreadcrumbs) {
  RegisterItems();

  const SettingsSearchItem& item = GetItem(SettingId::kSafeBrowsing);
  EXPECT_EQ("/security", item.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SAFEBROWSING_SECTION_LABEL),
            item.GetTitle());
  EXPECT_THAT(item.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY),
                          l10n_util::GetStringUTF16(IDS_SETTINGS_SECURITY)));
}

// Guards against gating this on the new layout: the toggle moves between
// layouts with balanced mode, but is always shown.
TEST_F(SecuritySectionTest,
       AlwaysUseSecureConnectionsRegisteredWithoutBalancedMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHttpsFirstBalancedMode);
  RegisterItems();

  EXPECT_THAT(GetIds(), Contains(SettingId::kAlwaysUseSecureConnections));
}

TEST_F(SecuritySectionTest, SecurityKeysEntryOpensSubpage) {
  RegisterItems();

  const SettingsSearchItem& item = GetItem(SettingId::kSecurityKeys);
  EXPECT_EQ("/securityKeys", item.route_path());
  EXPECT_THAT(item.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY),
                          l10n_util::GetStringUTF16(IDS_SETTINGS_SECURITY)));
}

TEST_F(SecuritySectionTest, SecurityKeysItemHasSubpageBreadcrumbs) {
  RegisterItems();

  const SettingsSearchItem& item = GetItem(SettingId::kSecurityKeysReset);
  EXPECT_EQ("/securityKeys", item.route_path());
  EXPECT_THAT(
      item.breadcrumbs(),
      ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY),
                  l10n_util::GetStringUTF16(IDS_SETTINGS_SECURITY),
                  l10n_util::GetStringUTF16(IDS_SETTINGS_SECURITY_KEYS_TITLE)));
}

#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS guests get the Privacy and security pages, Security included.
TEST_F(SecuritySectionTest, GuestRegistersItems) {
  user_manager()->LoginUser(user_manager()->AddGuestUser()->GetAccountId());
  RegisterItems();

  EXPECT_THAT(GetIds(), Contains(SettingId::kSecurity));
  EXPECT_THAT(GetIds(), Contains(SettingId::kOpenChromeOsSecureDnsSettings));
}
#else
// Desktop guests don't get the Privacy and security pages at all.
TEST_F(SecuritySectionTest, GuestRegistersNothing) {
  profile().SetGuestSession(true);
  RegisterItems();

  EXPECT_THAT(GetIds(), IsEmpty());
}

// Guards against a stale provider item after the secure DNS configuration
// changes while Settings is open.
TEST_F(SecuritySectionTest, SecureDnsProviderItemFollowsConfiguration) {
  SetSecureDns(SecureDnsConfig::kModeOff);
  RegisterItems();
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kSecureDnsProvider)));

  SetSecureDns(SecureDnsConfig::kModeAutomatic);
  EXPECT_THAT(GetIds(), Contains(SettingId::kSecureDnsProvider));

  local_state()->SetManagedPref(prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kSecureDnsProvider)));
}

// Guards against skipping the provider item when secure DNS is already on as
// the section is created.
TEST_F(SecuritySectionTest, SecureDnsProviderItemRegisteredOnCreation) {
  SetSecureDns(SecureDnsConfig::kModeAutomatic);
  RegisterItems();

  EXPECT_THAT(GetIds(), Contains(SettingId::kSecureDnsProvider));
}
#endif

#if BUILDFLAG(IS_WIN)
// The native Windows WebAuthn UI replaces the Security keys subpage when that
// API is available.
TEST_F(SecuritySectionTest, NativeWinWebAuthnApiHidesSecurityKeys) {
  base::test::ScopedFeatureList feature_list{device::kWebAuthUseNativeWinApi};
  fake_webauthn_api().set_available(/*available=*/true);
  RegisterItems();

  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kSecurityKeys)));
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kSecurityKeysPin)));
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kSecurityKeysSignInData)));
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kSecurityKeysFingerprints)));
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kSecurityKeysReset)));
}
#endif

}  // namespace

}  // namespace settings_search
