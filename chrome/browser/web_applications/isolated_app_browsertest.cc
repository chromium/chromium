// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
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
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"

namespace web_app {

namespace {

const char kAppHost[] = "app.com";
const char kApp2Host[] = "app2.com";
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
    if (service_worker_context_)
      service_worker_context_->RemoveObserver(this);
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
      GURL& url)
      : BaseServiceWorkerVersionWaiter(storage_partition), url_(url) {}

  int64_t AwaitVersionActivated() { return future.Get(); }

 private:
  // content::ServiceWorkerContextObserver:
  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    if (content::ServiceWorkerContext::ScopeMatches(scope, url_)) {
      future.SetValue(version_id);
    }
  }

  const GURL url_;
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

class IsolatedAppBrowserTest : public IsolatedAppBrowserTestHarness {
 public:
  IsolatedAppBrowserTest() = default;
  IsolatedAppBrowserTest(const IsolatedAppBrowserTest&) = delete;
  IsolatedAppBrowserTest& operator=(const IsolatedAppBrowserTest&) = delete;
  ~IsolatedAppBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedAppBrowserTestHarness::SetUpCommandLine(command_line);

    std::string isolated_app_origins =
        std::string("https://") + kAppHost + ",https://" + kApp2Host;
    command_line->AppendSwitchASCII(switches::kIsolatedAppOrigins,
                                    isolated_app_origins);
  }

 protected:
  content::StoragePartition* default_storage_partition() {
    return browser()->profile()->GetDefaultStoragePartition();
  }

  content::RenderFrameHost* GetMainFrame(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

  Browser* GetBrowserFromFrame(content::RenderFrameHost* frame) {
    Browser* browser = chrome::FindBrowserWithWebContents(
        content::WebContents::FromRenderFrameHost(frame));
    EXPECT_TRUE(browser);
    return browser;
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserTest, AppsPartitioned) {
  AppId app1_id = InstallIsolatedApp(kAppHost);
  AppId app2_id = InstallIsolatedApp(kApp2Host);

  auto* non_app_frame = ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/banners/isolated/simple.html"));
  EXPECT_TRUE(non_app_frame);
  EXPECT_EQ(default_storage_partition(), non_app_frame->GetStoragePartition());

  auto* app_frame = OpenApp(app1_id);
  EXPECT_NE(default_storage_partition(), app_frame->GetStoragePartition());

  auto* app2_frame = OpenApp(app2_id);
  EXPECT_NE(default_storage_partition(), app2_frame->GetStoragePartition());

  EXPECT_NE(app_frame->GetStoragePartition(),
            app2_frame->GetStoragePartition());
}

IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserTest,
                       OmniboxNavigationOpensNewPwaWindow) {
  AppId app_id = InstallIsolatedApp(kAppHost);

  GURL app_url =
      https_server()->GetURL(kAppHost, "/banners/isolated/simple.html");
  auto* app_frame =
      NavigateToURLInNewTab(browser(), app_url, WindowOpenDisposition::UNKNOWN);

  // The browser shouldn't have opened the app's page.
  EXPECT_EQ(GetMainFrame(browser())->GetLastCommittedURL(), GURL());

  // The app's frame should belong to an isolated PWA browser window.
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
  EXPECT_EQ(content::RenderFrameHost::WebExposedIsolationLevel::
                kMaybeIsolatedApplication,
            app_frame->GetWebExposedIsolationLevel());
}

IN_PROC_BROWSER_TEST_F(
    IsolatedAppBrowserTest,
    OmniboxNavigationOpensNewPwaWindowEvenIfUserDisplayModeIsBrowser) {
  AppId app_id = InstallIsolatedApp(kAppHost);

  WebAppProvider::GetForTest(browser()->profile())
      ->sync_bridge()
      .SetAppUserDisplayMode(app_id, UserDisplayMode::kBrowser,
                             /*is_user_action=*/false);

  GURL app_url =
      https_server()->GetURL(kAppHost, "/banners/isolated/simple.html");
  auto* app_frame =
      NavigateToURLInNewTab(browser(), app_url, WindowOpenDisposition::UNKNOWN);

  // The browser shouldn't have opened the app's page.
  EXPECT_EQ(GetMainFrame(browser())->GetLastCommittedURL(), GURL());

  // The app's frame should belong to an isolated PWA browser window.
  Browser* app_browser = GetBrowserFromFrame(app_frame);
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
  EXPECT_EQ(content::RenderFrameHost::WebExposedIsolationLevel::
                kMaybeIsolatedApplication,
            app_frame->GetWebExposedIsolationLevel());
}

