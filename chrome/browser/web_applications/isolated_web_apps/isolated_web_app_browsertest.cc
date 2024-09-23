// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <optional>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/push_messaging/push_messaging_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/result_catcher.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"

namespace web_app {

namespace {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Ne;
using ::testing::StartsWith;

const char kNonAppHost[] = "nonapp.com";

const int32_t kApplicationServerKeyLength = 65;
// NIST P-256 public key made available to tests. Must be an uncompressed
// point in accordance with SEC1 2.3.3.
const uint8_t kApplicationServerKey[kApplicationServerKeyLength] = {
    0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36,
    0x10, 0xC1, 0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48,
    0xC9, 0xC6, 0xBB, 0xBF, 0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B,
    0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52, 0x21, 0xD3, 0x71, 0x90, 0x13,
    0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1, 0x7F, 0xF2, 0x76,
    0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xFD};

std::string GetTestApplicationServerKey() {
  std::string application_server_key(
      kApplicationServerKey,
      kApplicationServerKey + std::size(kApplicationServerKey));

  return application_server_key;
}

class BaseServiceWorkerVersionWaiter
    : public content::ServiceWorkerContextObserver {
 public:
  explicit BaseServiceWorkerVersionWaiter(
      content::StoragePartition* storage_partition) {
    DCHECK(storage_partition);

    service_worker_context_ = storage_partition->GetServiceWorkerContext();
    service_worker_context_->AddObserver(this);
  }

  BaseServiceWorkerVersionWaiter(const BaseServiceWorkerVersionWaiter&) =
      delete;
  BaseServiceWorkerVersionWaiter& operator=(
      const BaseServiceWorkerVersionWaiter&) = delete;

  ~BaseServiceWorkerVersionWaiter() override {
    if (service_worker_context_) {
      service_worker_context_->RemoveObserver(this);
    }
  }

 protected:
  raw_ptr<content::ServiceWorkerContext> service_worker_context_ = nullptr;

 private:
  void OnDestruct(content::ServiceWorkerContext* context) override {
    service_worker_context_->RemoveObserver(this);
    service_worker_context_ = nullptr;
  }
};

class ServiceWorkerVersionActivatedWaiter
    : public BaseServiceWorkerVersionWaiter {
 public:
  ServiceWorkerVersionActivatedWaiter(
      content::StoragePartition* storage_partition,
      const GURL& url)
      : BaseServiceWorkerVersionWaiter(storage_partition), url_(url) {}

  int64_t AwaitVersionActivated() { return future.Get(); }

 private:
  // content::ServiceWorkerContextObserver:
  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    if (content::ServiceWorkerContext::ScopeMatches(scope, url_)) {
      future.SetValue(version_id);
    }
  }

  GURL url_;
  base::test::TestFuture<int64_t> future;
};

class ServiceWorkerVersionStartedRunningWaiter
    : public BaseServiceWorkerVersionWaiter {
 public:
  ServiceWorkerVersionStartedRunningWaiter(
      content::StoragePartition* storage_partition,
      int64_t version_id)
      : BaseServiceWorkerVersionWaiter(storage_partition),
        version_id_(version_id) {}

  void AwaitVersionStartedRunning() { run_loop_.Run(); }

 private:
  // content::ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (version_id == version_id_) {
      run_loop_.Quit();
    }
  }

  const int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;
  base::RunLoop run_loop_;
};

class ServiceWorkerVersionStoppedRunningWaiter
    : public BaseServiceWorkerVersionWaiter {
 public:
  ServiceWorkerVersionStoppedRunningWaiter(
      content::StoragePartition* storage_partition,
      int64_t version_id)
      : BaseServiceWorkerVersionWaiter(storage_partition),
        version_id_(version_id) {}

  void AwaitVersionStoppedRunning() { run_loop_.Run(); }

 private:
  // content::ServiceWorkerContextObserver:
  void OnVersionStoppedRunning(int64_t version_id) override {
    if (version_id == version_id_) {
      run_loop_.Quit();
    }
  }

  const int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;
  base::RunLoop run_loop_;
};

}  // namespace

class IsolatedWebAppBrowserTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  content::StoragePartition* default_storage_partition() {
    return browser()->profile()->GetDefaultStoragePartition();
  }

  content::RenderFrameHost* GetPrimaryMainFrame(Browser* browser) {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, DevProxyError) {
  std::unique_ptr<ScopedProxyIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddResource("/nonexistent", "", {{"Content-Type", "text/html"}},
                       net::HttpStatusCode::HTTP_NOT_FOUND)
          .BuildAndStartProxyServer();
  ASSERT_OK_AND_ASSIGN(auto url_info, app->Install(profile()));

  auto* app_frame = OpenApp(url_info.app_id());
  ASSERT_NE(nullptr, app_frame);

  content::TestNavigationObserver observer(
      content::WebContents::FromRenderFrameHost(app_frame));
  observer.StartWatchingNewWebContents();

  ASSERT_NE(ui_test_utils::NavigateToURL(
                GetBrowserFromFrame(app_frame),
                url_info.origin().GetURL().Resolve("/nonexistent")),
            nullptr);

  observer.WaitForNavigationFinished();
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(observer.last_net_error_code(),
            net::ERR_HTTP_RESPONSE_CODE_FAILURE);

  auto response_code = observer.last_http_response_code();
  ASSERT_TRUE(response_code);
  EXPECT_EQ(*response_code, net::HttpStatusCode::HTTP_NOT_FOUND);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, AppsPartitioned) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app1 =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info1,
                       app1->TrustBundleAndInstall(profile()));

  std::unique_ptr<ScopedBundledIsolatedWebApp> app2 =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info2,
                       app2->TrustBundleAndInstall(profile()));

  auto* non_app_frame = ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/simple.html"));
  EXPECT_TRUE(non_app_frame);
  EXPECT_EQ(default_storage_partition(), non_app_frame->GetStoragePartition());

  auto* app_frame = OpenApp(url_info1.app_id());
  EXPECT_NE(default_storage_partition(), app_frame->GetStoragePartition());

  auto* app2_frame = OpenApp(url_info2.app_id());
  EXPECT_NE(default_storage_partition(), app2_frame->GetStoragePartition());

  EXPECT_NE(app_frame->GetStoragePartition(),
            app2_frame->GetStoragePartition());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest,
                       OmniboxNavigationOpensNewPwaWindow) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddFileFromDisk("/index.html",
                           "web_apps/simple_isolated_app/index.html")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));

  GURL app_url = url_info.origin().GetURL().Resolve("/index.html");
  auto* app_frame =
      NavigateToURLInNewTab(browser(), app_url, WindowOpenDisposition::UNKNOWN);

  // The browser shouldn't have opened the app's page.
  EXPECT_EQ(GetPrimaryMainFrame(browser())->GetLastCommittedURL(), GURL());

  // The app's frame should belong to an isolated PWA browser window.
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(
      AppBrowserController::IsForWebApp(app_browser, url_info.app_id()));
  EXPECT_EQ(content::WebExposedIsolationLevel::kIsolatedApplication,
            app_frame->GetWebExposedIsolationLevel());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, SameOriginWindowOpen) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/popup", "<!DOCTYPE html><body>popup page</body>")
          .BuildBundle();
  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(auto url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL expected_url = url_info.origin().GetURL().Resolve("/popup");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  BrowserWaiter browser_waiter(nullptr);
  ASSERT_TRUE(ExecJs(app_frame, "window.open('/popup')"));
  Browser* popup = browser_waiter.AwaitAdded(FROM_HERE);
  navigation_observer.WaitForNavigationFinished();

  ASSERT_NE(popup, nullptr);
  content::RenderFrameHost* popup_frame = GetPrimaryMainFrame(popup);
  EXPECT_EQ(popup_frame->GetLastCommittedURL(), expected_url);
  EXPECT_EQ(EvalJs(popup_frame, "document.body.innerText"), "popup page");
  EXPECT_EQ(EvalJs(popup_frame, "window.opener !== null"), true);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, CrossOriginWindowOpen) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL expected_url = https_server()->GetURL("/simple.html");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  ASSERT_TRUE(
      ExecJs(app_frame, content::JsReplace("window.open($1)", expected_url)));
  content::WebContents* popup_contents = tab_waiter.Wait();
  navigation_observer.WaitForNavigationFinished();

  ASSERT_NE(popup_contents, nullptr);
  content::RenderFrameHost* popup_frame = popup_contents->GetPrimaryMainFrame();
  EXPECT_EQ(popup_frame->GetLastCommittedURL(), expected_url);
  EXPECT_EQ(EvalJs(popup_frame, "window.opener === null"), true);
}

// TODO(b/366524200): Find out why the navigation isn't opening in an IWA window
IN_PROC_BROWSER_TEST_F(
    IsolatedWebAppBrowserTest,
    DISABLED_OmniboxNavigationOpensNewPwaWindowEvenIfUserDisplayModeIsBrowser) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddFileFromDisk("/", "web_apps/simple_isolated_app/index.html")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));

  WebAppProvider::GetForTest(browser()->profile())
      ->sync_bridge_unsafe()
      .SetAppUserDisplayModeForTesting(url_info.app_id(),
                                       mojom::UserDisplayMode::kBrowser);

  GURL app_url = url_info.origin().GetURL().Resolve("/index.html");
  auto* app_frame =
      NavigateToURLInNewTab(browser(), app_url, WindowOpenDisposition::UNKNOWN);

  // The browser shouldn't have opened the app's page.
  EXPECT_EQ(GetPrimaryMainFrame(browser())->GetLastCommittedURL(), GURL());

  // The app's frame should belong to an isolated PWA browser window.
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(
      AppBrowserController::IsForWebApp(app_browser, url_info.app_id()));
  EXPECT_FALSE(app_browser->app_controller()->HasMinimalUiButtons());
  EXPECT_EQ(content::WebExposedIsolationLevel::kIsolatedApplication,
            app_frame->GetWebExposedIsolationLevel());
}

