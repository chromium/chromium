// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include <optional>
#include <string_view>
#include <variant>

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_orphaned_isolated_web_apps_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/integrity_block_data_matcher.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/update_channel.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::VariantWith;

using UpdateDiscoveryTaskFuture =
    base::test::TestFuture<IsolatedWebAppUpdateDiscoveryTask::CompletionStatus>;

constexpr std::string_view kIndexHtml304WithServiceWorker = R"(
  <head>
    <script type="text/javascript" src="/register-sw.js"></script>
    <title>3.0.4</title>
  </head>
  <body>
    <h1>Hello from version 3.0.4</h1>
  </body>)";

static constexpr std::string_view kIndexHtml706 = R"(
  <head>
    <title>7.0.6</title>
  </head>
  <body>
    <h1>Hello from version 7.0.6</h1>
  </body>)";

constexpr std::string_view kRegisterServiceWorkerScript = R"(
  window.trustedTypes.createPolicy('default', {
    createHTML: (html) => html,
    createScriptURL: (url) => url,
    createScript: (script) => script,
  });
  if (location.search.includes('register-sw=1')) {
    navigator.serviceWorker.register("/sw.js");
  }
)";

constexpr std::string_view kServiceWorkerScript = R"(
  self.addEventListener('install', (event) => {
    self.skipWaiting();
  });
  self.addEventListener("fetch", (event) => {
    console.log("SW: used fetch: " + event.request.url);
    event.respondWith(new Response("", {
      status: 404,
      statusText: "Not Found",
    }));
  });
)";

const UpdateChannel kBetaChannel = UpdateChannel::Create("beta").value();
const UpdateChannel kRandomChannel = UpdateChannel::Create("random").value();

