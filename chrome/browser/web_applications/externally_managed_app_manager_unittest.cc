// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <sstream>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

int GetInstallCountsFromResult(
    const std::map<GURL, ExternallyManagedAppManager::InstallResult>&
        install_results,
    bool include_duplicates = false) {
  int count = 0;
  for (const auto& result : install_results) {
    if (result.second.code == webapps::InstallResultCode::kSuccessNewInstall ||
        (include_duplicates &&
         result.second.code ==
             webapps::InstallResultCode::kSuccessAlreadyInstalled)) {
      count++;
    }
  }
  return count;
}

}  // namespace

class ExternallyManagedAppManagerTest : public WebAppTest {
 public:
  ExternallyManagedAppManagerTest() = default;

 protected:
  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void ForceSystemShutdown() { provider_->Shutdown(); }

  void Sync(const std::vector<GURL>& urls, bool include_duplicates = false) {
    ResetCounts();

    std::vector<ExternalInstallOptions> install_options_list;
    install_options_list.reserve(urls.size());
    for (const auto& url : urls) {
      install_options_list.emplace_back(
          url, mojom::UserDisplayMode::kStandalone,
          ExternalInstallSource::kInternalDefault);
    }

    base::RunLoop run_loop;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        std::move(install_options_list),
        ExternalInstallSource::kInternalDefault,
        base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, webapps::UninstallResultCode>
                    uninstall_results) {
              install_count_ = GetInstallCountsFromResult(install_results,
                                                          include_duplicates);
              uninstall_count_ = uninstall_results.size();
              run_loop.Quit();
            }));
    // Wait for SynchronizeInstalledApps to finish.
    run_loop.Run();
  }

  void Expect(int install_count,
              int uninstall_count,
              const std::vector<GURL>& installed_app_urls) {
    EXPECT_EQ(install_count, install_count_);
    EXPECT_EQ(uninstall_count, uninstall_count_);
    base::flat_map<webapps::AppId, base::flat_set<GURL>> apps =
        app_registrar().GetExternallyInstalledApps(
            ExternalInstallSource::kInternalDefault);
    std::vector<GURL> urls;
    for (const auto& it : apps) {
      std::ranges::copy(it.second, std::back_inserter(urls));
    }

    std::sort(urls.begin(), urls.end());
    EXPECT_EQ(installed_app_urls, urls);
  }

  void ResetCounts() {
    install_count_ = 0;
    uninstall_count_ = 0;
  }

  WebAppProvider& provider() { return *provider_; }

  WebAppRegistrar& app_registrar() { return provider().registrar_unsafe(); }

 private:
  int install_count_ = 0;
  int uninstall_count_ = 0;

  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
};

// Test that destroying ExternallyManagedAppManager during a synchronize call
// that installs an app doesn't crash. Regression test for
// https://crbug.com/962808
TEST_F(ExternallyManagedAppManagerTest, DestroyDuringInstallInSynchronize) {
  std::vector<ExternalInstallOptions> install_options_list;
  install_options_list.emplace_back(GURL("https://foo.example"),
                                    mojom::UserDisplayMode::kStandalone,
                                    ExternalInstallSource::kInternalDefault);
  install_options_list.emplace_back(GURL("https://bar.example"),
                                    mojom::UserDisplayMode::kStandalone,
                                    ExternalInstallSource::kInternalDefault);

  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kInternalDefault,
      // ExternallyManagedAppManager gives no guarantees about whether its
      // pending callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  ForceSystemShutdown();
  base::RunLoop().RunUntilIdle();
}