// Tests that the app menu doesn't have an 'Open in Chrome' option.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, NoOpenInChrome) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  Browser* app_browser = GetBrowserFromFrame(app_frame);

  EXPECT_FALSE(
      app_browser->command_controller()->IsCommandEnabled(IDC_OPEN_IN_CHROME));

  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, app_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  size_t index = 0;
  const bool found = app_menu_model->GetModelAndIndexForCommandId(
      IDC_OPEN_IN_CHROME, &model, &index);
  EXPECT_TRUE(found);
  EXPECT_FALSE(model->IsVisibleAt(index));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, WasmLoadableFromFile) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddFileFromDisk("/empty.wasm",
                           "web_apps/simple_isolated_app/empty.wasm")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  content::EvalJsResult result = EvalJs(app_frame, R"(
    (async function() {
      const response = await fetch('empty.wasm');
      await WebAssembly.instantiateStreaming(response);
      return 'loaded';
    })();
  )");

  EXPECT_EQ("loaded", result);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, WasmLoadableFromBytes) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  content::EvalJsResult result = EvalJs(app_frame, R"(
    (async function() {
      // The smallest possible Wasm module. Just the header (0, "A", "S", "M"),
      // and the version (0x1).
      const bytes = new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0]);
      await WebAssembly.instantiate(bytes);
      return 'loaded';
    })();
  )");

  EXPECT_EQ("loaded", result);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, CanNavigateToBlobUrl) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  content::TestNavigationObserver navigation_observer(
      content::WebContents::FromRenderFrameHost(app_frame));
  EXPECT_TRUE(ExecJs(app_frame,
                     "const blob = new Blob(['test'], {type : 'text/plain'});"
                     "location.href = window.URL.createObjectURL(blob)"));
  navigation_observer.Wait();

  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer.last_net_error_code(), Eq(net::OK));
  EXPECT_THAT(navigation_observer.last_navigation_url().spec(),
              StartsWith("blob:" + url_info.origin().GetURL().spec()));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, WebCannotLoadIwaResources) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     https_server()->GetURL("/simple.html")));

  EXPECT_THAT(
      EvalJs(web_contents, content::JsReplace(R"(
    (async () => {
      const response = await fetch($1);
      return response.ok;
    })();
  )",
                                              url_info.origin().Serialize()))
          .error,
      HasSubstr("Failed to fetch"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest,
                       IwaCannotLoadOtherIwaResources) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app1 =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info1,
                       app1->TrustBundleAndInstall(profile()));

  std::unique_ptr<ScopedBundledIsolatedWebApp> app2 =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info2,
                       app2->TrustBundleAndInstall(profile()));

  content::RenderFrameHost* app1_frame = OpenApp(url_info1.app_id());
  content::TestNavigationObserver navigation_observer(
      content::WebContents::FromRenderFrameHost(app1_frame));
  EXPECT_TRUE(
      ExecJs(app1_frame, content::JsReplace(R"(
    const iframe = document.createElement('iframe');
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                            url_info2.origin().Serialize())));
  navigation_observer.Wait();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_THAT(navigation_observer.last_net_error_code(),
              Eq(net::ERR_BLOCKED_BY_CSP));

  EXPECT_THAT(
      EvalJs(app1_frame, content::JsReplace(R"(
    (async () => {
      const response = await fetch($1 + '/icon.png');
      return response.ok;
    })();
  )",
                                            url_info2.origin().Serialize()))
          .error,
      HasSubstr("Failed to fetch"));
}

