// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

using WebAppUtilsTest = WebAppTest;
using ::testing::ElementsAre;
class FakeWebAppProvider;

// Sanity check that iteration order of SortedSizesPx is ascending. The
// correctness of most usage of SortedSizesPx depends on this.
TEST(WebAppTest, SortedSizesPxIsAscending) {
  // Removal of duplicates is expected but not required for correctness.
  std::vector<SquareSizePx> in{512, 512, 16, 512, 64, 32, 256};
  SortedSizesPx sorted(in);
  ASSERT_THAT(sorted, ElementsAre(16, 32, 64, 256, 512));

  std::vector<SquareSizePx> out(sorted.begin(), sorted.end());
  ASSERT_THAT(out, ElementsAre(16, 32, 64, 256, 512));

  std::vector<SquareSizePx> reversed(sorted.rbegin(), sorted.rend());
  ASSERT_THAT(reversed, ElementsAre(512, 256, 64, 32, 16));

  std::vector<SquareSizePx> base_reversed(base::Reversed(sorted).begin(),
                                          base::Reversed(sorted).end());
  ASSERT_THAT(base_reversed, ElementsAre(512, 256, 64, 32, 16));
}

TEST_F(WebAppUtilsTest, AreWebAppsEnabled) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_FALSE(AreWebAppsEnabled(regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(AreWebAppsEnabled(guest_profile));
  EXPECT_FALSE(AreWebAppsEnabled(guest_profile->GetOriginalProfile()));
  EXPECT_TRUE(AreWebAppsEnabled(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#else
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile));
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile->GetOriginalProfile()));
  EXPECT_FALSE(AreWebAppsEnabled(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  Profile* signin_profile =
      profile_manager().CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsEnabled(signin_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      signin_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  const AccountId account_id = AccountId::FromUserEmail("test@test");
  {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  }
  {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* user = user_manager->AddKioskAppUser(account_id);
    user_manager->UserLoggedIn(
        user->GetAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_FALSE(AreWebAppsEnabled(regular_profile));
  }
  {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* user = user_manager->AddWebKioskAppUser(account_id);
    user_manager->UserLoggedIn(
        user->GetAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  }
#endif
}

TEST_F(WebAppUtilsTest, AreWebAppsUserInstallable) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsUserInstallable(regular_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(guest_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

#if !BUILDFLAG(IS_CHROMEOS)
  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(system_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif

#if BUILDFLAG(IS_CHROMEOS)
  Profile* signin_profile =
      profile_manager().CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsUserInstallable(signin_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      signin_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebApps) {
  Profile* regular_profile = profile();

  Profile* expected_otr_browser_context = nullptr;
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(https://crbug.com/384063076): Stop returning for profiles on ChromeOS
  // where `AreWebAppsEnabled` returns `false`.
  expected_otr_browser_context = regular_profile;
#endif

  EXPECT_EQ(regular_profile, GetBrowserContextForWebApps(regular_profile));
  EXPECT_EQ(expected_otr_browser_context,
            GetBrowserContextForWebApps(regular_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
  EXPECT_EQ(expected_otr_browser_context,
            GetBrowserContextForWebApps(regular_profile->GetOffTheRecordProfile(
                Profile::OTRProfileID::CreateUniqueForTesting(),
                /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  Profile* guest_otr_profile = guest_profile->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(guest_profile));
  EXPECT_EQ(guest_otr_profile, GetBrowserContextForWebApps(guest_otr_profile));
#else
  EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(guest_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(guest_otr_profile));

  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(system_profile));
  EXPECT_EQ(nullptr,
            GetBrowserContextForWebApps(system_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebAppMetrics) {
  Profile* regular_profile = profile();

  Profile* expected_otr_browser_context = nullptr;
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(https://crbug.com/384063076): Stop returning for profiles on ChromeOS
  // where `AreWebAppsEnabled` returns `false`.
  expected_otr_browser_context = regular_profile;
#endif

  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebAppMetrics(regular_profile));
  EXPECT_EQ(
      expected_otr_browser_context,
      GetBrowserContextForWebAppMetrics(
          regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_EQ(
      expected_otr_browser_context,
      GetBrowserContextForWebAppMetrics(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  Profile* guest_otr_profile = guest_profile->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_otr_profile));

#if !BUILDFLAG(IS_CHROMEOS)
  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(system_profile));
  EXPECT_EQ(
      nullptr,
      GetBrowserContextForWebAppMetrics(
          system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
// TODO(http://b/331208955): Remove after migration.
TEST_F(WebAppUtilsTest, CanUserUninstallGeminiApp) {
  EXPECT_FALSE(CanUserUninstallWebApp(
      ash::kGeminiAppId, WebAppManagementTypes({WebAppManagement::kDefault})));
  EXPECT_TRUE(CanUserUninstallWebApp(
      ash::kGeminiAppId, WebAppManagementTypes({WebAppManagement::kSync})));
}

// TODO(http://b/331208955): Remove after migration.
TEST_F(WebAppUtilsTest, GeminiAppWillBeSystemWebApp) {
  for (auto src : WebAppManagementTypes::All()) {
    EXPECT_THAT(
        WillBeSystemWebApp(ash::kGeminiAppId, WebAppManagementTypes({src})),
        src == WebAppManagement::kDefault);
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)

class WebAppUtilsPolicyIdsTest : public WebAppTest {
 public:
  using InstallResults = std::map<GURL /*install_url*/,
                                  ExternallyManagedAppManager::InstallResult>;
  using UninstallResults =
      std::map<GURL /*install_url*/, webapps::UninstallResultCode>;
  using SynchronizeFuture =
      base::test::TestFuture<InstallResults, UninstallResults>;

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());
    provider_->UseRealOsIntegrationManager();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(WebAppUtilsPolicyIdsTest, GetWebAppPolicyIdsForWebApp) {
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager())
          .CreateBasicInstallPageState(kInstallUrl, kManifestUrl, kWebAppUrl);

  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);

  SynchronizeFuture result;
  std::vector<ExternalInstallOptions> install_options_list;
  install_options_list.emplace_back(kInstallUrl,
                                    /*user_display_mode=*/std::nullopt,
                                    ExternalInstallSource::kExternalPolicy);

  fake_provider().externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
      result.GetCallback());
  ASSERT_TRUE(result.Wait());
  const WebApp* app = fake_provider().registrar_unsafe().GetAppById(app_id);

  EXPECT_EQ(
      GetPolicyIds(profile(), *app),
      std::vector<std::string>({"https://www.example.com/install_url.html"}));
}

TEST_F(WebAppUtilsPolicyIdsTest, GetWebAppPolicyIdsForIsolatedWebApp) {
  auto bundle = IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
                    .BuildBundle();
  IsolatedWebAppUrlInfo info =
      bundle
          ->InstallWithSource(profile(),
                              &IsolatedWebAppInstallSource::FromExternalPolicy)
          .value();
  const WebApp* app =
      fake_provider().registrar_unsafe().GetAppById(info.app_id());

  EXPECT_EQ(GetPolicyIds(profile(), *app),
            std::vector<std::string>({info.web_bundle_id().id()}));
}

}  // namespace web_app