// Test that destroying ExternallyManagedAppManager during a synchronize call
// that uninstalls an app doesn't crash. Regression test for
// https://crbug.com/962808
TEST_F(ExternallyManagedAppManagerTest, DestroyDuringUninstallInSynchronize) {
  // Install an app that will be uninstalled next.
  {
    std::vector<ExternalInstallOptions> install_options_list;
    install_options_list.emplace_back(GURL("https://foo.example"),
                                      mojom::UserDisplayMode::kStandalone,
                                      ExternalInstallSource::kInternalDefault);
    base::RunLoop run_loop;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        std::move(install_options_list),
        ExternalInstallSource::kInternalDefault,
        base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, webapps::UninstallResultCode>
                    uninstall_results) { run_loop.Quit(); }));
    run_loop.Run();
  }

  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      std::vector<ExternalInstallOptions>(),
      ExternalInstallSource::kInternalDefault,
      // ExternallyManagedAppManager gives no guarantees about whether its
      // pending callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  ForceSystemShutdown();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExternallyManagedAppManagerTest, SynchronizeInstalledApps) {
  GURL a("https://a.example.com/");
  GURL b("https://b.example.com/");
  GURL c("https://c.example.com/");
  GURL d("https://d.example.com/");
  GURL e("https://e.example.com/");

  FakeWebContentsManager& web_contents_manager =
      static_cast<FakeWebContentsManager&>(provider().web_contents_manager());
  web_contents_manager.CreateBasicInstallPageState(a, a, a);
  web_contents_manager.CreateBasicInstallPageState(b, b, b);
  web_contents_manager.CreateBasicInstallPageState(c, c, c);
  web_contents_manager.CreateBasicInstallPageState(d, d, d);
  web_contents_manager.CreateBasicInstallPageState(e, e, e);

  Sync(std::vector<GURL>{a, b, d});
  Expect(3, 0, std::vector<GURL>{a, b, d});

  Sync(std::vector<GURL>{b, e});
  Expect(1, 2, std::vector<GURL>{b, e});

  Sync(std::vector<GURL>{e});
  Expect(0, 1, std::vector<GURL>{e});

  Sync(std::vector<GURL>{c});
  Expect(1, 1, std::vector<GURL>{c});

  Sync(std::vector<GURL>{e, a, d});
  Expect(3, 1, std::vector<GURL>{a, d, e});

  Sync(std::vector<GURL>{c, a, b, d, e});
  Expect(2, 0, std::vector<GURL>{a, b, c, d, e});

  Sync(std::vector<GURL>{});
  Expect(0, 5, std::vector<GURL>{});

  // The remaining code tests duplicate inputs.
  Sync(std::vector<GURL>{b, a, b, c}, /*include_duplicates=*/true);
  Expect(3, 0, std::vector<GURL>{a, b, c});

  Sync(std::vector<GURL>{e, a, e, e, e, a}, /*include_duplicates=*/true);
  Expect(2, 2, std::vector<GURL>{a, e});

  Sync(std::vector<GURL>{b, c, d}, /*include_duplicates=*/true);
  Expect(3, 2, std::vector<GURL>{b, c, d});

  Sync(std::vector<GURL>{a, a, a, a, a, a}, /*include_duplicates=*/true);
  Expect(1, 3, std::vector<GURL>{a});

  Sync(std::vector<GURL>{}, /*include_duplicates=*/true);
  Expect(0, 1, std::vector<GURL>{});
}

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

std::unique_ptr<WebAppInstallInfo> GetWebAppInstallInfo(const GURL& url) {
  std::unique_ptr<WebAppInstallInfo> info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(url);
  info->scope = url.GetWithoutFilename();
  info->title = u"Web App";
  return info;
}

// Test harness that keep the system as real as possible.
class ExternallyAppManagerTest : public WebAppTest {
 public:
  using InstallResults = std::map<GURL /*install_url*/,
                                  ExternallyManagedAppManager::InstallResult>;
  using UninstallResults =
      std::map<GURL /*install_url*/, webapps::UninstallResultCode>;
  using SynchronizeFuture =
      base::test::TestFuture<InstallResults, UninstallResults>;
  using InstallNowFuture =
      base::test::TestFuture<const GURL&,
                             ExternallyManagedAppManager::InstallResult>;

  ExternallyAppManagerTest() = default;

  void SetUp() override {
    WebAppTest::SetUp();
    // TODO(http://b/278922549): Disable the external management apps so we
    // don't compete with the policy app manager for our installs /
    // synchronization.
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::vector<ExternalInstallOptions> CreateExternalInstallOptionsFromTemplate(
      std::vector<GURL> install_urls,
      ExternalInstallSource source,
      std::optional<ExternalInstallOptions> template_options = std::nullopt) {
    std::vector<ExternalInstallOptions> output;
    std::ranges::transform(
        install_urls, std::back_inserter(output),
        [source, &template_options](const GURL& install_url) {
          ExternalInstallOptions options = template_options.value_or(
              ExternalInstallOptions(install_url, std::nullopt, source));
          options.install_url = install_url;
          options.install_source = source;
          return options;
        });
    return output;
  }

  ExternalInstallOptions CreateExternalInstallOptionsWithAppInfo(
      GURL install_url,
      ExternalInstallSource source) {
    ExternalInstallOptions options(install_url,
                                   mojom::UserDisplayMode::kBrowser, source);
    options.app_info_factory =
        base::BindRepeating(&GetWebAppInstallInfo, install_url);
    return options;
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  WebAppRegistrar& app_registrar() { return provider().registrar_unsafe(); }

  ExternallyManagedAppManager& external_manager() {
    return provider().externally_managed_app_manager();
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }
};

TEST_F(ExternallyAppManagerTest, NoNetworkNoPlaceholder) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");

  // Not populating the `FakeWebContentsManager` means it treats the network as
  // non-functional / not available.

  SynchronizeFuture result;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Empty uninstall results.
  EXPECT_THAT(result.Get<UninstallResults>(), IsEmpty());

  // Install should have failed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(install_results,
              ElementsAre(std::make_pair(
                  kInstallUrl,
                  ExternallyManagedAppManager::InstallResult(
                      webapps::InstallResultCode::kInstallURLLoadFailed))));
}

TEST_F(ExternallyAppManagerTest, SimpleInstall) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);

  SynchronizeFuture result;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Empty uninstall results.
  EXPECT_THAT(result.Get<UninstallResults>(), IsEmpty());

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      ElementsAre(std::make_pair(
          kInstallUrl,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id))));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_SimpleInstallWebAppInfo DISABLED_SimpleInstallWebAppInfo