class IsolatedWebAppApiAccessBrowserTest : public IsolatedWebAppBrowserTest {
 protected:
  IsolatedWebAppApiAccessBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kIsolateSandboxedIframes,
                                    blink::features::kDirectSockets},
                                   {});
  }

  IsolatedWebAppUrlInfo InstallAppWithSocketPermission() {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(
            ManifestBuilder().AddPermissionsPolicy(
                blink::mojom::PermissionsPolicyFeature::kDirectSockets,
                /*self=*/true, {}))
            .AddJs("/csp_violation_handler.js", R"(
              console.log('In bundled script');
              window.addEventListener('securitypolicyviolation', (e) => {
                window.cspViolation = e;
              });
              window.ranBundledScript = true;
            )")
            .BuildBundle();
    app->TrustSigningKey();
    return app->InstallChecked(profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppApiAccessBrowserTest,
                       NoApiAccessInDataIframe) {
  IsolatedWebAppUrlInfo url_info = InstallAppWithSocketPermission();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_THAT(EvalJs(app_frame, "'TCPSocket' in window"), Eq(true));

  ASSERT_TRUE(ExecJs(app_frame, R"(
      const src = '<!DOCTYPE html><p>data: URL</p>';
      const url = `data:text/html;base64,${btoa(src)}`;
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = url;
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )"));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 0);
  ASSERT_THAT(iframe, Ne(nullptr));

  EXPECT_THAT(
      EvalJs(iframe, "location.href"),
      Eq("data:text/html;base64,PCFET0NUWVBFIGh0bWw+PHA+ZGF0YTogVVJMPC9wPg=="));
  EXPECT_THAT(EvalJs(iframe, "window.origin"), Eq("null"));
  EXPECT_THAT(EvalJs(iframe, "window.isSecureContext"), Eq(false));
  EXPECT_THAT(EvalJs(iframe, "window.crossOriginIsolated"), Eq(false));
  EXPECT_THAT(EvalJs(iframe, "'TCPSocket' in window"), Eq(false));
  EXPECT_THAT(
      iframe->GetLastCommittedURL(),
      Eq("data:text/html;base64,PCFET0NUWVBFIGh0bWw+PHA+ZGF0YTogVVJMPC9wPg=="));
  EXPECT_THAT(iframe->GetLastCommittedOrigin().opaque(), Eq(true));
  EXPECT_THAT(iframe->GetWebExposedIsolationLevel(),
              Eq(content::WebExposedIsolationLevel::kNotIsolated));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppApiAccessBrowserTest,
                       NoApiAccessInSandboxedIframe) {
  IsolatedWebAppUrlInfo url_info = InstallAppWithSocketPermission();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_THAT(EvalJs(app_frame, "'TCPSocket' in window"), Eq(true));

  std::string start_url = url_info.origin().GetURL().spec();
  ASSERT_TRUE(ExecJs(app_frame, content::JsReplace(R"(
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = $1;
        f.sandbox = 'allow-scripts';  // for EvalJs
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )",
                                                   start_url)));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 0);
  ASSERT_THAT(iframe, Ne(nullptr));

  EXPECT_THAT(EvalJs(iframe, "location.href"), Eq(start_url));
  EXPECT_THAT(EvalJs(iframe, "window.origin"), Eq("null"));
  EXPECT_THAT(EvalJs(iframe, "window.isSecureContext"), Eq(true));
  EXPECT_THAT(EvalJs(iframe, "window.crossOriginIsolated"), Eq(false));
  EXPECT_THAT(EvalJs(iframe, "'TCPSocket' in window"), Eq(false));
  EXPECT_THAT(iframe->GetProcess()->GetWebExposedIsolationLevel(),
              Eq(content::WebExposedIsolationLevel::kIsolated));
  EXPECT_THAT(iframe->GetLastCommittedURL(), Eq(start_url));
  EXPECT_THAT(iframe->GetLastCommittedOrigin().opaque(), Eq(true));
  EXPECT_THAT(iframe->GetWebExposedIsolationLevel(),
              Eq(content::WebExposedIsolationLevel::kNotIsolated));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppApiAccessBrowserTest,
                       CspInheritedInSrcdocIframe) {
  IsolatedWebAppUrlInfo url_info = InstallAppWithSocketPermission();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  // Create a srcdoc iframe with an inline <script> tag that should
  // be blocked by the inherited CSP.
  ASSERT_TRUE(ExecJs(app_frame, R"(
      const noopPolicy = trustedTypes.createPolicy("policy", {
        createHTML: (string) => string,
      });
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.srcdoc = noopPolicy.createHTML(`
            <!DOCTYPE html>
            <p>srcdoc iframe</p>
            <script src="/csp_violation_handler.js"></script>
            <script>window.ranScript = true;</script>
        `);
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )"));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 0);
  ASSERT_THAT(iframe, Ne(nullptr));

  EXPECT_THAT(EvalJs(iframe, "location.href"), Eq("about:srcdoc"));
  EXPECT_THAT(EvalJs(iframe, "window.origin"),
              Eq(url_info.origin().Serialize()));
  EXPECT_THAT(EvalJs(iframe, "window.isSecureContext"), Eq(true));
  EXPECT_THAT(EvalJs(iframe, "window.crossOriginIsolated"), Eq(true));
  EXPECT_THAT(iframe->GetLastCommittedURL(), Eq("about:srcdoc"));
  EXPECT_THAT(iframe->GetLastCommittedOrigin(), Eq(url_info.origin()));
  // Non-sandboxed srcdoc iframes are same-origin with their parent, meaning
  // they also have application isolation level (i.e. are IsolatedContexts).
  // This is safe because they also inherit the strict CSP.
  EXPECT_THAT(iframe->GetWebExposedIsolationLevel(),
              Eq(content::WebExposedIsolationLevel::kIsolatedApplication));
  EXPECT_THAT(EvalJs(iframe, "String(window.ranScript)"), Eq("undefined"));
  EXPECT_THAT(EvalJs(iframe, "String(window.ranBundledScript)"), Eq("true"));
  EXPECT_THAT(
      EvalJs(iframe,
             "window.cspViolation instanceof SecurityPolicyViolationEvent"),
      Eq(true));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppApiAccessBrowserTest,
                       CspInheritedInBlobIframe) {
  IsolatedWebAppUrlInfo url_info = InstallAppWithSocketPermission();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(
      ExecJs(app_frame, content::JsReplace(R"(
          const blobSource = `
              <!DOCTYPE html>
              <p>blob html page</p>
              <script src=$1></script>
              <script>window.ranScript = true;</script>
          `;
          const blob = new Blob([blobSource], {
            type: 'text/html'
          });
          const url = URL.createObjectURL(blob);
          new Promise(resolve => {
            const f = document.createElement('iframe');
            f.src = url;
            f.addEventListener('load', resolve);
            document.body.appendChild(f);
          });
      )",
                                           url_info.origin().GetURL().Resolve(
                                               "/csp_violation_handler.js"))));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 0);
  ASSERT_THAT(iframe, Ne(nullptr));

  EXPECT_THAT(EvalJs(iframe, "location.href").ExtractString(),
              StartsWith("blob:"));
  EXPECT_THAT(EvalJs(iframe, "window.origin"),
              Eq(url_info.origin().Serialize()));
  EXPECT_THAT(EvalJs(iframe, "window.isSecureContext"), Eq(true));
  EXPECT_THAT(EvalJs(iframe, "window.crossOriginIsolated"), Eq(true));
  EXPECT_THAT(iframe->GetLastCommittedURL().SchemeIsBlob(), Eq(true));
  EXPECT_THAT(iframe->GetLastCommittedOrigin(), Eq(url_info.origin()));
  EXPECT_THAT(iframe->GetWebExposedIsolationLevel(),
              Eq(content::WebExposedIsolationLevel::kIsolatedApplication));
  EXPECT_THAT(EvalJs(iframe, "String(window.ranScript)"), Eq("undefined"));
  EXPECT_THAT(EvalJs(iframe, "String(window.ranBundledScript)"), Eq("true"));
  EXPECT_THAT(
      EvalJs(iframe,
             "window.cspViolation instanceof SecurityPolicyViolationEvent"),
      Eq(true));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppApiAccessBrowserTest,
                       CspInheritedInBlobNavigation) {
  IsolatedWebAppUrlInfo url_info = InstallAppWithSocketPermission();
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  content::WebContents* app_contents =
      content::WebContents::FromRenderFrameHost(app_frame);
  content::TestNavigationObserver navigation_observer(app_contents);
  ASSERT_TRUE(
      ExecJs(app_frame, content::JsReplace(R"(
          const blobSource = `
              <!DOCTYPE html>
              <p>blob html page</p>
              <script src=$1></script>
              <script>window.ranScript = true;</script>
          `;
          const blob = new Blob([blobSource], {
            type: 'text/html'
          });
          location.href = URL.createObjectURL(blob);
      )",
                                           url_info.origin().GetURL().Resolve(
                                               "/csp_violation_handler.js"))));
  navigation_observer.Wait();

  EXPECT_THAT(navigation_observer.last_navigation_succeeded(), Eq(true));
  EXPECT_THAT(navigation_observer.last_net_error_code(), Eq(net::OK));
  EXPECT_THAT(navigation_observer.last_navigation_url().spec(),
              StartsWith("blob:" + url_info.origin().GetURL().spec()));
  app_frame = app_contents->GetPrimaryMainFrame();
  EXPECT_THAT(EvalJs(app_frame, "location.href").ExtractString(),
              StartsWith("blob:"));
  EXPECT_THAT(EvalJs(app_frame, "window.origin"),
              Eq(url_info.origin().Serialize()));
  EXPECT_THAT(EvalJs(app_frame, "window.isSecureContext"), Eq(true));
  EXPECT_THAT(EvalJs(app_frame, "window.crossOriginIsolated"), Eq(true));
  EXPECT_THAT(app_frame->GetLastCommittedURL().SchemeIsBlob(), Eq(true));
  EXPECT_THAT(app_frame->GetLastCommittedOrigin(), Eq(url_info.origin()));
  EXPECT_THAT(app_frame->GetWebExposedIsolationLevel(),
              Eq(content::WebExposedIsolationLevel::kIsolatedApplication));
  EXPECT_THAT(EvalJs(app_frame, "String(window.ranScript)"), Eq("undefined"));
  EXPECT_THAT(EvalJs(app_frame, "String(window.ranBundledScript)"), Eq("true"));
  EXPECT_THAT(
      EvalJs(app_frame,
             "window.cspViolation instanceof SecurityPolicyViolationEvent"),
      Eq(true));
}