// Tests that the app menu doesn't have an 'Open in Chrome' option.
IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserTest, NoOpenInChrome) {
  AppId app_id = InstallIsolatedApp(kAppHost);
  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);

  EXPECT_FALSE(
      app_browser->command_controller()->IsCommandEnabled(IDC_OPEN_IN_CHROME));

  auto app_menu_model = std::make_unique<WebAppMenuModel>(
      /*provider=*/nullptr, app_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  const bool found = app_menu_model->GetModelAndIndexForCommandId(
      IDC_OPEN_IN_CHROME, &model, &index);
  EXPECT_FALSE(found);
}

class IsolatedAppBrowserCookieTest : public IsolatedAppBrowserTest {
 public:
  using CookieHeaders = std::vector<std::string>;

  void SetUpOnMainThread() override {
    https_server()->RegisterRequestMonitor(base::BindRepeating(
        &IsolatedAppBrowserCookieTest::MonitorRequest, base::Unretained(this)));

    IsolatedAppBrowserTest::SetUpOnMainThread();
  }

 protected:
  // Returns the "Cookie" headers that were received for the given URL.
  const CookieHeaders& GetCookieHeadersForUrl(const GURL& url) {
    return cookie_map_[url.spec()];
  }

  void CreateIframe(content::RenderFrameHost* parent_frame,
                    const std::string& iframe_id,
                    const GURL& url) {
    EXPECT_EQ(true, content::EvalJs(parent_frame,
                                    content::JsReplace(R"(
            new Promise(resolve => {
              let f = document.createElement('iframe');
              f.id = $1;
              f.src = $2;
              f.addEventListener('load', () => resolve(true));
              document.body.appendChild(f);
            });
        )",
                                                       iframe_id, url)));
  }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    // Replace the host in |request.GetURL()| with the value from the Host
    // header, as GetURL()'s host will be 127.0.0.1.
    std::string host = GURL("https://" + GetHeader(request, "Host")).host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    GURL url = request.GetURL().ReplaceComponents(replace_host);
    cookie_map_[url.spec()].push_back(GetHeader(request, "cookie"));
  }

  std::string GetHeader(const net::test_server::HttpRequest& request,
                        const std::string& header_name) {
    auto header = request.headers.find(header_name);
    return header != request.headers.end() ? header->second : "";
  }

  // Maps GURLs to a vector of cookie strings. The nth item in the vector will
  // contain the contents of the "Cookies" header for the nth request to the
  // given GURL.
  std::unordered_map<std::string, CookieHeaders> cookie_map_;
};

IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserCookieTest, Cookies) {
  AppId app_id = InstallIsolatedApp(kAppHost);

  GURL app_url =
      https_server()->GetURL(kAppHost, "/banners/isolated/cookie.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/cookie.html");

  // Load a page that sets a cookie, then create a cross-origin iframe that
  // loads the same page.
  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  CreateIframe(app_frame, "child", non_app_url);

  const auto& app_cookies = GetCookieHeadersForUrl(app_url);
  EXPECT_EQ(1u, app_cookies.size());
  EXPECT_TRUE(app_cookies[0].empty());
  const auto& non_app_cookies = GetCookieHeadersForUrl(non_app_url);
  EXPECT_EQ(1u, non_app_cookies.size());
  EXPECT_TRUE(non_app_cookies[0].empty());

  // Load the pages again. Both frames should send the cookie in their requests.
  auto* app_frame2 = OpenApp(app_id);
  auto* app_browser2 = GetBrowserFromFrame(app_frame2);
  app_frame2 = ui_test_utils::NavigateToURL(app_browser2, app_url);
  CreateIframe(app_frame2, "child", non_app_url);

  EXPECT_EQ(2u, app_cookies.size());
  EXPECT_EQ("foo=bar", app_cookies[1]);
  EXPECT_EQ(2u, non_app_cookies.size());
  EXPECT_EQ("foo=bar", non_app_cookies[1]);

  // Load the cross-origin's iframe as a top-level page. Because this page was
  // previously loaded in an isolated app, it shouldn't have cookies set when
  // loaded in a main frame here.
  ASSERT_TRUE(NavigateToURLInNewTab(browser(), non_app_url));

  EXPECT_EQ(3u, non_app_cookies.size());
  EXPECT_TRUE(non_app_cookies[2].empty());
}