#else
#define MAYBE_SimpleInstallWebAppInfo SimpleInstallWebAppInfo
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ExternallyAppManagerTest, MAYBE_SimpleInstallWebAppInfo) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);

  InstallNowFuture future;
  external_manager().InstallNow(
      CreateExternalInstallOptionsWithAppInfo(
          kInstallUrl, ExternalInstallSource::kExternalPolicy),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Install should succeed.
  ExternallyManagedAppManager::InstallResult result =
      future.Get<ExternallyManagedAppManager::InstallResult>();
  EXPECT_EQ(result,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_MultipleInstallWebAppInfoInstallUrl \
  DISABLED_MultipleInstallWebAppInfoInstallUrl
#else
#define MAYBE_MultipleInstallWebAppInfoInstallUrl \
  MultipleInstallWebAppInfoInstallUrl
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ExternallyAppManagerTest, MAYBE_MultipleInstallWebAppInfoInstallUrl) {
  const GURL kStartUrl1 = GURL("https://www.example1.com/index.html");
  const GURL kInstallUrl1 =
      GURL("https://www.example1.com/nested/install_url.html");
  const GURL kManifestUrl1 = GURL("https://www.example1.com/manifest.json");

  const GURL kStartUrl2 = GURL("https://www.example2.com/index.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example2.com/nested/install_url.html");
  const GURL kManifestUrl2 = GURL("https://www.example2.com/manifest.json");

  webapps::AppId app_id1 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl1, kManifestUrl1, kStartUrl1);

  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl2, kManifestUrl2, kStartUrl2);

  InstallNowFuture future1;
  InstallNowFuture future2;
  external_manager().InstallNow(
      CreateExternalInstallOptionsWithAppInfo(
          kInstallUrl1, ExternalInstallSource::kExternalDefault),
      future1.GetCallback());
  external_manager().InstallNow(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl2}, ExternalInstallSource::kExternalPolicy)[0],
      future2.GetCallback());
  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future2.Wait());

  // Both installs should succeed.
  ExternallyManagedAppManager::InstallResult result1 =
      future1.Get<ExternallyManagedAppManager::InstallResult>();
  ExternallyManagedAppManager::InstallResult result2 =
      future2.Get<ExternallyManagedAppManager::InstallResult>();

  EXPECT_EQ(result1,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id1));
  EXPECT_EQ(result2,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id2));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_InstallWebAppInfoTwiceAlreadyInstalled \
  DISABLED_InstallWebAppInfoTwiceAlreadyInstalled
#else
#define MAYBE_InstallWebAppInfoTwiceAlreadyInstalled \
  InstallWebAppInfoTwiceAlreadyInstalled
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ExternallyAppManagerTest, MAYBE_InstallWebAppInfoTwiceAlreadyInstalled) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);

  InstallNowFuture future1;
  InstallNowFuture future2;
  external_manager().Install(
      CreateExternalInstallOptionsWithAppInfo(
          kInstallUrl, ExternalInstallSource::kExternalDefault),
      future1.GetCallback());
  external_manager().Install(
      CreateExternalInstallOptionsWithAppInfo(
          kInstallUrl, ExternalInstallSource::kExternalDefault),
      future2.GetCallback());
  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future2.Wait());

  // Both installs should succeed, with the 2nd one returning a
  // `kSuccessAlreadyInstalled` result.
  ExternallyManagedAppManager::InstallResult result1 =
      future1.Get<ExternallyManagedAppManager::InstallResult>();
  ExternallyManagedAppManager::InstallResult result2 =
      future2.Get<ExternallyManagedAppManager::InstallResult>();

  EXPECT_EQ(result1,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id));
  EXPECT_EQ(result2,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessAlreadyInstalled, app_id));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_InstallOptionsTwiceForReinstall \
  DISABLED_InstallOptionsTwiceForReinstall