class IsolatedWebAppBrowserCookieTest : public IsolatedWebAppBrowserTest {
 public:
  using CookieHeaders = std::vector<std::string>;

  void SetUpOnMainThread() override {
    https_server()->RegisterRequestMonitor(
        base::BindRepeating(&IsolatedWebAppBrowserCookieTest::MonitorRequest,
                            base::Unretained(this)));

    base::FilePath isolated_web_app_dev_server_root =
        GetChromeTestDataDir().Append(
            FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
    isolated_web_app_dev_server_ = std::make_unique<net::EmbeddedTestServer>();
    isolated_web_app_dev_server_->AddDefaultHandlers(
        isolated_web_app_dev_server_root);
    isolated_web_app_dev_server_->RegisterRequestMonitor(
        base::BindRepeating(&IsolatedWebAppBrowserCookieTest::MonitorRequest,
                            base::Unretained(this)));
    CHECK(isolated_web_app_dev_server_->Start());

    IsolatedWebAppBrowserTest::SetUpOnMainThread();
  }

  void MonitorRequest(const net::test_server::HttpRequest& request) {
    // Replace the host in |request.GetURL()| with the value from the Host
    // header, as GetURL()'s host will be 127.0.0.1.
    std::string host = GURL("https://" + GetHeader(request, "Host")).host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    GURL url = request.GetURL().ReplaceComponents(replace_host);
    cookie_map_[url.spec()].push_back(GetHeader(request, "cookie"));
  }

 protected:
  // Returns the "Cookie" headers that were received for the given URL.
  const CookieHeaders& GetCookieHeadersForUrl(const GURL& url) {
    return cookie_map_[url.spec()];
  }

  const net::EmbeddedTestServer& isolated_web_app_dev_server() {
    return *isolated_web_app_dev_server_.get();
  }

 private:
  std::string GetHeader(const net::test_server::HttpRequest& request,
                        const std::string& header_name) {
    auto header = request.headers.find(header_name);
    return header != request.headers.end() ? header->second : "";
  }

  // Maps GURLs to a vector of cookie strings. The nth item in the vector will
  // contain the contents of the "Cookies" header for the nth request to the
  // given GURL.
  std::unordered_map<std::string, CookieHeaders> cookie_map_;
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserCookieTest, Cookies) {
  IsolatedWebAppUrlInfo url_info =
      InstallDevModeProxyIsolatedWebApp(url::Origin::Create(
          isolated_web_app_dev_server().GetURL("localhost", "/")));

  GURL app_url = url_info.origin().GetURL().Resolve("/cookie.html");
  GURL app_proxy_url =
      isolated_web_app_dev_server().GetURL("localhost", "/cookie.html");
  GURL non_app_url = https_server()->GetURL(
      kNonAppHost, "/web_apps/simple_isolated_app/cookie.html");
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(non_app_url, CONTENT_SETTING_ALLOW);

  // Load a page that sets a cookie, then create a cross-origin iframe that
  // loads the same page.
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  CreateIframe(app_frame, "child", non_app_url, "");

  const auto& app_cookies = GetCookieHeadersForUrl(app_proxy_url);
  EXPECT_EQ(1u, app_cookies.size());
  EXPECT_TRUE(app_cookies[0].empty());
  const auto& non_app_cookies = GetCookieHeadersForUrl(non_app_url);
  EXPECT_EQ(1u, non_app_cookies.size());
  EXPECT_TRUE(non_app_cookies[0].empty());

  // Load the pages again. The non-app page should send the cookie, but the
  // app won't because the proxy disables cookies (CredentialsMode::kOmit).
  content::RenderFrameHost* app_frame2 = OpenApp(url_info.app_id());
  Browser* app_browser2 = GetBrowserFromFrame(app_frame2);
  app_frame2 = ui_test_utils::NavigateToURL(app_browser2, app_url);
  CreateIframe(app_frame2, "child", non_app_url, "");

  EXPECT_EQ(2u, app_cookies.size());
  EXPECT_TRUE(app_cookies[1].empty());
  EXPECT_EQ(2u, non_app_cookies.size());
  EXPECT_EQ("foo=bar", non_app_cookies[1]);

  // Load the cross-origin's iframe as a top-level page. Because this page was
  // previously loaded in an isolated app, it shouldn't have cookies set when
  // loaded in a main frame here.
  ASSERT_TRUE(NavigateToURLInNewTab(browser(), non_app_url));

  EXPECT_EQ(3u, non_app_cookies.size());
  EXPECT_TRUE(non_app_cookies[2].empty());
}

class IsolatedWebAppBrowserServiceWorkerTest
    : public IsolatedWebAppBrowserTest {
 protected:
  int64_t InstallIsolatedWebAppAndWaitForServiceWorker() {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(ManifestBuilder())
            .AddHtml("/register_service_worker.html", "ABA")
            .AddFileFromDisk(
                "/register_service_worker.html",
                "web_apps/simple_isolated_app/register_service_worker.html")
            .AddFileFromDisk(
                "/register_service_worker.js",
                "web_apps/simple_isolated_app/register_service_worker.js")
            .AddFileFromDisk("/service_worker.js",
                             "web_apps/simple_isolated_app/service_worker.js")
            .BuildBundle();
    app->TrustSigningKey();
    IsolatedWebAppUrlInfo url_info = app->InstallChecked(profile());
    app_url_ = url_info.origin().GetURL();

    content::RenderFrameHost* original_frame = OpenApp(url_info.app_id());
    CHECK_NE(default_storage_partition(),
             original_frame->GetStoragePartition());

    app_web_contents_ =
        content::WebContents::FromRenderFrameHost(original_frame);
    app_window_ = GetBrowserFromFrame(original_frame);

    GURL register_service_worker_page =
        app_url_.Resolve("register_service_worker.html");

    app_frame_ =
        ui_test_utils::NavigateToURL(app_window_, register_service_worker_page);
    storage_partition_ = app_frame_->GetStoragePartition();
    CHECK_NE(default_storage_partition(), storage_partition_);

    ServiceWorkerVersionActivatedWaiter version_activated_waiter(
        storage_partition_, app_url_);

    return version_activated_waiter.AwaitVersionActivated();
  }

  const GURL& app_url() const { return app_url_; }

  raw_ptr<Browser, AcrossTasksDanglingUntriaged> app_window_ = nullptr;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      app_web_contents_ = nullptr;
  raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged> app_frame_ =
      nullptr;
  raw_ptr<content::StoragePartition, AcrossTasksDanglingUntriaged>
      storage_partition_ = nullptr;
  GURL app_url_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserServiceWorkerTest,
                       ServiceWorkerPartitioned) {
  InstallIsolatedWebAppAndWaitForServiceWorker();
  test::CheckServiceWorkerStatus(
      app_url(), storage_partition_,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

class IsolatedWebAppBrowserServiceWorkerPushTest
    : public IsolatedWebAppBrowserServiceWorkerTest {
 public:
  IsolatedWebAppBrowserServiceWorkerPushTest()
      : scoped_testing_factory_installer_(
            base::BindRepeating(&gcm::FakeGCMProfileService::Build)) {}

 protected:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserServiceWorkerTest::SetUpOnMainThread();

    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
  }

  void SendMessageAndWaitUntilHandled(
      content::BrowserContext* context,
      const PushMessagingAppIdentifier& app_identifier,
      const gcm::IncomingMessage& message) {
    PushMessagingServiceImpl* push_service =
        PushMessagingServiceFactory::GetForProfile(context);

    CHECK_EQ(push_service->GetPermissionStatus(app_url(),
                                               /*user_visible=*/true),
             blink::mojom::PermissionStatus::GRANTED);

    // If there is not enough budget, a generic notification will be displayed
    // saying: "This site has been updated in the background.". In order to
    // avoid flakiness, we give the URL the maximum value of EngagementPoints so
    // it will not display the generic notification.
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(profile());
    service->ResetBaseScoreForURL(app_url(), service->GetMaxPoints());
    CHECK(service->GetMaxPoints() == service->GetScore(app_url()));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_barrier =
        base::BarrierClosure(/*num_closures=*/2, run_loop.QuitClosure());
    push_service->SetMessageCallbackForTesting(quit_barrier);
    notification_tester_->SetNotificationAddedClosure(quit_barrier);
    push_service->OnMessage(app_identifier.app_id(), message);
    run_loop.Run();
  }

  PushMessagingAppIdentifier GetAppIdentifierForServiceWorkerRegistration(
      int64_t service_worker_registration_id) {
    PushMessagingAppIdentifier app_identifier =
        PushMessagingAppIdentifier::FindByServiceWorker(
            browser()->profile(), app_url(), service_worker_registration_id);
    return app_identifier;
  }

  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;

 private:
  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
};

IN_PROC_BROWSER_TEST_F(
    IsolatedWebAppBrowserServiceWorkerPushTest,
    ServiceWorkerPartitionedWhenWakingUpDueToPushNotification) {
  int64_t service_worker_version_id =
      InstallIsolatedWebAppAndWaitForServiceWorker();

  // Request and confirm permission to show notifications.
  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(app_web_contents_);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  ASSERT_EQ("permission status - granted", content::EvalJs(app_frame_, R"js(
    (async () => {
      return 'permission status - ' + await Notification.requestPermission();
    })();
  )js"));

  // Subscribe to push notifications and retrieve the app identifier.
  std::string push_messaging_endpoint = content::EvalJs(app_frame_, R"js(
// NIST P-256 public key made available to tests. Must be an uncompressed
// point in accordance with SEC1 2.3.3.
var kApplicationServerKey = new Uint8Array([
  0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36, 0x10, 0xC1,
  0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48, 0xC9, 0xC6, 0xBB, 0xBF,
  0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B, 0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52,
  0x21, 0xD3, 0x71, 0x90, 0x13, 0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1,
  0x7F, 0xF2, 0x76, 0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xFD
]);

(async () => {
  const registration = await navigator.serviceWorker.ready;
  const subscription = await registration.pushManager.subscribe({
      userVisibleOnly: true,
      applicationServerKey: kApplicationServerKey.buffer,
  });
  return subscription.endpoint;
})();
  )js")
                                            .ExtractString();

  size_t last_slash = push_messaging_endpoint.rfind('/');
  ASSERT_NE(last_slash, std::string::npos);
  ASSERT_EQ(base::FeatureList::IsEnabled(
                features::kPushMessagingGcmEndpointEnvironment)
                ? push_messaging::GetGcmEndpointForChannel(chrome::GetChannel())
                : kPushMessagingGcmEndpoint,
            push_messaging_endpoint.substr(0, last_slash + 1));
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_FALSE(app_identifier.is_null());

  // Close the browser and stop the ServiceWorker
  ServiceWorkerVersionStoppedRunningWaiter version_stopped_waiter(
      storage_partition_, service_worker_version_id);
  CloseBrowserSynchronously(app_window_);
  base::RunLoop run_loop;
  storage_partition_->GetServiceWorkerContext()->StopAllServiceWorkers(
      run_loop.QuitClosure());
  run_loop.Run();
  version_stopped_waiter.AwaitVersionStoppedRunning();

  // Push a message to the ServiceWorker and make sure the service worker is
  // started again.
  ServiceWorkerVersionStartedRunningWaiter version_started_waiter(
      storage_partition_, service_worker_version_id);

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "test";
  message.decrypted = true;
  SendMessageAndWaitUntilHandled(browser()->profile(), app_identifier, message);

  version_started_waiter.AwaitVersionStartedRunning();

  // Verify that the ServiceWorker has received the push message and created
  // a push notification, then click on it.
  auto notifications = notification_tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::WEB_PERSISTENT);
  EXPECT_EQ(notifications.size(), 1UL);

  BrowserWaiter browser_waiter(nullptr);
  notification_tester_->SimulateClick(NotificationHandler::Type::WEB_PERSISTENT,
                                      notifications[0].id(), std::nullopt,
                                      std::nullopt);

  // Check that the click resulted in a new isolated web app window that runs in
  // the same isolated non-default storage partition.
  auto* new_app_window = browser_waiter.AwaitAdded();
  auto* new_app_frame = GetPrimaryMainFrame(new_app_window);
  auto* new_storage_partition = new_app_frame->GetStoragePartition();
  EXPECT_EQ(new_storage_partition, storage_partition_);
  EXPECT_EQ(new_app_frame->GetWebExposedIsolationLevel(),
            content::WebExposedIsolationLevel::kIsolatedApplication);
  EXPECT_TRUE(AppBrowserController::IsWebApp(new_app_window));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, SharedWorker) {
  std::string register_worker_js = R"(
    const policy = trustedTypes.createPolicy('default', {
      createScriptURL: (url) => url,
    });
    const worker = new SharedWorker(
        policy.createScriptURL('/shared_worker.js'));

    let listener = null;
    worker.port.addEventListener('message', (e) => {
      listener(e.data);
      listener = null;
    });
    worker.port.start();

    function sendMessage(body) {
      if (listener !== null) {
        return Promise.reject('Already have pending request');
      }
      return new Promise((resolve) => {
        listener = resolve;
        worker.port.postMessage(body);
      });
    }
  )";

  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddFileFromDisk("/shared_worker.js",
                           "web_apps/simple_isolated_app/shared_worker.js")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));

  content::RenderFrameHost* app_frame1 = OpenApp(url_info.app_id());
  ASSERT_TRUE(ExecJs(app_frame1, register_worker_js));

  EXPECT_EQ("none", EvalJs(app_frame1, "sendMessage('hello')"));
  EXPECT_EQ("hello", EvalJs(app_frame1, "sendMessage('world')"));

  // Open a second window and make sure it uses the same worker instance.
  content::RenderFrameHost* app_frame2 = OpenApp(url_info.app_id());
  ASSERT_TRUE(ExecJs(app_frame2, register_worker_js));

  EXPECT_EQ("world", EvalJs(app_frame2, "sendMessage('frame2!')"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, DedicatedWorker) {
  std::string register_worker_js = R"(
    const policy = trustedTypes.createPolicy('default', {
      createScriptURL: (url) => url,
    });
    const worker = new Worker(policy.createScriptURL('/dedicated_worker.js'));

    let listener = null;
    worker.addEventListener('message', (e) => {
      listener(e.data);
      listener = null;
    });

    function sendMessage(body) {
      if (listener !== null) {
        return Promise.reject('Already have pending request');
      }
      return new Promise((resolve) => {
        listener = resolve;
        worker.postMessage(body);
      });
    }
  )";

  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddFileFromDisk("/dedicated_worker.js",
                           "web_apps/simple_isolated_app/dedicated_worker.js")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(ExecJs(app_frame, register_worker_js));

  EXPECT_EQ("none", EvalJs(app_frame, "sendMessage('hello')"));
  EXPECT_EQ("hello", EvalJs(app_frame, "sendMessage('world')"));
}

