// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include <optional>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {
namespace {

using base::test::HasValue;
using ::testing::_;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::VariantWith;

inline constexpr uint8_t kTestPublicKey[] = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

inline constexpr uint8_t kTestPrivateKey[] = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

constexpr std::string_view kUpdateManifestFileName = "update_manifest.json";
constexpr std::string_view kBundle304FileName = "bundle304.swbn";
constexpr std::string_view kBundle706FileName = "bundle706.swbn";

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
  IsolatedWebAppUpdateManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kIsolatedWebAppAutomaticUpdates);
  }

  void AddUpdate() {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;

    base::FilePath bundle_706_path = temp_dir_.Append(kBundle706FileName);
    bundle_706_ =
        IsolatedWebAppBuilder(
            ManifestBuilder().SetName("app-7.0.6").SetVersion("7.0.6"))
            .AddHtml("/", R"(
                <head>
                  <title>7.0.6</title>
                </head>
                <body>
                  <h1>Hello from version 7.0.6</h1>
                </body>
            )")
            .BuildBundle(bundle_706_path, key_pair_);

    EXPECT_TRUE(base::WriteFile(
        temp_dir_.Append(kUpdateManifestFileName),
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

 protected:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    SetUpFilesAndServer();
    AddTrustedWebBundleIdForTesting(url_info_->web_bundle_id());
  }

  void SetUpFilesAndServer() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // We cannot use `ScopedTempDir` here because the directory must survive
    // restarts for the `PRE_` tests to work. Use a directory within the profile
    // directory instead.
    temp_dir_ = profile()->GetPath().AppendASCII("iwa-temp-for-testing");
    EXPECT_TRUE(base::CreateDirectory(temp_dir_));
    iwa_server_.ServeFilesFromDirectory(temp_dir_);
    EXPECT_TRUE(iwa_server_.Start());

    auto key_pair = web_package::WebBundleSigner::Ed25519KeyPair(
        kTestPublicKey, kTestPrivateKey);
    auto bundle_id = web_package::SignedWebBundleId::CreateForEd25519PublicKey(
        key_pair_.public_key);
    url_info_ = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);

    auto builder = IsolatedWebAppBuilder(
        ManifestBuilder().SetName("app-3.0.4").SetVersion("3.0.4"));
    builder.AddHtml("/", R"(
        <head>
          <script type="text/javascript" src="/register-sw.js"></script>
          <title>3.0.4</title>
        </head>
        <body>
          <h1>Hello from version 3.0.4</h1>
        </body>)");
    builder.AddJs("/register-sw.js", R"(
        window.trustedTypes.createPolicy('default', {
          createHTML: (html) => html,
          createScriptURL: (url) => url,
          createScript: (script) => script,
        });
        if (location.search.includes('register-sw=1')) {
          navigator.serviceWorker.register("/sw.js");
        }
      )");
    builder.AddJs("/sw.js", R"(
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
    base::FilePath bundle_304_path = temp_dir_.Append(kBundle304FileName);
    bundle_304_ = builder.BuildBundle(bundle_304_path, key_pair_);

    EXPECT_TRUE(base::WriteFile(
        temp_dir_.Append(kUpdateManifestFileName),
        base::ReplaceStringPlaceholders(
            R"(
              {
                "versions": [
                  {"version": "3.0.4", "src": "$1"}
                ]
              }
            )",
            {iwa_server_.GetURL(base::StrCat({"/", kBundle304FileName}))
                 .spec()},
            /*offsets=*/nullptr)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<IsolatedWebAppUrlInfo> url_info_;
  base::FilePath temp_dir_;
  net::EmbeddedTestServer iwa_server_;
  std::unique_ptr<BundledIsolatedWebApp> bundle_304_;
  std::unique_ptr<BundledIsolatedWebApp> bundle_706_;
  web_package::WebBundleSigner::Ed25519KeyPair key_pair_ =
      web_package::WebBundleSigner::Ed25519KeyPair(kTestPublicKey,
                                                   kTestPrivateKey);
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest, Succeeds) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(kPolicyWebBundleIdKey, url_info_->web_bundle_id().id())
              .Set(kPolicyUpdateManifestUrlKey,
                   iwa_server_
                       .GetURL(base::StrCat({"/", kUpdateManifestFileName}))
                       .spec())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({url_info_->app_id()});

  AddUpdate();
  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({url_info_->app_id()});

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  manifest_updated_observer.Wait();
  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info_->app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt))));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       SucceedsWithServiceWorkerWithFetchHandler) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(kPolicyWebBundleIdKey, url_info_->web_bundle_id().id())
              .Set(kPolicyUpdateManifestUrlKey,
                   iwa_server_
                       .GetURL(base::StrCat({"/", kUpdateManifestFileName}))
                       .spec())));

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({url_info_->app_id()});
  AddUpdate();

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListening({url_info_->app_id()});

  // Open the app, which will register the Service Worker.
  content::RenderFrameHost* app_frame =
      OpenApp(url_info_->app_id(), "?register-sw=1");
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(url_info_->app_id()),
              Eq(1ul));

  // Wait for the Service Worker to start running.
  content::StoragePartition* storage_partition =
      app_frame->GetStoragePartition();
  ServiceWorkerVersionStartedRunningWaiter waiter(storage_partition);
  waiter.AwaitStartedRunning();
  test::CheckServiceWorkerStatus(
      url_info_->origin().GetURL(), storage_partition,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  // Updates will be applied once the app's window is closed.
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(url_info_->app_id()),
              Eq(0ul));

  manifest_updated_observer.Wait();
  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info_->app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt))));
}