#else
#define MAYBE_InstallOptionsTwiceForReinstall InstallOptionsTwiceForReinstall
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ExternallyAppManagerTest, MAYBE_InstallOptionsTwiceForReinstall) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);

  ExternalInstallOptions options = CreateExternalInstallOptionsFromTemplate(
      {kInstallUrl}, ExternalInstallSource::kExternalPolicy)[0];
  options.force_reinstall = true;

  InstallNowFuture future1;
  InstallNowFuture future2;
  external_manager().Install(options, future1.GetCallback());
  external_manager().Install(options, future2.GetCallback());
  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future2.Wait());

  // Both installs should succeed, with the 2nd one returning a
  // `kSuccessNewInstall` result because of the `force_reinstall` flag.
  ExternallyManagedAppManager::InstallResult result1 =
      future1.Get<ExternallyManagedAppManager::InstallResult>();
  ExternallyManagedAppManager::InstallResult result2 =
      future2.Get<ExternallyManagedAppManager::InstallResult>();

  EXPECT_EQ(result1,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id));
  EXPECT_EQ(result2,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_InstallReentrantCallback DISABLED_InstallReentrantCallback
#else
#define MAYBE_InstallReentrantCallback InstallReentrantCallback
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ExternallyAppManagerTest, MAYBE_InstallReentrantCallback) {
  const GURL kStartUrl1 = GURL("https://www.example1.com/index.html");
  const GURL kInstallUrl1 =
      GURL("https://www.example1.com/nested/install_url.html");
  const GURL kManifestUrl1 = GURL("https://www.example1.com/manifest.json");

  const GURL kStartUrl2 = GURL("https://www.example2.com/index.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example2.com/nested/install_url.html");
  const GURL kManifestUrl2 = GURL("https://www.example2.com/manifest.json");

  webapps::AppId app_id1 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl1, kManifestUrl1, kStartUrl1);

  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl2, kManifestUrl2, kStartUrl2);

  InstallNowFuture inside_future;
  ExternallyManagedAppManager::InstallResult external_result;
  external_manager().InstallNow(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl1}, ExternalInstallSource::kExternalPolicy)[0],
      base::BindLambdaForTesting(
          [&](const GURL& url,
              ExternallyManagedAppManager::InstallResult result) {
            // Verify the first installation has succeeded.
            EXPECT_EQ(
                result,
                ExternallyManagedAppManager::InstallResult(
                    webapps::InstallResultCode::kSuccessNewInstall, app_id1));
            external_manager().InstallNow(
                CreateExternalInstallOptionsFromTemplate(
                    {kInstallUrl2}, ExternalInstallSource::kExternalPolicy)[0],
                inside_future.GetCallback());
          }));
  ASSERT_TRUE(inside_future.Wait());

  // The intermediary install inside the callback should succeed.
  ExternallyManagedAppManager::InstallResult result =
      inside_future.Get<ExternallyManagedAppManager::InstallResult>();
  EXPECT_EQ(result,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id2));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_MultipleAppsInstallSerial DISABLED_MultipleAppsInstallSerial
#else
#define MAYBE_MultipleAppsInstallSerial MultipleAppsInstallSerial
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ExternallyAppManagerTest, MAYBE_MultipleAppsInstallSerial) {
  const GURL kStartUrl1 = GURL("https://www.example.com/index1.html");
  const GURL kInstallUrl1 =
      GURL("https://www.example.com/nested/install_url1.html");
  const GURL kManifestUrl1 = GURL("https://www.example.com/manifest1.json");

  webapps::AppId app_id1 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl1, kManifestUrl1, kStartUrl1);

  SynchronizeFuture result1;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl1}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result1.GetCallback());
  ASSERT_TRUE(result1.Wait());

  // Empty uninstall results.
  EXPECT_THAT(result1.Get<UninstallResults>(), IsEmpty());

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results1 =
      result1.Get<InstallResults>();
  EXPECT_THAT(
      install_results1,
      ElementsAre(std::make_pair(
          kInstallUrl1,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id1))));

  const GURL kStartUrl2 = GURL("https://www.example2.com/index.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example2.com/nested/install_url.html");
  const GURL kManifestUrl2 = GURL("https://www.example2.com/manifest.json");

  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl2, kManifestUrl2, kStartUrl2);

  SynchronizeFuture result2;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl2}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result2.GetCallback());
  ASSERT_TRUE(result2.Wait());

  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results2 =
      result2.Get<InstallResults>();
  EXPECT_THAT(
      install_results2,
      ElementsAre(std::make_pair(
          kInstallUrl2,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id2))));
}

