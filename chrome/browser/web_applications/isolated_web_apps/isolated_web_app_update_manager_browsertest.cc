// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {
namespace {

using base::test::HasValue;
using ::testing::_;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::VariantWith;

constexpr base::StringPiece kUpdateManifestFileName = "update_manifest.json";
constexpr base::StringPiece kBundle304FileName = "bundle304.swbn";
constexpr base::StringPiece kBundle706FileName = "bundle706.swbn";

constexpr base::StringPiece kTestManifest = R"({
      "name": "$1",
      "version": "$2",
      "id": "/",
      "scope": "/",
      "start_url": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "256x256-green.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ]
    })";

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

  void AwaitStartedRunning() { future_.Wait(); }

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
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kIsolatedWebAppAutomaticUpdates);
    SetTrustedWebBundleIdsForTesting({url_info_.web_bundle_id()});
    SetUpFilesAndServer();

    IsolatedWebAppBrowserTestHarness::SetUp();
  }

  void SetUpFilesAndServer() {
    auto key_pair =
        web_package::WebBundleSigner::KeyPair(kTestPublicKey, kTestPrivateKey);

    TestSignedWebBundleBuilder builder(key_pair);
    builder.AddManifest(base::ReplaceStringPlaceholders(
        kTestManifest, {"app-3.0.4", base::Version("3.0.4").GetString()},
        /*offsets=*/nullptr));
    builder.AddPngImage(
        "/256x256-green.png",
        test::BitmapAsPng(CreateSquareIcon(256, SK_ColorGREEN)));
    builder.AddHtml("/", R"(
      <head>
        <link rel="manifest" href="/manifest.webmanifest">
        <script type="text/javascript" src="/register-sw.js"></script>
      </head>
      <body>
        <h1>Hello from version 3.0.4</h1>
      </body>
    )");
    builder.AddJavaScript("/register-sw.js", R"(
      window.trustedTypes.createPolicy('default', {
        createHTML: (html) => html,
        createScriptURL: (url) => url,
        createScript: (script) => script,
      });
      navigator.serviceWorker.register("/sw.js");
    )");
    builder.AddJavaScript("/sw.js", R"(
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
    )");
    TestSignedWebBundle bundle304 = builder.Build();

    TestSignedWebBundle bundle706 = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions()
            .SetKeyPair(key_pair)
            .SetAppName("app-7.0.6")
            .SetVersion(base::Version("7.0.6")));

    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    iwa_server_.ServeFilesFromDirectory(temp_dir_.GetPath());
    EXPECT_TRUE(iwa_server_.Start());

    EXPECT_TRUE(base::WriteFile(temp_dir_.GetPath().Append(kBundle304FileName),
                                bundle304.data));
    EXPECT_TRUE(base::WriteFile(temp_dir_.GetPath().Append(kBundle706FileName),
                                bundle706.data));
    EXPECT_TRUE(base::WriteFile(
        temp_dir_.GetPath().Append(kUpdateManifestFileName),
        base::ReplaceStringPlaceholders(
            R"(
              {
                "versions": [
                  {"version": "3.0.4", "src": "$1"},
                  {"version": "7.0.6", "src": "$2"}
                ]
              }
            )",
            {iwa_server_.GetURL(base::StrCat({"/", kBundle304FileName})).spec(),
             iwa_server_.GetURL(base::StrCat({"/", kBundle706FileName}))
                 .spec()},
            /*offsets=*/nullptr)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  IsolatedWebAppUrlInfo url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId));
  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer iwa_server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest, Succeeds) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(kPolicyWebBundleIdKey, url_info_.web_bundle_id().id())
              .Set(kPolicyUpdateManifestUrlKey,
                   iwa_server_
                       .GetURL(base::StrCat({"/", kUpdateManifestFileName}))
                       .spec())));

  {
    base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>>
        future;
    provider().scheduler().InstallIsolatedWebApp(
        url_info_,
        InstalledBundle{.path = temp_dir_.GetPath().Append(kBundle304FileName)},
        base::Version("3.0.4"), /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    EXPECT_THAT(future.Take(), HasValue());
  }

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({url_info_.app_id()});

  // No ServiceWorker should have been registered since we never opened
  // `isolated-app://.../`.
  test::CheckServiceWorkerStatus(
      url_info_.origin().GetURL(),
      profile()->GetStoragePartition(
          url_info_.storage_partition_config(profile()), /*can_create=*/false),
      content::ServiceWorkerCapability::NO_SERVICE_WORKER);

  provider().iwa_update_manager().DiscoverUpdatesNowForTesting();

  manifest_updated_observer.Wait();
  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              VariantWith<InstalledBundle>(_),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(absl::nullopt))));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       SucceedsWithServiceWorkerWithFetchHandler) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(kPolicyWebBundleIdKey, url_info_.web_bundle_id().id())
              .Set(kPolicyUpdateManifestUrlKey,
                   iwa_server_
                       .GetURL(base::StrCat({"/", kUpdateManifestFileName}))
                       .spec())));

  {
    base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>>
        future;
    provider().scheduler().InstallIsolatedWebApp(
        url_info_,
        InstalledBundle{.path = temp_dir_.GetPath().Append(kBundle304FileName)},
        base::Version("3.0.4"), /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    EXPECT_THAT(future.Take(), HasValue());
  }

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({url_info_.app_id()});

  // Open the app, which will register the Service Worker.
  content::RenderFrameHost* app_frame = OpenApp(url_info_.app_id());
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(url_info_.app_id()),
              Eq(1ul));

  // Wait for the Service Worker to start running.
  content::StoragePartition* storage_partition =
      app_frame->GetStoragePartition();
  ServiceWorkerVersionStartedRunningWaiter waiter(storage_partition);
  waiter.AwaitStartedRunning();
  test::CheckServiceWorkerStatus(
      url_info_.origin().GetURL(), storage_partition,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  provider().iwa_update_manager().DiscoverUpdatesNowForTesting();

  // Updates will be applied once the app's window is closed.
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(url_info_.app_id()),
              Eq(0ul));

  manifest_updated_observer.Wait();
  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              VariantWith<InstalledBundle>(_),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(absl::nullopt))));
}

}  // namespace
}  // namespace web_app