#if BUILDFLAG(IS_CHROMEOS)
void CheckBundleExists(Profile* profile, const base::FilePath& directory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::DirectoryExists(
      CHECK_DEREF(profile).GetPath().Append(kIwaDirName).Append(directory)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class ServiceWorkerVersionStartedRunningWaiter
    : public content::ServiceWorkerContextObserver {
 public:
  explicit ServiceWorkerVersionStartedRunningWaiter(
      content::StoragePartition* storage_partition) {
    CHECK(storage_partition);
    observation_.Observe(storage_partition->GetServiceWorkerContext());
  }

  ServiceWorkerVersionStartedRunningWaiter(
      const ServiceWorkerVersionStartedRunningWaiter&) = delete;
  ServiceWorkerVersionStartedRunningWaiter& operator=(
      const ServiceWorkerVersionStartedRunningWaiter&) = delete;

  void AwaitStartedRunning() { CHECK(future_.Wait()); }

 protected:
  // `content::ServiceWorkerContextObserver`:
  void OnDestruct(content::ServiceWorkerContext* context) override {
    observation_.Reset();
  }
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    future_.SetValue(version_id);
  }

 private:
  base::test::TestFuture<int64_t> future_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      observation_{this};
};

class IsolatedWebAppUpdateManagerBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  void AddNewBundleToUpdateServer(std::string_view app_name,
                                  std::string_view app_version,
                                  std::optional<std::vector<UpdateChannel>>
                                      update_channels = std::nullopt) {
    update_server_mixin_.AddBundle(
        IsolatedWebAppBuilder(
            ManifestBuilder().SetName(app_name).SetVersion(app_version))
            .AddHtml("/", kIndexHtml706)
            .BuildBundle(GetWebBundleId(), {test::GetDefaultEd25519KeyPair()}),
        update_channels);
  }
  url::Origin GetAppOrigin() const {
    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(GetWebBundleId())
        .origin();
  }
  webapps::AppId GetAppId() const {
    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(GetWebBundleId())
        .app_id();
  }
  web_package::SignedWebBundleId GetWebBundleId() const {
    return test::GetDefaultEd25519WebBundleId();
  }
  const WebApp* GetIsolatedWebApp(const webapps::AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(app_id);
  }

 protected:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    AddInitialBundle();
  }

  void AddInitialBundle() {
    update_server_mixin_.AddBundle(
        IsolatedWebAppBuilder(
            ManifestBuilder().SetName("app-3.0.4").SetVersion("3.0.4"))
            .AddHtml("/", kIndexHtml304WithServiceWorker)
            .AddJs("/register-sw.js", kRegisterServiceWorkerScript)
            .AddJs("/sw.js", kServiceWorkerScript)
            .BuildBundle(GetWebBundleId(), {test::GetDefaultEd25519KeyPair()}));
  }

  IsolatedWebAppUpdateServerMixin update_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest, Succeeds) {
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  manifest_updated_observer.Wait();

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       NoUpdatesToIwaPinnedToCurrentVersion) {
  base::HistogramTester histogram_tester;

  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            update_server_mixin_.CreateForceInstallPolicyEntry(
                /*web_bundle_id=*/GetWebBundleId(),
                /*update_channel=*/std::nullopt,
                /*pinned_version=*/base::Version("3.0.4"))));

    web_app::WebAppTestInstallObserver(browser()->profile())
        .BeginListeningAndWait({GetAppId()});
  }

  EXPECT_THAT(GetIsolatedWebApp(GetAppId())->isolation_data()->version(),
              base::Version("3.0.4"));

  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(0ul));

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-3.0.4"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("3.0.4")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       NoUpdatesToIwaDowngradingToCurrentVersion) {
  base::HistogramTester histogram_tester;

  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            update_server_mixin_.CreateForceInstallPolicyEntry(
                /*web_bundle_id=*/GetWebBundleId(),
                /*update_channel=*/std::nullopt,
                /*pinned_version=*/base::Version("3.0.4"),
                /*allow_downgrades=*/true)));

    web_app::WebAppTestInstallObserver(browser()->profile())
        .BeginListeningAndWait({GetAppId()});
  }

  EXPECT_THAT(GetIsolatedWebApp(GetAppId())->isolation_data()->version(),
              base::Version("3.0.4"));

  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(0ul));

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-3.0.4"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("3.0.4")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}
IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       SucceedsWithDowngrade) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  EXPECT_EQ(GetIsolatedWebApp(GetAppId())->isolation_data()->version(),
            base::Version("3.0.4"));

  // Pin IWA to version 1.0.0 and allow downgrading.
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/std::nullopt,
              /*pinned_version=*/base::Version("1.0.0"),
              /*allow_downgrades=*/true)));

  AddNewBundleToUpdateServer("app-1.0.0", "1.0.0");

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  content::BrowsingDataRemover* browsing_data_remover =
      profile()->GetBrowsingDataRemover();
  base::test::TestFuture<void> future;
  browsing_data_remover->SetWouldCompleteCallbackForTesting(
      base::BindLambdaForTesting([&](base::OnceClosure callback) {
        if (browsing_data_remover->GetPendingTaskCountForTesting() == 1) {
          future.SetValue();
        }
        std::move(callback).Run();
      }));

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  manifest_updated_observer.Wait();
  EXPECT_TRUE(future.Wait());

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-1.0.0"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("1.0.0")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       FailToDowngradeToNotExistentVersion) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  EXPECT_EQ(GetIsolatedWebApp(GetAppId())->isolation_data()->version(),
            base::Version("3.0.4"));

  // Pin IWA to version 1.0.0 and allow downgrading.
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/kBetaChannel,
              /*pinned_version=*/base::Version("1.0.0"),
              /*allow_downgrades=*/true)));

  AddNewBundleToUpdateServer("app-1.0.0", "1.0.0");

  UpdateDiscoveryTaskFuture initial_update_future;
  UpdateDiscoveryTaskResultWaiter initial_update_waiter(
      provider(), GetAppId(), initial_update_future.GetCallback());

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
  EXPECT_THAT(initial_update_future.Take(),
              ErrorIs(IsolatedWebAppUpdateDiscoveryTask::Error::
                          kUpdateManifestNoApplicableVersion));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       DoNotUpdateDowngradedApp) {
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  EXPECT_EQ(GetIsolatedWebApp(GetAppId())->isolation_data()->version(),
            base::Version("3.0.4"));

  // Pin IWA to version 5.0.5 and allow downgrading.
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/std::nullopt,
              /*pinned_version=*/base::Version("1.0.5"),
              /*allow_downgrades=*/true)));

  AddNewBundleToUpdateServer("app-1.0.5", "1.0.5");

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  manifest_updated_observer.Wait();

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-1.0.5"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("1.0.5")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  // Add new version 7.0.6
  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(0ul));

  EXPECT_EQ(GetIsolatedWebApp(GetAppId())->isolation_data()->version(),
            base::Version("1.0.5"));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}
IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       UpdateToPinnedVersionInstallsWhenAvailable) {
  // Install IWA at version 3.0.4.
  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            update_server_mixin_.CreateForceInstallPolicyEntry(
                /*web_bundle_id=*/GetWebBundleId())));

    web_app::WebAppTestInstallObserver(browser()->profile())
        .BeginListeningAndWait({GetAppId()});
  }

  // Pin IWA to version 7.0.6.
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/std::nullopt,
              /*pinned_version=*/base::Version("7.0.6"))));

  // Add an update to 5.0.5, which should be ignored because the IWA is pinned
  // to 7.0.6.
  {
    AddNewBundleToUpdateServer("app-5.0.5", "5.0.5");

    UpdateDiscoveryTaskFuture initial_update_future;
    UpdateDiscoveryTaskResultWaiter initial_update_waiter(
        provider(), GetAppId(), initial_update_future.GetCallback());

    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
    EXPECT_THAT(initial_update_future.Take(),
                ErrorIs(IsolatedWebAppUpdateDiscoveryTask::Error::
                            kUpdateManifestNoApplicableVersion));
  }

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-3.0.4"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("3.0.4")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  // The IWA successfully updates to the new version because it was
  // previously pinned to that version.
  {
    AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({GetAppId()});

    UpdateDiscoveryTaskFuture second_update_future;
    UpdateDiscoveryTaskResultWaiter second_update_waiter(
        provider(), GetAppId(), second_update_future.GetCallback());

    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
    EXPECT_THAT(second_update_future.Take(),
                ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::
                            kUpdateFoundAndSavedInDatabase));

    manifest_updated_observer.Wait();
    EXPECT_THAT(
        GetIsolatedWebApp(GetAppId()),
        test::IwaIs(
            Eq("app-7.0.6"),
            test::IsolationDataIs(
                Property("variant", &IsolatedWebAppStorageLocation::variant,
                         VariantWith<IwaStorageOwnedBundle>(_)),
                Eq(base::Version("7.0.6")),
                /*controlled_frame_partitions=*/_,
                /*pending_update_info=*/Eq(std::nullopt),
                /*integrity_block_data=*/_)));
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       ChangePinnedVersionToNewerVersion) {
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/std::nullopt,
              /*pinned_version=*/base::Version("3.0.4"))));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  AddNewBundleToUpdateServer("app-5.0.5", "5.0.5");
  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/std::nullopt,
              /*pinned_version=*/base::Version("5.0.5"))));
  manifest_updated_observer.BeginListening({GetAppId()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  manifest_updated_observer.Wait();

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-5.0.5"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("5.0.5")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest, Unpinning) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/std::nullopt,
              /*pinned_version=*/base::Version("3.0.4"))));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  // IWA does not update to newer version that becomes available due to being
  // pinned.
  {
    AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");
    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(0ul));
  }

  // After pinned_version is unset, the app is updated to the latest version
  // available.
  {
    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({GetAppId()});

    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            update_server_mixin_.CreateForceInstallPolicyEntry(
                /*web_bundle_id=*/GetWebBundleId())));

    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

    manifest_updated_observer.Wait();

    EXPECT_THAT(
        GetIsolatedWebApp(GetAppId()),
        test::IwaIs(
            Eq("app-7.0.6"),
            test::IsolationDataIs(
                Property("variant", &IsolatedWebAppStorageLocation::variant,
                         VariantWith<IwaStorageOwnedBundle>(_)),
                Eq(base::Version("7.0.6")),
                /*controlled_frame_partitions=*/_,
                /*pending_update_info=*/Eq(std::nullopt),
                /*integrity_block_data=*/_)));
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       PinningWithChannelChange) {
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  AddNewBundleToUpdateServer("app-5.0.5", "5.0.5", {{kBetaChannel}});
  AddNewBundleToUpdateServer("app-6.0.0", "6.0.0", {{kRandomChannel}});
  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              /*web_bundle_id=*/GetWebBundleId(),
              /*update_channel=*/kBetaChannel,
              /*pinned_version=*/base::Version("5.0.5"))));

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  manifest_updated_observer.Wait();

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-5.0.5"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("5.0.5")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       PinningWithNonDefaultChannelToNonExistentVersion) {
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  // IWA is pinned to non-existent version. Attempt to update IWA to this
  // version fails.
  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            update_server_mixin_.CreateForceInstallPolicyEntry(
                /*web_bundle_id=*/GetWebBundleId(),
                /*update_channel=*/kBetaChannel,
                /*pinned_version=*/base::Version("6.0.0"))));

    UpdateDiscoveryTaskFuture future;
    UpdateDiscoveryTaskResultWaiter initial_update_waiter(
        provider(), GetAppId(), future.GetCallback());

    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
    EXPECT_THAT(future.Take(),
                ErrorIs(IsolatedWebAppUpdateDiscoveryTask::Error::
                            kUpdateManifestNoApplicableVersion));
  }

  // Version equal to pinned_version appears in update manifest. The app is
  // successfully updated to it.
  {
    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({GetAppId()});

    AddNewBundleToUpdateServer("app-5.0.5", "5.0.5", {{kBetaChannel}});
    AddNewBundleToUpdateServer("app-6.0.0", "6.0.0", {{kBetaChannel}});
    AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

    manifest_updated_observer.Wait();

    EXPECT_THAT(
        GetIsolatedWebApp(GetAppId()),
        test::IwaIs(
            Eq("app-6.0.0"),
            test::IsolationDataIs(
                Property("variant", &IsolatedWebAppStorageLocation::variant,
                         VariantWith<IwaStorageOwnedBundle>(_)),
                Eq(base::Version("6.0.0")),
                /*controlled_frame_partitions=*/_,
                /*pending_update_info=*/Eq(std::nullopt),
                /*integrity_block_data=*/_)));
  }

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       SucceedsWithNonDefaultChannel) {
  base::HistogramTester histogram_tester;

  // Updates initial app version with channels.
  {
    AddNewBundleToUpdateServer("app-3.0.4", "3.0.4", {{kBetaChannel}});

    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            update_server_mixin_.CreateForceInstallPolicyEntry(GetWebBundleId(),
                                                               kBetaChannel)));

    web_app::WebAppTestInstallObserver(browser()->profile())
        .BeginListeningAndWait({GetAppId()});
  }

  UpdateDiscoveryTaskFuture future;
  UpdateDiscoveryTaskResultWaiter waiter(provider(), GetAppId(),
                                         future.GetCallback());

  AddNewBundleToUpdateServer(
      "app-7.0.6", "7.0.6",
      std::vector<UpdateChannel>{kBetaChannel, kRandomChannel});

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
  EXPECT_THAT(future.Take(),
              ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::
                          kUpdateFoundAndSavedInDatabase));

  manifest_updated_observer.Wait();
  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       NoUpdatesOnCurrentChannel) {
  base::HistogramTester histogram_tester;

  AddNewBundleToUpdateServer(
      "app-7.0.6", "7.0.6",
      std::vector<UpdateChannel>{kRandomChannel, kBetaChannel});

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  UpdateDiscoveryTaskFuture future;
  UpdateDiscoveryTaskResultWaiter waiter(provider(), GetAppId(),
                                         future.GetCallback());

  // Adding new version for an IWA triggers Discovery Task, but no updates will
  // be applied as new version is not meant for the "default" channel.
  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
  EXPECT_THAT(
      future.Take(),
      ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound));

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-3.0.4"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("3.0.4")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       SucceedsWithChannelSwitch) {
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  AddNewBundleToUpdateServer(
      "app-7.0.6", "7.0.6",
      std::vector<UpdateChannel>{kBetaChannel, kRandomChannel});
  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  UpdateDiscoveryTaskFuture future;
  UpdateDiscoveryTaskResultWaiter waiter(provider(), GetAppId(),
                                         future.GetCallback());

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
  EXPECT_THAT(
      future.Take(),
      ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound));

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-3.0.4"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("3.0.4")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(GetWebBundleId(),
                                                             kBetaChannel)));

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  manifest_updated_observer.Wait();
  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("WebApp.Isolated.UpdateSuccess",
                                     /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError",
                                    /*expected_count=*/0);
}