// TODO(crbug.com/405912587): Investigate and enable on Linux TSAN bots.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_PlaceholderAppWindowsClosed DISABLED_PlaceholderAppWindowsClosed
#else
#define MAYBE_PlaceholderAppWindowsClosed PlaceholderAppWindowsClosed
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ExternallyAppManagerTest, MAYBE_PlaceholderAppWindowsClosed) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  SynchronizeFuture result;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
          template_options),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // The webapps::AppId should be created from the install url.
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, kInstallUrl);

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      ElementsAre(std::make_pair(
          kInstallUrl,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id))));

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);

  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                         WebApp::ExternalManagementConfig(
                                             /*is_placeholder=*/true,
                                             /*install_urls=*/{kInstallUrl},
                                             /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, TwoInstallUrlsSameApp) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl1 =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example.com/nested/install_url2.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl1, kManifestUrl, kStartUrl);
  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl2, kManifestUrl, kStartUrl);
  EXPECT_EQ(app_id, app_id2);

  SynchronizeFuture result;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl1, kInstallUrl2}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Empty uninstall results.
  EXPECT_THAT(result.Get<UninstallResults>(), IsEmpty());

  // Installs should have both succeeded.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      UnorderedElementsAre(
          std::make_pair(
              kInstallUrl1,
              ExternallyManagedAppManager::InstallResult(
                  webapps::InstallResultCode::kSuccessNewInstall, app_id)),
          std::make_pair(
              kInstallUrl2,
              ExternallyManagedAppManager::InstallResult(
                  webapps::InstallResultCode::kSuccessNewInstall, app_id))));

  EXPECT_EQ(app_registrar().GetAppIds().size(), 1ul);
  const WebApp* app = app_registrar().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(
                  WebAppManagement::kPolicy,
                  WebApp::ExternalManagementConfig(
                      /*is_placeholder=*/false,
                      /*install_urls=*/{kInstallUrl1, kInstallUrl2},
                      /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, RemovingInstallUrlsFromSource) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl1 =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example.com/nested/install_url2.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl1, kManifestUrl, kStartUrl);
  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl2, kManifestUrl, kStartUrl);
  EXPECT_EQ(app_id, app_id2);

  // Synchronize with 2 install URLs.
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl1, kInstallUrl2},
            ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());

    // Empty uninstall results.
    EXPECT_THAT(result.Get<UninstallResults>(), IsEmpty());

    // Installs should have both succeeded.
    EXPECT_THAT(
        result.Get<InstallResults>(),
        UnorderedElementsAre(
            std::make_pair(
                kInstallUrl1,
                ExternallyManagedAppManager::InstallResult(
                    webapps::InstallResultCode::kSuccessNewInstall, app_id)),
            std::make_pair(
                kInstallUrl2,
                ExternallyManagedAppManager::InstallResult(
                    webapps::InstallResultCode::kSuccessNewInstall, app_id))));

    EXPECT_EQ(app_registrar().GetAppIds().size(), 1ul);
    const WebApp* app = app_registrar().GetAppById(app_id);
    ASSERT_TRUE(app);
    EXPECT_THAT(app->management_to_external_config_map(),
                ElementsAre(std::make_pair(
                    WebAppManagement::kPolicy,
                    WebApp::ExternalManagementConfig(
                        /*is_placeholder=*/false,
                        /*install_urls=*/{kInstallUrl1, kInstallUrl2},
                        /*additional_policy_ids=*/{}))));
  }

  // Synchronize with 1 install URL.
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl1}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());

    // Empty install results.
    EXPECT_THAT(result.Get<InstallResults>(),
                UnorderedElementsAre(std::make_pair(
                    kInstallUrl1,
                    ExternallyManagedAppManager::InstallResult(
                        webapps::InstallResultCode::kSuccessAlreadyInstalled,
                        app_id))));

    // One install URL uninstalled.
    EXPECT_THAT(
        result.Get<UninstallResults>(),
        UnorderedElementsAre(std::make_pair(
            kInstallUrl2, webapps::UninstallResultCode::kInstallUrlRemoved)));

    EXPECT_EQ(app_registrar().GetAppIds().size(), 1ul);
    const WebApp* app = app_registrar().GetAppById(app_id);
    ASSERT_TRUE(app);
    EXPECT_THAT(app->management_to_external_config_map(),
                ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                           WebApp::ExternalManagementConfig(
                                               /*is_placeholder=*/false,
                                               /*install_urls=*/{kInstallUrl1},
                                               /*additional_policy_ids=*/{}))));
  }

  // Synchronize with 0 install URLs.
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());

    // Empty install results.
    EXPECT_THAT(result.Get<InstallResults>(), IsEmpty());

    // One install URL uninstalled.
    EXPECT_THAT(result.Get<UninstallResults>(),
                UnorderedElementsAre(std::make_pair(
                    kInstallUrl1, webapps::UninstallResultCode::kAppRemoved)));

    // App should be cleaned up.
    EXPECT_EQ(app_registrar().GetAppIds().size(), 0ul);
    const WebApp* app = app_registrar().GetAppById(app_id);
    ASSERT_FALSE(app);
  }
}

TEST_F(ExternallyAppManagerTest, InstallUrlChanges) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example.com/nested/install_url2.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);
  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl2, kManifestUrl, kStartUrl);
  EXPECT_EQ(app_id, app_id2);

  // First synchronize will install the app.
  {
    SynchronizeFuture result;
    external_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());

    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
        result.Get<InstallResults>();
    EXPECT_THAT(
        install_results,
        ElementsAre(std::make_pair(
            kInstallUrl,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id))));
  }

  // Second synchronize with a different install url should succeed and update
  // the install urls correctly.
  {
    SynchronizeFuture result;
    external_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl2}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
        result.Get<InstallResults>();
    EXPECT_THAT(
        install_results,
        ElementsAre(std::make_pair(
            kInstallUrl2,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id))));

    ASSERT_THAT(result.Get<UninstallResults>(),
                testing::UnorderedElementsAre(std::make_pair(
                    kInstallUrl, webapps::UninstallResultCode::kAppRemoved)));
  }

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                         WebApp::ExternalManagementConfig(
                                             /*is_placeholder=*/false,
                                             /*install_urls=*/{kInstallUrl2},
                                             /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, PolicyAppOverridesUserInstalledApp) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);

  {
    // Install user app
    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(kInstallUrl);
    install_page_state.manifest_before_default_processing->short_name =
        u"Test user app";

    auto install_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kStartUrl);
    install_info->title = u"Test user app";
    std::optional<webapps::AppId> user_app_id =
        test::InstallWebApp(profile(), std::move(install_info));

    ASSERT_TRUE(user_app_id.has_value());
    ASSERT_EQ(user_app_id.value(), app_id);
    ASSERT_TRUE(app_registrar().WasInstalledByUser(app_id));
    ASSERT_FALSE(app_registrar().HasExternalApp(app_id));
    ASSERT_EQ("Test user app", app_registrar().GetAppShortName(app_id));
  }
  {
    // Install policy app
    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(kInstallUrl);
    install_page_state.manifest_before_default_processing->short_name =
        u"Test policy app";

    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
        result.Get<InstallResults>();
    EXPECT_THAT(
        install_results,
        ElementsAre(std::make_pair(
            kInstallUrl,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id))));
    ASSERT_EQ("Test policy app", app_registrar().GetAppShortName(app_id));
  }
}