// TODO(crbug.com/40929933): Session restore does not restore app windows on
// Lacros. Forcing the IWA to open via the `--app-id` command line switch is
// also not viable, because `WebAppBrowserTestBase` expects a `browser()`
// to open before the `WebAppProvider` is ready.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       PRE_AppliesUpdateOnStartupIfAppWindowNeverCloses) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(kPolicyWebBundleIdKey, url_info_->web_bundle_id().id())
              .Set(kPolicyUpdateManifestUrlKey,
                   iwa_server_
                       .GetURL(base::StrCat({"/", kUpdateManifestFileName}))
                       .spec())));

  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile(), pref);

  profile()->GetPrefs()->CommitPendingWrite();

  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({url_info_->app_id()});

  // Open the app to prevent the update from being applied.
  OpenApp(url_info_->app_id());
  EXPECT_THAT(provider().ui_manager().GetNumWindowsForApp(url_info_->app_id()),
              Eq(1ul));

  AddUpdate();
  EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));

  ASSERT_TRUE(base::test::RunUntil([this]() {
    const WebApp* app =
        provider().registrar_unsafe().GetAppById(url_info_->app_id());
    return app->isolation_data()->pending_update_info().has_value();
  }));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest,
                       AppliesUpdateOnStartupIfAppWindowNeverCloses) {
  // Wait for the update to be applied if it hasn't already.
  const WebApp* app =
      provider().registrar_unsafe().GetAppById(url_info_->app_id());

  if (app->isolation_data()->version != base::Version("7.0.6")) {
    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({url_info_->app_id()});
    manifest_updated_observer.Wait();
  }

  EXPECT_THAT(provider().registrar_unsafe().GetAppById(url_info_->app_id()),
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt))));

  Browser* app_window =
      AppBrowserController::FindForWebApp(*profile(), url_info_->app_id());
  ASSERT_THAT(app_window, NotNull());
  content::WebContents* web_contents =
      app_window->tab_strip_model()->GetActiveWebContents();

  content::TitleWatcher title_watcher(web_contents, u"7.0.6");
  title_watcher.AlsoWaitForTitle(u"3.0.4");
  EXPECT_THAT(title_watcher.WaitAndGetTitle(), Eq(u"7.0.6"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
}  // namespace web_app