// TODO(b/402650079) flaky on mac
#if BUILDFLAG(IS_MAC)
#define MAYBE_SucceedsWithServiceWorkerWithFetchHandler \
  DISABLED_SucceedsWithServiceWorkerWithFetchHandler
#else
#define MAYBE_SucceedsWithServiceWorkerWithFetchHandler \
  SucceedsWithServiceWorkerWithFetchHandler
#endif
IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       MAYBE_SucceedsWithServiceWorkerWithFetchHandler) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});
  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({GetAppId()});

  // Open the app, which will register the Service Worker.
  content::RenderFrameHost* app_frame = OpenApp(GetAppId(), "?register-sw=1");
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(GetAppId()), Eq(1ul));

  // Wait for the Service Worker to start running.
  content::StoragePartition* storage_partition =
      app_frame->GetStoragePartition();
  ServiceWorkerVersionStartedRunningWaiter waiter(storage_partition);
  waiter.AwaitStartedRunning();
  test::CheckServiceWorkerStatus(
      GetAppOrigin().GetURL(), storage_partition,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  // Updates will be applied once the app's window is closed.
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(GetAppId()), Eq(0ul));

  manifest_updated_observer.Wait();

  EXPECT_THAT(GetIsolatedWebApp(GetAppId()),
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));
}