TEST_F(ExternallyAppManagerTest, NoNetworkWithPlaceholder) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  SynchronizeFuture result;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
          template_options),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // The webapps::AppId should be created from the install url.
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, kInstallUrl);

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      ElementsAre(std::make_pair(
          kInstallUrl,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id))));

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);

  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                         WebApp::ExternalManagementConfig(
                                             /*is_placeholder=*/true,
                                             /*install_urls=*/{kInstallUrl},
                                             /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, RedirectInstallUrlPlaceholder) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kRedirectToUrl =
      GURL("https://www.otherorigin.com/redirected.html");
  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  // Verify that a redirection causes a placeholder app to be installed.

  auto& page_state = web_contents_manager().GetOrCreatePageState(kInstallUrl);
  page_state.redirection_url = kRedirectToUrl;

  SynchronizeFuture result;
  external_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
          template_options),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // The webapps::AppId should be created from teh install url.
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, kInstallUrl);

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      ElementsAre(std::make_pair(
          kInstallUrl,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id))));

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                         WebApp::ExternalManagementConfig(
                                             /*is_placeholder=*/true,
                                             /*install_urls=*/{kInstallUrl},
                                             /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, PlaceholderResolvedFromSynchronize) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kRedirectToUrl =
      GURL("https://www.otherorigin.com/redirected.html");
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  auto& page_state = web_contents_manager().GetOrCreatePageState(kInstallUrl);
  page_state.redirection_url = kRedirectToUrl;
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
            template_options),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
  }

  webapps::AppId placeholder_app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, kInstallUrl);

  auto app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(placeholder_app_id));

  // Replace the redirect with an app that resolves.
  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);

  // The placeholder app should be uninstalled & the real one installed.
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
            template_options),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
  }

  app_ids = provider().registrar_unsafe().GetAppIds();

  EXPECT_THAT(app_ids, ElementsAre(app_id));
}

TEST_F(ExternallyAppManagerTest, PlaceholderResolvedFromInstallNow) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kRedirectToUrl =
      GURL("https://www.otherorigin.com/redirected.html");
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  auto& page_state = web_contents_manager().GetOrCreatePageState(kInstallUrl);
  page_state.redirection_url = kRedirectToUrl;
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
            template_options),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
  }

  webapps::AppId placeholder_app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, kInstallUrl);

  auto app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(placeholder_app_id));

  // Replace the redirect with an app that resolves.
  webapps::AppId app_id = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl, kStartUrl);

  ExternalInstallOptions options = template_options;
  options.install_url = kInstallUrl;
  options.placeholder_resolution_behavior =
      PlaceholderResolutionBehavior::kClose;
  InstallNowFuture install_future;
  provider().externally_managed_app_manager().InstallNow(
      std::move(options), install_future.GetCallback());
  ASSERT_TRUE(install_future.Wait());

  app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(app_id));
}

TEST_F(ExternallyAppManagerTest, TwoAppsSameInstallUrlSameSourceInstallNow) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kStartUrl1 = GURL("https://www.example.com/index1.html");
  const GURL kStartUrl2 = GURL("https://www.example.com/index2.html");
  const GURL kManifestUrl1 = GURL("https://www.example.com/manifest1.json");
  const GURL kManifestUrl2 = GURL("https://www.example.com/manifest2.json");

  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);

  webapps::AppId app_id1 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl1, kStartUrl1);

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl;
    base::test::TestFuture<const GURL&,
                           ExternallyManagedAppManager::InstallResult>
        install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
  }

  auto app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(app_id1));

  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl2, kStartUrl2);

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl;
    InstallNowFuture install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
  }

  // TODO(crbug.com/40264854): This keeps the original app, but perhaps
  // should install app_id2.
  app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(app_id1));
}

TEST_F(ExternallyAppManagerTest, TwoAppsSameInstallUrlTwoSourcesInstallNow) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kStartUrl1 = GURL("https://www.example.com/index1.html");
  const GURL kStartUrl2 = GURL("https://www.example.com/index2.html");
  const GURL kManifestUrl1 = GURL("https://www.example.com/manifest1.json");
  const GURL kManifestUrl2 = GURL("https://www.example.com/manifest2.json");

  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);

  webapps::AppId app_id1 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl1, kStartUrl1);

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl;
    base::test::TestFuture<const GURL&,
                           ExternallyManagedAppManager::InstallResult>
        install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
  }

  auto app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(app_id1));

  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl2, kStartUrl2);

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl;
    options.install_source = ExternalInstallSource::kInternalDefault;
    InstallNowFuture install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
  }

  // TODO(crbug.com/40264854): Currently, this keeps the original app,
  // but we should eventually resolve all apps to app_id2.
  app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(app_id1));
}

