// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/uninstall_all_user_installed_web_apps_command.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_file_utils_wrapper.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/browsing_data_remover.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "components/nacl/browser/nacl_browser.h"
#endif  // BUILDFLAG(ENABLE_NACL)

namespace web_app {

namespace {

void WaitForPendingDataClearingTasks(Profile* profile) {
  content::BrowsingDataRemover* browsing_data_remover =
      profile->GetBrowsingDataRemover();
  if (browsing_data_remover->GetPendingTaskCountForTesting() == 0) {
    return;
  }

  base::test::TestFuture<void> future;
  browsing_data_remover->SetWouldCompleteCallbackForTesting(
      base::BindLambdaForTesting([&](base::OnceClosure callback) {
        if (browsing_data_remover->GetPendingTaskCountForTesting() == 1) {
          future.SetValue();
        }
        std::move(callback).Run();
      }));
  CHECK(future.Wait());
}

#if BUILDFLAG(ENABLE_NACL)
class ScopedNaClBrowserDelegate {
 public:
  explicit ScopedNaClBrowserDelegate(ProfileManager* profile_manager) {
    nacl::NaClBrowser::SetDelegate(
        std::make_unique<NaClBrowserDelegateImpl>(profile_manager));
  }

  ~ScopedNaClBrowserDelegate() { nacl::NaClBrowser::ClearAndDeleteDelegate(); }
};
#endif  // BUILDFLAG(ENABLE_NACL)

}  // namespace

class UninstallAllUserInstalledWebAppsCommandTest : public WebAppTest {
 public:
  UninstallAllUserInstalledWebAppsCommandTest() = default;

  void SetUp() override {
    WebAppTest::SetUp();

#if BUILDFLAG(ENABLE_NACL)
    // Uninstalling an IWA will clear PNACL cache, which needs this delegate
    // set.
    nacl_browser_delegate_ = std::make_unique<ScopedNaClBrowserDelegate>(
        profile_manager().profile_manager());
#endif  // BUILDFLAG(ENABLE_NACL)

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // IWAs will start a data clearing job when uninstalled, which needs to
    // complete before we delete the Profile.
    WaitForPendingDataClearingTasks(profile());
    provider()->Shutdown();
#if BUILDFLAG(ENABLE_NACL)
    nacl_browser_delegate_.reset();
#endif  // BUILDFLAG(ENABLE_NACL)
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  WebAppRegistrar& registrar_unsafe() { return provider()->registrar_unsafe(); }

 private:
#if BUILDFLAG(ENABLE_NACL)
  std::unique_ptr<ScopedNaClBrowserDelegate> nacl_browser_delegate_;
#endif  // BUILDFLAG(ENABLE_NACL)
};

TEST_F(UninstallAllUserInstalledWebAppsCommandTest, NoUserInstalledWebApps) {
  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
  {
    base::Value::Dict app_policy;
    app_policy.Set(web_app::kUrlKey, "https://example.com/install");
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kWebAppInstallForceList);
    update->Append(std::move(app_policy));
  }
  webapps::AppId app_id = observer.Wait();

  base::test::TestFuture<const std::optional<std::string>&> future;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<UninstallAllUserInstalledWebAppsCommand>(
          webapps::WebappUninstallSource::kHealthcareUserInstallCleanup,
          *profile(), future.GetCallback()));
  EXPECT_EQ(future.Get(), std::nullopt);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id));
}

TEST_F(UninstallAllUserInstalledWebAppsCommandTest, RemovesUserInstallSources) {
  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
  {
    base::Value::Dict app_policy;
    app_policy.Set(web_app::kUrlKey, "https://example.com/install");
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kWebAppInstallForceList);
    update->Append(std::move(app_policy));
  }
  webapps::AppId app_id = observer.Wait();

  webapps::AppId sync_app_id = test::InstallDummyWebApp(
      profile(), "app from sync", GURL("https://example.com/install"),
      webapps::WebappInstallSource::SYNC);
  EXPECT_EQ(app_id, sync_app_id);

  const WebApp* web_app = registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->GetSources().Has(WebAppManagement::kPolicy));
  EXPECT_TRUE(web_app->GetSources().Has(WebAppManagement::kSync));

  base::test::TestFuture<const std::optional<std::string>&> future;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<UninstallAllUserInstalledWebAppsCommand>(
          webapps::WebappUninstallSource::kHealthcareUserInstallCleanup,
          *profile(), future.GetCallback()));
  EXPECT_EQ(future.Get(), std::nullopt);

  EXPECT_TRUE(registrar_unsafe().IsInstalled(app_id));
  EXPECT_TRUE(web_app->GetSources().Has(WebAppManagement::kPolicy));
  EXPECT_FALSE(web_app->GetSources().Has(WebAppManagement::kSync));
}