// Session restore related tests that can only be run in ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       PRE_AppliesUpdateOnStartupIfAppWindowNeverCloses) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile(), pref);

  profile()->GetPrefs()->CommitPendingWrite();

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  // Open the app to prevent the update from being applied.
  OpenApp(GetAppId());
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(GetAppId()), Eq(1ul));

  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");
  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  ASSERT_TRUE(base::test::RunUntil([this]() {
    const WebApp* app = GetIsolatedWebApp(GetAppId());
    return app->isolation_data()->pending_update_info().has_value();
  }));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       AppliesUpdateOnStartupIfAppWindowNeverCloses) {
  // Wait for the update to be applied if it hasn't already.
  const auto* web_app = GetIsolatedWebApp(GetAppId());
  if (web_app->isolation_data()->version() != base::Version("7.0.6")) {
    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({GetAppId()});
    manifest_updated_observer.Wait();
    web_app = GetIsolatedWebApp(GetAppId());
  }

  EXPECT_THAT(web_app,
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  Browser* app_window =
      AppBrowserController::FindForWebApp(*profile(), GetAppId());
  ASSERT_THAT(app_window, NotNull());
  content::WebContents* web_contents =
      app_window->tab_strip_model()->GetActiveWebContents();

  content::TitleWatcher title_watcher(web_contents, u"7.0.6");
  title_watcher.AlsoWaitForTitle(u"3.0.4");
  EXPECT_THAT(title_watcher.WaitAndGetTitle(), Eq(u"7.0.6"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       PendingUpdateDoesNotGetCleanedUp) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              GetWebBundleId())));

  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile(), pref);

  profile()->GetPrefs()->CommitPendingWrite();

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});

  // Open the app to prevent the update from being applied.
  OpenApp(GetAppId());
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(GetAppId()), Eq(1ul));

  AddNewBundleToUpdateServer("app-7.0.6", "7.0.6");
  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  ASSERT_TRUE(base::test::RunUntil([this]() {
    const WebApp* app = GetIsolatedWebApp(GetAppId());
    return app->isolation_data()->pending_update_info().has_value();
  }));

  const auto& isolation_data = GetIsolatedWebApp(GetAppId())->isolation_data();
  const auto& app_location =
      base::FilePath(std::get_if<IsolatedWebAppStorageLocation::OwnedBundle>(
                         &isolation_data->location().variant())
                         ->dir_name_ascii());
  const auto& app_update_location = base::FilePath(
      std::get_if<IsolatedWebAppStorageLocation::OwnedBundle>(
          &isolation_data->pending_update_info()->location.variant())
          ->dir_name_ascii());

  // Check that both IWA directories (currently running instance and the update)
  // are there.
  CheckBundleExists(profile(), app_location);
  CheckBundleExists(profile(), app_update_location);

  // Run the cleanup while both bundles are there.
  base::test::TestFuture<
      base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                     CleanupOrphanedIsolatedWebAppsCommandError>>
      future;
  provider().scheduler().CleanupOrphanedIsolatedApps(future.GetCallback());
  const bool command_successful =
      future
          .Get<base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                              CleanupOrphanedIsolatedWebAppsCommandError>>()
          .has_value();
  ASSERT_TRUE(command_successful);

  // Neither of the bundles should be deleted.
  CheckBundleExists(profile(), app_location);
  CheckBundleExists(profile(), app_update_location);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class IsolatedWebAppUpdateManagerWithKeyRotationBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  const WebApp* GetIsolatedWebApp(const webapps::AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(app_id);
  }

 protected:
  void AddBundleSignedBy(const web_package::test::KeyPair& key_pair) {
    update_server_mixin_.AddBundle(
        IsolatedWebAppBuilder(
            ManifestBuilder().SetName("app-1.0.0").SetVersion("1.0.0"))
            .AddHtml("/", R"(
              <head>
                <title>1.0.0</title>
              </head>
            )")
            .AddHtml("/another_page.html", R"(
              <head>
                <title>another page</title>
              </head>
            )")
            .BuildBundle(web_bundle_id_, {key_pair}));
  }

  IsolatedWebAppUpdateServerMixin update_server_mixin_{&mixin_host_};
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  base::test::ScopedFeatureList features_{
      component_updater::kIwaKeyDistributionComponent};
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  web_package::SignedWebBundleId web_bundle_id_ =
      test::GetDefaultEd25519WebBundleId();

  // Override the pre-install component directory and its alternative directory
  // so that the component update will not find the pre-installed key dist
  // component.
  base::ScopedPathOverride preinstalled_dir_override_{
      component_updater::DIR_COMPONENT_PREINSTALLED};
  base::ScopedPathOverride preinstalled_alt_dir_override_{
      component_updater::DIR_COMPONENT_PREINSTALLED_ALT};
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerWithKeyRotationBrowserTest,
                       Succeeds) {
  auto app_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_)
          .app_id();

  // Add a bundle with version 1.0.0 signed by the original key corresponding to
  // `web_bundle_id_`.
  AddBundleSignedBy(test::GetDefaultEd25519KeyPair());

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(web_bundle_id_)));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});

  EXPECT_THAT(
      GetIsolatedWebApp(app_id),
      test::IwaIs(Eq("app-1.0.0"),
                  test::IsolationDataIs(
                      /*location=*/_, Eq(base::Version("1.0.0")),
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/Eq(std::nullopt),
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(
                          test::GetDefaultEd25519KeyPair().public_key))));

  // Add a bundle with version 1.0.0 signed by a rotated key.
  AddBundleSignedBy(test::GetDefaultEcdsaP256KeyPair());

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({app_id});
  // Key rotation should trigger a discovery in the update manager.
  EXPECT_THAT(
      test::InstallIwaKeyDistributionComponent(
          base::Version("0.1.0"), test::GetDefaultEd25519WebBundleId().id(),
          test::GetDefaultEcdsaP256KeyPair().public_key.bytes()),
      HasValue());
  manifest_updated_observer.Wait();

  // The app's integrity block data must be different now due to an update.
  EXPECT_THAT(
      GetIsolatedWebApp(app_id),
      test::IwaIs(Eq("app-1.0.0"),
                  test::IsolationDataIs(
                      /*location=*/_, Eq(base::Version("1.0.0")),
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/Eq(std::nullopt),
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(
                          test::GetDefaultEcdsaP256KeyPair().public_key))));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerWithKeyRotationBrowserTest,
                       AppStopsOpeningOnUpdateFailure) {
  auto app_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_)
          .app_id();

  // Add a bundle with version 1.0.0 signed by the original key corresponding to
  // `web_bundle_id_`.
  AddBundleSignedBy(test::GetDefaultEd25519KeyPair());

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(web_bundle_id_)));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});

  EXPECT_THAT(
      GetIsolatedWebApp(app_id),
      test::IwaIs(Eq("app-1.0.0"),
                  test::IsolationDataIs(
                      /*location=*/_, Eq(base::Version("1.0.0")),
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/Eq(std::nullopt),
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(
                          test::GetDefaultEd25519KeyPair().public_key))));

  // Open the app and ensure it loads the content properly. This will also cache
  // a bundle reader.
  {
    auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
    content::WebContents* web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();

    content::TitleWatcher title_watcher(web_contents, u"1.0.0");
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), u"1.0.0");
    app_browser->window()->Close();
    ui_test_utils::WaitForBrowserToClose(app_browser);
  }

  // Key rotation should trigger an unsuccessful discovery in the update manager
  // and clear the reader cache.
  EXPECT_THAT(
      test::InstallIwaKeyDistributionComponent(
          base::Version("0.1.0"), test::GetDefaultEd25519WebBundleId().id(),
          test::GetDefaultEcdsaP256KeyPair().public_key.bytes()),
      HasValue());

  // Now an attempt to open the app should display the "missing or damaged"
  // page.
  {
    auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
    content::WebContents* web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();

    EXPECT_THAT(EvalJs(web_contents, "document.body.innerText").ExtractString(),
                HasSubstr("This application is missing or damaged"));

    app_browser->window()->Close();
    ui_test_utils::WaitForBrowserToClose(app_browser);
  }

  // Apply a late update.
  {
    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({app_id});

    // Add a bundle with version 1.0.0 signed by a rotated key.
    AddBundleSignedBy(test::GetDefaultEcdsaP256KeyPair());
    EXPECT_EQ(provider().iwa_update_manager().DiscoverUpdatesNow(), 1u);
    manifest_updated_observer.Wait();
  }

  // The app should open as expected.
  {
    auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
    content::WebContents* web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();

    content::TitleWatcher title_watcher(web_contents, u"1.0.0");
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), u"1.0.0");
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerWithKeyRotationBrowserTest,
                       DoesntAffectRunningApps) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_);
  auto app_id = url_info.app_id();

  // Add a bundle with version 1.0.0 signed by the original key corresponding to
  // `web_bundle_id_`.
  AddBundleSignedBy(test::GetDefaultEd25519KeyPair());

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(web_bundle_id_)));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});

  EXPECT_THAT(
      GetIsolatedWebApp(app_id),
      test::IwaIs(Eq("app-1.0.0"),
                  test::IsolationDataIs(
                      /*location=*/_, Eq(base::Version("1.0.0")),
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/Eq(std::nullopt),
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(
                          test::GetDefaultEd25519KeyPair().public_key))));

  // Open the app and ensure it loads the content properly. This will also cache
  // a bundle reader.
  auto* app_browser = LaunchWebAppBrowserAndWait(app_id);
  {
    content::WebContents* web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();

    content::TitleWatcher title_watcher(web_contents, u"1.0.0");
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), u"1.0.0");
  }

  // Key rotation should trigger an unsuccessful discovery in the update manager
  // and queue a cache clear request for this bundle reader.
  EXPECT_THAT(
      test::InstallIwaKeyDistributionComponent(
          base::Version("0.1.0"), test::GetDefaultEd25519WebBundleId().id(),
          test::GetDefaultEcdsaP256KeyPair().public_key.bytes()),
      HasValue());

  // The currently open app should not be affected.
  {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        app_browser, url_info.origin().GetURL().Resolve("/another_page.html")));
    content::WebContents* web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();

    content::TitleWatcher title_watcher(web_contents, u"another page");
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), u"another page");
  }

  // Close the browser.
  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);

  // Now an attempt to open the app should display the "missing or damaged"
  // page.
  {
    auto* new_app_browser = LaunchWebAppBrowserAndWait(app_id);
    content::WebContents* web_contents =
        new_app_browser->tab_strip_model()->GetActiveWebContents();

    EXPECT_THAT(EvalJs(web_contents, "document.body.innerText").ExtractString(),
                HasSubstr("This application is missing or damaged"));
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerWithKeyRotationBrowserTest,
                       PolicyReprocessOnComponentUpdate) {
  base::HistogramTester ht;

  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_);
  auto app_id = url_info.app_id();

  {
    // Add a bundle with version 1.0.0 signed by a rotated key and attempt to
    // install it; this installation will fail.
    AddBundleSignedBy(test::GetDefaultEcdsaP256KeyPair());

    base::test::TestFuture<web_package::SignedWebBundleId, IwaInstallerResult>
        future;
    IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
        future.GetRepeatingCallback());

    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            update_server_mixin_.CreateForceInstallPolicyEntry(
                web_bundle_id_)));

    auto [web_bundle_id, result] = future.Take();
    EXPECT_EQ(web_bundle_id, web_bundle_id_);
    EXPECT_EQ(result.type(),
              IwaInstallerResultType::kErrorCantInstallFromWebBundle);

    EXPECT_FALSE(GetIsolatedWebApp(app_id));

    IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
        base::NullCallback());
  }

  auto waiter = web_app::WebAppTestInstallObserver(browser()->profile());
  waiter.BeginListening({app_id});

  // Key rotation should trigger a policy reprocess.
  EXPECT_THAT(
      test::InstallIwaKeyDistributionComponent(
          base::Version("0.1.0"), test::GetDefaultEd25519WebBundleId().id(),
          test::GetDefaultEcdsaP256KeyPair().public_key.bytes()),
      HasValue());

  waiter.Wait();

  // Now the app should be installed.
  EXPECT_THAT(
      GetIsolatedWebApp(app_id),
      test::IwaIs(Eq("app-1.0.0"),
                  test::IsolationDataIs(
                      /*location=*/_, Eq(base::Version("1.0.0")),
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/Eq(std::nullopt),
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(
                          test::GetDefaultEcdsaP256KeyPair().public_key))));
}

}  // namespace
}  // namespace web_app