class IsolatedAppBrowserServiceWorkerTest : public IsolatedAppBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    IsolatedAppBrowserTest::SetUpOnMainThread();

    app_url_ = https_server()->GetURL(
        kAppHost, "/banners/isolated/register_service_worker.html");
  }

  int64_t InstallIsolatedAppAndWaitForServiceWorker() {
    AppId app_id = InstallIsolatedApp(app_url_);

    auto* original_frame = OpenApp(app_id);
    app_web_contents_ =
        content::WebContents::FromRenderFrameHost(original_frame);
    app_window_ = chrome::FindBrowserWithWebContents(app_web_contents_);
    app_frame_ = ui_test_utils::NavigateToURL(app_window_, app_url_);
    storage_partition_ = app_frame_->GetStoragePartition();
    EXPECT_NE(default_storage_partition(), storage_partition_);

    ServiceWorkerVersionActivatedWaiter version_activated_waiter(
        storage_partition_, app_url_);

    return version_activated_waiter.AwaitVersionActivated();
  }

  Browser* app_window_;
  content::WebContents* app_web_contents_;
  content::RenderFrameHost* app_frame_;
  content::StoragePartition* storage_partition_;

  GURL app_url_;
};

IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserServiceWorkerTest,
                       ServiceWorkerPartitioned) {
  InstallIsolatedAppAndWaitForServiceWorker();
  test::CheckServiceWorkerStatus(
      app_url_, storage_partition_,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

class IsolatedAppBrowserServiceWorkerPushTest
    : public IsolatedAppBrowserServiceWorkerTest {
 public:
  IsolatedAppBrowserServiceWorkerPushTest()
      : scoped_testing_factory_installer_(
            base::BindRepeating(&gcm::FakeGCMProfileService::Build)) {}

 protected:
  void SetUpOnMainThread() override {
    IsolatedAppBrowserServiceWorkerTest::SetUpOnMainThread();

    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
  }

  void SendMessageAndWaitUntilHandled(
      content::BrowserContext* context,
      const PushMessagingAppIdentifier& app_identifier,
      const gcm::IncomingMessage& message) {
    auto* push_service = PushMessagingServiceFactory::GetForProfile(context);
    base::RunLoop run_loop;
    base::RepeatingClosure quit_barrier =
        base::BarrierClosure(2 /* num_closures */, run_loop.QuitClosure());
    push_service->SetMessageCallbackForTesting(quit_barrier);
    notification_tester_->SetNotificationAddedClosure(quit_barrier);
    push_service->OnMessage(app_identifier.app_id(), message);
    run_loop.Run();
  }

  PushMessagingAppIdentifier GetAppIdentifierForServiceWorkerRegistration(
      int64_t service_worker_registration_id) {
    GURL origin = url::Origin::Create(app_url_).GetURL();

    PushMessagingAppIdentifier app_identifier =
        PushMessagingAppIdentifier::FindByServiceWorker(
            browser()->profile(), origin, service_worker_registration_id);
    return app_identifier;
  }

  std::string RunScript(content::RenderFrameHost* app_frame,
                        const std::string& script) {
    std::string script_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(app_frame, script,
                                                       &script_result));
    return script_result;
  }

  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;

 private:
  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
};

IN_PROC_BROWSER_TEST_F(
    IsolatedAppBrowserServiceWorkerPushTest,
    ServiceWorkerPartitionedWhenWakingUpDuetoPushNotification) {
  int64_t service_worker_version_id =
      InstallIsolatedAppAndWaitForServiceWorker();

  // Request and confirm permission to show notifications.
  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(app_web_contents_);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_EQ("permission status - granted",
            RunScript(app_frame_, "requestNotificationPermission()"));

  // Subscribe to push notifications and retrieve the app identifier.
  std::string push_messaging_endpoint =
      RunScript(app_frame_, "documentSubscribePush()");
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
  EXPECT_EQ(notifications.size(), 1UL);

  web_app::BrowserWaiter browser_waiter(nullptr);
  notification_tester_->SimulateClick(NotificationHandler::Type::WEB_PERSISTENT,
                                      notifications[0].id(), absl::nullopt,
                                      absl::nullopt);

  // Check that the click resulted in a new isolated web app window that runs in
  // the same isolated non-default storage partition.
  auto* new_app_window = browser_waiter.AwaitAdded();
  auto* new_app_frame =
      new_app_window->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  auto* new_storage_partition = new_app_frame->GetStoragePartition();
  EXPECT_EQ(new_storage_partition, storage_partition_);
  EXPECT_EQ(new_app_frame->GetWebExposedIsolationLevel(),
            content::RenderFrameHost::WebExposedIsolationLevel::
                kMaybeIsolatedApplication);
  EXPECT_TRUE(AppBrowserController::IsWebApp(new_app_window));
}

}  // namespace web_app