struct ExtensionTestParam {
  std::string test_name;
  bool should_succeed;
  // The value to set in the extension's manifest as
  // `externally_connectable.matches[0]`. `${IWA_ORIGIN}` will be replaced by
  // the IWA's origin without a trailing slash.
  std::string externally_connectable_match;
};

class IsolatedWebAppExtensionBrowserTest
    : public IsolatedWebAppBrowserTest,
      public ::testing::WithParamInterface<ExtensionTestParam> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    IsolatedWebAppBrowserTest::SetUp();
  }

  bool IsChromeRuntimeDefined(content::RenderFrameHost* app_frame) {
    return EvalJs(app_frame, "chrome.runtime !== undefined").ExtractBool();
  }

  std::string GetMatch(const IsolatedWebAppUrlInfo& url_info) {
    std::string origin = url_info.origin().GetURL().spec();
    std::string match = GetParam().externally_connectable_match;
    base::ReplaceSubstringsAfterOffset(
        &match, /*start_offset=*/0, "${IWA_ORIGIN}",
        base::TrimString(origin, "/", base::TRIM_TRAILING));
    return match;
  }

  base::ScopedTempDir temp_dir_;

  static constexpr std::string_view kExtensionManifest = R"({
    "name": "foo",
    "description": "foo",
    "version": "0.1",
    "manifest_version": 3,
    "externally_connectable": {
      "matches": [ $1 ]
    },
    "background": {"service_worker": "service_worker_background.js"}
  })";
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppExtensionBrowserTest,
                       SendMessageToExtension) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder().SetStartUrl("/index.html"))
          .AddFileFromDisk("/index.html",
                           "web_apps/simple_isolated_app/index.html")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::WriteFile(temp_dir_.GetPath().AppendASCII("manifest.json"),
                    content::JsReplace(kExtensionManifest, GetMatch(url_info)));
    // Extension: Listen for pings from the IWA.
    base::WriteFile(
        temp_dir_.GetPath().AppendASCII("service_worker_background.js"),
        R"(
        chrome.runtime.onMessageExternal.addListener(
          (request, sender, sendResponse) => {
            chrome.test.assertEq('iwa->extension: ping', request);
            sendResponse('extension->iwa: pong');
            chrome.test.notifyPass();
          });
    )");
  }

  extensions::ResultCatcher result_catcher;
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(temp_dir_.GetPath());
  ASSERT_TRUE(extension);

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  if (!GetParam().should_succeed) {
    ASSERT_FALSE(IsChromeRuntimeDefined(app_frame));
    return;
  }
  ASSERT_TRUE(IsChromeRuntimeDefined(app_frame));

  // IWA: Send a ping to the extension and wait for the pong.
  constexpr std::string_view kSendPing = R"(
    chrome.runtime.sendMessage($1, "iwa->extension: ping");
  )";
  EXPECT_EQ(EvalJs(app_frame, content::JsReplace(kSendPing, extension->id())),
            "extension->iwa: pong");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppExtensionBrowserTest, ConnectToExtension) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder().SetStartUrl("/index.html"))
          .AddFileFromDisk("/index.html",
                           "web_apps/simple_isolated_app/index.html")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::WriteFile(temp_dir_.GetPath().AppendASCII("manifest.json"),
                    content::JsReplace(kExtensionManifest, GetMatch(url_info)));
    // Extension: Listen for pings from the IWA.
    base::WriteFile(
        temp_dir_.GetPath().AppendASCII("service_worker_background.js"),
        R"(
          chrome.runtime.onConnectExternal.addListener(
            (port) =>
              port.onMessage.addListener((message) => {
                chrome.test.assertEq('iwa->extension: ping', message);
                port.postMessage('extension->iwa: pong');
                chrome.test.notifyPass();
              }));
    )");
  }

  extensions::ResultCatcher result_catcher;
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(temp_dir_.GetPath());

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  if (!GetParam().should_succeed) {
    ASSERT_FALSE(IsChromeRuntimeDefined(app_frame));
    return;
  }
  ASSERT_TRUE(IsChromeRuntimeDefined(app_frame));

  // IWA: Send a ping to the extension and wait for the pong.
  constexpr std::string_view kSendPing = R"(
    new Promise((resolve, reject) => {
      const port = chrome.runtime.connect($1);
      port.onMessage.addListener((response) => resolve(response));
      port.onDisconnect.addListener(() => reject());
      port.postMessage("iwa->extension: ping");
    });
  )";
  EXPECT_EQ(EvalJs(app_frame, content::JsReplace(kSendPing, extension->id())),
            "extension->iwa: pong");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix*/,
    IsolatedWebAppExtensionBrowserTest,
    ::testing::Values(
        ExtensionTestParam{
            .test_name = "origin_with_start_url",
            .should_succeed = true,
            // /index.html is the IWA's start_url which is opened in the test.
            .externally_connectable_match = {"${IWA_ORIGIN}/index.html"}},
        ExtensionTestParam{
            .test_name = "origin_with_other_path",
            .should_succeed = false,
            .externally_connectable_match = {"${IWA_ORIGIN}/foo"}},
        ExtensionTestParam{.test_name = "origin_with_star",
                           .should_succeed = true,
                           .externally_connectable_match = {"${IWA_ORIGIN}/*"}},
        ExtensionTestParam{.test_name = "all_urls",
                           .should_succeed = true,
                           .externally_connectable_match = {"<all_urls>"}},
        ExtensionTestParam{
            .test_name = "wildcard_all_iwas",
            .should_succeed = true,
            .externally_connectable_match = {"isolated-app://*/*"}},
        ExtensionTestParam{
            .test_name = "non_matching_url",
            .should_succeed = false,
            .externally_connectable_match = {"https://example.com/"}}),
    [](const ::testing::TestParamInfo<ExtensionTestParam>& info) {
      return info.param.test_name;
    });

}  // namespace web_app