TEST_F(ExternallyAppManagerTest, TwoAppsSameInstallUrlTwoSourcesSynchronize) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kStartUrl1 = GURL("https://www.example.com/index1.html");
  const GURL kStartUrl2 = GURL("https://www.example.com/index2.html");
  const GURL kManifestUrl1 = GURL("https://www.example.com/manifest1.json");
  const GURL kManifestUrl2 = GURL("https://www.example.com/manifest2.json");

  ExternalInstallOptions template_options(
      kInstallUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);

  webapps::AppId app_id1 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl1, kStartUrl1);

  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
            template_options),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
  }

  auto app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, ElementsAre(app_id1));

  webapps::AppId app_id2 = web_contents_manager().CreateBasicInstallPageState(
      kInstallUrl, kManifestUrl2, kStartUrl2);

  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kInternalDefault,
            template_options),
        ExternalInstallSource::kInternalDefault, result.GetCallback());
    ASSERT_TRUE(result.Wait());
  }

  // TODO(crbug.com/40264854): Currently this resolves to app_id1, but
  // should probably eventually resolve to app_id2.
  app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids, UnorderedElementsAre(app_id1));
}

TEST_F(ExternallyAppManagerTest, PlaceholderFixedBySecondInstallUrlInstallNow) {
  const GURL kInstallUrl1 = GURL("https://www.example.com/install_url1.html");
  const GURL kInstallUrl2 = GURL("https://www.example.com/install_url2.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest1.json");

  ExternalInstallOptions template_options(
      kInstallUrl1, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  // The first install creates a placeholder at kInstallUrl1, as that redirects.
  // The second install doesn't redirect, and the app at kInstallUrl2 points to
  // kInstallUrl1 as it's start_url (basically - it's identity conflicts with
  // the placeholder app's default identity).

  auto& page_state = web_contents_manager().GetOrCreatePageState(kInstallUrl1);
  page_state.redirection_url =
      GURL("https://www.otherorigin.com/redirect.html");

  webapps::AppId app_at_install_url =
      web_contents_manager().CreateBasicInstallPageState(
          kInstallUrl2, kManifestUrl, kInstallUrl1);

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl1;
    InstallNowFuture install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
    EXPECT_THAT(
        install_future.Get(),
        Eq(std::make_tuple(kInstallUrl1,
                           ExternallyManagedAppManager::InstallResult(
                               webapps::InstallResultCode::kSuccessNewInstall,
                               app_at_install_url))));
  }

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl2;
    base::test::TestFuture<const GURL&,
                           ExternallyManagedAppManager::InstallResult>
        install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
    EXPECT_THAT(
        install_future.Get(),
        Eq(std::make_tuple(kInstallUrl2,
                           ExternallyManagedAppManager::InstallResult(
                               webapps::InstallResultCode::kSuccessNewInstall,
                               app_at_install_url))));
  }

  // The current implementation records placeholder information per-source, so
  // when the second install succeeds, it overrides the `is_placeholder` to
  // `false`, and thus the first install is considered fully installed now too.
  // This is in contrast to the behavior if the two installs are different
  // sources, which will (correctly?) evaluate the manifest served by the
  // install url of the first install, which could be a different app identity.
  const WebApp* app =
      provider().registrar_unsafe().GetAppById(app_at_install_url);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(
                  WebAppManagement::kPolicy,
                  WebApp::ExternalManagementConfig(
                      /*is_placeholder=*/false,
                      /*install_urls=*/{kInstallUrl1, kInstallUrl2},
                      /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest,
       PlaceholderFixedBySecondInstallUrlSynchronize) {
  const GURL kInstallUrl1 = GURL("https://www.example.com/install_url1.html");
  const GURL kInstallUrl2 = GURL("https://www.example.com/install_url2.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest1.json");

  // The first install creates a placeholder at kInstallUrl1, as that redirects.
  // The second install doesn't redirect, and the app at kInstallUrl2 points to
  // kInstallUrl1 as it's start_url (basically - it's identity conflicts with
  // the placeholder app's default identity).

  ExternalInstallOptions template_options(
      kInstallUrl1, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  auto& page_state = web_contents_manager().GetOrCreatePageState(kInstallUrl1);
  page_state.redirection_url =
      GURL("https://www.otherorigin.com/redirect.html");

  webapps::AppId app_at_install_url =
      web_contents_manager().CreateBasicInstallPageState(
          kInstallUrl2, kManifestUrl, /*start_url=*/kInstallUrl1);

  SynchronizeFuture result;
  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl1, kInstallUrl2}, ExternalInstallSource::kExternalPolicy,
          template_options),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      UnorderedElementsAre(
          std::make_pair(kInstallUrl1,
                         ExternallyManagedAppManager::InstallResult(
                             webapps::InstallResultCode::kSuccessNewInstall,
                             app_at_install_url)),
          std::make_pair(kInstallUrl2,
                         ExternallyManagedAppManager::InstallResult(
                             webapps::InstallResultCode::kSuccessNewInstall,
                             app_at_install_url))));

  // The current implementation records placeholder information per-source, so
  // when the second install succeeds, it overrides the `is_placeholder` to
  // `false`, and thus the first install is considered fully installed now too.
  const WebApp* app =
      provider().registrar_unsafe().GetAppById(app_at_install_url);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(
                  WebAppManagement::kPolicy,
                  WebApp::ExternalManagementConfig(
                      /*is_placeholder=*/false,
                      /*install_urls=*/{kInstallUrl1, kInstallUrl2},
                      /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, PlaceholderFullInstallConflictCanUpdate) {
  // This test exists to test what happens when a placeholder install conflicts
  // with a another install from a different source, and then resolves to a
  // separate app entirely after the placeholder state is fixed.

  // Phase 1 configuration with redirect:
  // - kInstallUrl1 will redirect & generate a placeholder
  // - kInstallUrl2 will serve a manifest with kInstallUrl1 as the start_url

  // Phase 2 configuration - redirect removed
  // - kInstallUrl1 will serve a manifest with kStartUrl as the start_url.

  // What happens when resolving the placeholder at kInstallUrl1? Currently this
  // updates so that kStartUrl1 is installed.

  const GURL kInstallUrl1 = GURL("https://www.example.com/install_url1.html");
  const GURL kInstallUrl2 = GURL("https://www.example.com/install_url2.html");
  const GURL kStartUrl1 = GURL("https://www.example.com/index1.html");
  const GURL kManifestUrl1 = GURL("https://www.example.com/manifest1.json");
  const GURL kManifestUrl2 = GURL("https://www.example.com/manifest2.json");

  ExternalInstallOptions template_options(
      kInstallUrl1, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  // Phase 1 state
  auto& page_state = web_contents_manager().GetOrCreatePageState(kInstallUrl1);
  page_state.redirection_url =
      GURL("https://www.otherorigin.com/redirect.html");

  webapps::AppId app_at_install_url =
      web_contents_manager().CreateBasicInstallPageState(
          kInstallUrl2, kManifestUrl1, kInstallUrl1);

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl1;
    base::test::TestFuture<const GURL&,
                           ExternallyManagedAppManager::InstallResult>
        install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
    EXPECT_THAT(
        install_future.Get(),
        Eq(std::make_tuple(kInstallUrl1,
                           ExternallyManagedAppManager::InstallResult(
                               webapps::InstallResultCode::kSuccessNewInstall,
                               app_at_install_url))));
  }

  {
    ExternalInstallOptions options(kInstallUrl2,
                                   mojom::UserDisplayMode::kStandalone,
                                   ExternalInstallSource::kInternalDefault);
    base::test::TestFuture<const GURL&,
                           ExternallyManagedAppManager::InstallResult>
        install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());
    EXPECT_THAT(
        install_future.Get(),
        Eq(std::make_tuple(kInstallUrl2,
                           ExternallyManagedAppManager::InstallResult(
                               webapps::InstallResultCode::kSuccessNewInstall,
                               app_at_install_url))));
  }

  const WebApp* app =
      provider().registrar_unsafe().GetAppById(app_at_install_url);
  ASSERT_TRUE(app);
  EXPECT_THAT(
      app->management_to_external_config_map(),
      UnorderedElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                          WebApp::ExternalManagementConfig(
                                              /*is_placeholder=*/true,
                                              /*install_urls=*/{kInstallUrl1},
                                              /*additional_policy_ids=*/{})),
                           std::make_pair(WebAppManagement::kDefault,
                                          WebApp::ExternalManagementConfig(
                                              /*is_placeholder=*/false,
                                              /*install_urls=*/{kInstallUrl2},
                                              /*additional_policy_ids=*/{}))));
  // Phase 2 - undo the redirection, and point to start url.
  webapps::AppId app_at_start_url =
      web_contents_manager().CreateBasicInstallPageState(
          kInstallUrl1, kManifestUrl2, kStartUrl1);

  {
    ExternalInstallOptions options = template_options;
    options.install_url = kInstallUrl1;
    base::test::TestFuture<const GURL&,
                           ExternallyManagedAppManager::InstallResult>
        install_future;
    provider().externally_managed_app_manager().InstallNow(
        std::move(options), install_future.GetCallback());
    ASSERT_TRUE(install_future.Wait());

    EXPECT_THAT(
        install_future.Get(),
        Eq(std::make_tuple(kInstallUrl1,
                           ExternallyManagedAppManager::InstallResult(
                               webapps::InstallResultCode::kSuccessNewInstall,
                               app_at_start_url))));
  }

  // This is the current behavior, but it could change if we decide that
  // 'placeholder' is a per-app state instead of a per-app-and-source state.
  auto app_ids = provider().registrar_unsafe().GetAppIds();
  EXPECT_THAT(app_ids,
              UnorderedElementsAre(app_at_install_url, app_at_start_url));
}

}  // namespace
}  // namespace web_app
