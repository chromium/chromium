// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"

namespace web_app {

namespace {

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
  raw_ptr<content::ServiceWorkerContext> service_worker_context_;

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
 public:
  IsolatedWebAppBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    isolated_web_app_dev_server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  }

  IsolatedWebAppBrowserTest(const IsolatedWebAppBrowserTest&) = delete;
  IsolatedWebAppBrowserTest& operator=(const IsolatedWebAppBrowserTest&) =
      delete;

 protected:
  content::StoragePartition* default_storage_partition() {
    return browser()->profile()->GetDefaultStoragePartition();
  }

  content::RenderFrameHost* GetPrimaryMainFrame(Browser* browser) {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  const net::EmbeddedTestServer& isolated_web_app_dev_server() {
    return *isolated_web_app_dev_server_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, AppsPartitioned) {
  web_app::IsolatedWebAppUrlInfo url_info1 = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());
  web_app::IsolatedWebAppUrlInfo url_info2 = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());

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
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());

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
  EXPECT_EQ(content::WebExposedIsolationLevel::kMaybeIsolatedApplication,
            app_frame->GetWebExposedIsolationLevel());
}

IN_PROC_BROWSER_TEST_F(
    IsolatedWebAppBrowserTest,
    OmniboxNavigationOpensNewPwaWindowEvenIfUserDisplayModeIsBrowser) {
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());

  WebAppProvider::GetForTest(browser()->profile())
      ->sync_bridge_unsafe()
      .SetAppUserDisplayMode(url_info.app_id(),
                             mojom::UserDisplayMode::kBrowser,
                             /*is_user_action=*/false);

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
  EXPECT_EQ(content::WebExposedIsolationLevel::kMaybeIsolatedApplication,
            app_frame->GetWebExposedIsolationLevel());
}

// Tests that the app menu doesn't have an 'Open in Chrome' option.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, NoOpenInChrome) {
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());
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
  EXPECT_FALSE(found);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, WasmLoadableFromFile) {
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());
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
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());
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
  web_app::IsolatedWebAppUrlInfo url_info =
      InstallDevModeProxyIsolatedWebApp(url::Origin::Create(
          isolated_web_app_dev_server().GetURL("localhost", "/")));

  GURL app_url = url_info.origin().GetURL().Resolve("/cookie.html");
  GURL app_proxy_url =
      isolated_web_app_dev_server().GetURL("localhost", "/cookie.html");
  GURL non_app_url = https_server()->GetURL(
      kNonAppHost, "/web_apps/simple_isolated_app/cookie.html");

  // Load a page that sets a cookie, then create a cross-origin iframe that
  // loads the same page.
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  web_app::CreateIframe(app_frame, "child", non_app_url, "");

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
  web_app::CreateIframe(app_frame2, "child", non_app_url, "");

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
    web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
        isolated_web_app_dev_server().GetOrigin());
    url_info_ = url_info;

    content::RenderFrameHost* original_frame = OpenApp(url_info.app_id());
    CHECK_NE(default_storage_partition(),
             original_frame->GetStoragePartition());

    app_web_contents_ =
        content::WebContents::FromRenderFrameHost(original_frame);
    app_window_ = GetBrowserFromFrame(original_frame);

    GURL register_service_worker_page =
        url_info.origin().GetURL().Resolve("register_service_worker.html");

    app_frame_ =
        ui_test_utils::NavigateToURL(app_window_, register_service_worker_page);
    storage_partition_ = app_frame_->GetStoragePartition();
    CHECK_NE(default_storage_partition(), storage_partition_);

    ServiceWorkerVersionActivatedWaiter version_activated_waiter(
        storage_partition_, url_info.origin().GetURL());

    return version_activated_waiter.AwaitVersionActivated();
  }

  const web_app::IsolatedWebAppUrlInfo& url_info() const {
    CHECK(url_info_.has_value());
    return *url_info_;
  }

  raw_ptr<Browser, DanglingUntriaged> app_window_;
  raw_ptr<content::WebContents, DanglingUntriaged> app_web_contents_;
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> app_frame_;
  raw_ptr<content::StoragePartition, DanglingUntriaged> storage_partition_;
  absl::optional<web_app::IsolatedWebAppUrlInfo> url_info_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserServiceWorkerTest,
                       ServiceWorkerPartitioned) {
  InstallIsolatedWebAppAndWaitForServiceWorker();
  test::CheckServiceWorkerStatus(
      url_info().origin().GetURL(), storage_partition_,
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

    CHECK_EQ(push_service->GetPermissionStatus(url_info_->origin().GetURL(),
                                               /*user_visible=*/true),
             blink::mojom::PermissionStatus::GRANTED);

    // A second auto-generated notifications will be shown.
    // See PushMessagingNotificationManager::EnforceUserVisibleOnlyRequirements.
    base::RunLoop run_loop;
    base::RepeatingClosure quit_barrier =
        base::BarrierClosure(/*num_closures=*/3, run_loop.QuitClosure());
    push_service->SetMessageCallbackForTesting(quit_barrier);
    notification_tester_->SetNotificationAddedClosure(quit_barrier);
    push_service->OnMessage(app_identifier.app_id(), message);
    run_loop.Run();
  }

  PushMessagingAppIdentifier GetAppIdentifierForServiceWorkerRegistration(
      int64_t service_worker_registration_id) {
    PushMessagingAppIdentifier app_identifier =
        PushMessagingAppIdentifier::FindByServiceWorker(
            browser()->profile(), url_info().origin().GetURL(),
            service_worker_registration_id);
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
  ASSERT_EQ(kPushMessagingGcmEndpoint,
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
  EXPECT_EQ(notifications.size(), 2UL);

  BrowserWaiter browser_waiter(nullptr);
  notification_tester_->SimulateClick(NotificationHandler::Type::WEB_PERSISTENT,
                                      notifications[0].id(), absl::nullopt,
                                      absl::nullopt);

  // Check that the click resulted in a new isolated web app window that runs in
  // the same isolated non-default storage partition.
  auto* new_app_window = browser_waiter.AwaitAdded();
  auto* new_app_frame = new_app_window->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetPrimaryMainFrame();
  auto* new_storage_partition = new_app_frame->GetStoragePartition();
  EXPECT_EQ(new_storage_partition, storage_partition_);
  EXPECT_EQ(new_app_frame->GetWebExposedIsolationLevel(),
            content::WebExposedIsolationLevel::kMaybeIsolatedApplication);
  EXPECT_TRUE(AppBrowserController::IsWebApp(new_app_window));
}

}  // namespace web_app