TEST_F(UninstallAllUserInstalledWebAppsCommandTest,
       UninstallsUserInstalledWebApps) {
  webapps::AppId app_id1 = test::InstallDummyWebApp(
      profile(), "app from browser", GURL("https://example1.com"),
      webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB);

  webapps::AppId app_id2 = test::InstallDummyWebApp(
      profile(), "app from sync", GURL("https://example2.com"),
      webapps::WebappInstallSource::SYNC);

  webapps::AppId app_id3 = AddDummyIsolatedAppToRegistry(
      profile(),
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateRandomForProxyMode())
          .origin()
          .GetURL(),
      "iwa from installer",
      IsolationData::Builder(
          IwaStorageOwnedBundle{/*dir_name_ascii=*/"", /*dev_mode=*/false},
          base::Version("1"))
          .Build(),
      webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER);

  webapps::AppId app_id4 = AddDummyIsolatedAppToRegistry(
      profile(),
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateRandomForProxyMode())
          .origin()
          .GetURL(),
      "iwa from dev ui",
      IsolationData::Builder(
          IwaStorageOwnedBundle{/*dir_name_ascii=*/"", /*dev_mode=*/true},
          base::Version("1"))
          .Build(),
      webapps::WebappInstallSource::IWA_DEV_UI);

  webapps::AppId app_id5 = AddDummyIsolatedAppToRegistry(
      profile(),
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateRandomForProxyMode())
          .origin()
          .GetURL(),
      "iwa from dev command line",
      IsolationData::Builder(
          IwaStorageOwnedBundle{/*dir_name_ascii=*/"", /*dev_mode=*/true},
          base::Version("1"))
          .Build(),
      webapps::WebappInstallSource::IWA_DEV_COMMAND_LINE);

  base::test::TestFuture<const std::optional<std::string>&> future;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<UninstallAllUserInstalledWebAppsCommand>(
          webapps::WebappUninstallSource::kHealthcareUserInstallCleanup,
          *profile(), future.GetCallback()));
  EXPECT_EQ(future.Get(), std::nullopt);

  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id1));
  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id2));
  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id3));
  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id4));
  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id5));
}

class UninstallAllUserInstalledWebAppsCommandWithIconManagerTest
    : public UninstallAllUserInstalledWebAppsCommandTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    file_utils_wrapper_ =
        base::MakeRefCounted<testing::NiceMock<MockFileUtilsWrapper>>();
    fake_provider().SetFileUtils(file_utils_wrapper_);

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    file_utils_wrapper_ = nullptr;
    UninstallAllUserInstalledWebAppsCommandTest::TearDown();
  }

  scoped_refptr<testing::NiceMock<MockFileUtilsWrapper>> file_utils_wrapper_;
};

TEST_F(UninstallAllUserInstalledWebAppsCommandWithIconManagerTest,
       ReturnUninstallErrors) {
  EXPECT_CALL(*file_utils_wrapper_, WriteFile)
      .WillRepeatedly(testing::Return(true));

  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "app from sync", GURL("https://example.com"),
      webapps::WebappInstallSource::SYNC);

  EXPECT_CALL(*file_utils_wrapper_, DeleteFileRecursively)
      .WillOnce(testing::Return(false));

  base::test::TestFuture<const std::optional<std::string>&> future;
  provider()->command_manager().ScheduleCommand(
      std::make_unique<UninstallAllUserInstalledWebAppsCommand>(
          webapps::WebappUninstallSource::kHealthcareUserInstallCleanup,
          *profile(), future.GetCallback()));
  EXPECT_EQ(future.Get(), app_id + "[Sync]: kError");

  EXPECT_FALSE(registrar_unsafe().IsInstalled(app_id));
}

}  // namespace web_app
