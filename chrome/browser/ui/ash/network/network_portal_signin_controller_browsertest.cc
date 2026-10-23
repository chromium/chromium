// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ui/ash/network/network_portal_signin_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kPortalPath[] = "/portal";
constexpr char kDownloadPath[] = "/download.txt";

const NetworkState& GetDefaultNetwork() {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  DCHECK(network);
  return *network;
}

// Serves a fake captive portal page, plus a file sent as an attachment that the
// portal page can navigate to in order to trigger a download.
std::unique_ptr<net::test_server::HttpResponse> HandlePortalRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request.relative_url == kPortalPath) {
    response->set_content_type("text/html");
    response->set_content("<html><body>Captive portal</body></html>");
    return response;
  }
  if (request.relative_url == kDownloadPath) {
    response->set_content_type("text/plain");
    response->AddCustomHeader("Content-Disposition",
                              "attachment; filename=download.txt");
    response->set_content("payload");
    return response;
  }
  return nullptr;
}

// Waits for the first outcome of a download request: either the DownloadManager
// drops the request, or it creates a DownloadItem for it.
class DownloadOutcomeWaiter : public content::DownloadManager::Observer {
 public:
  explicit DownloadOutcomeWaiter(content::DownloadManager* download_manager) {
    observation_.Observe(download_manager);
  }
  DownloadOutcomeWaiter(const DownloadOutcomeWaiter&) = delete;
  DownloadOutcomeWaiter& operator=(const DownloadOutcomeWaiter&) = delete;
  ~DownloadOutcomeWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  bool download_created() const { return download_created_; }
  bool download_dropped() const { return download_dropped_; }

  // content::DownloadManager::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    download_created_ = true;
    run_loop_.Quit();
  }
  void OnDownloadDropped(content::DownloadManager* manager) override {
    download_dropped_ = true;
    run_loop_.Quit();
  }
  void ManagerGoingDown(content::DownloadManager* manager) override {
    observation_.Reset();
    run_loop_.Quit();
  }

 private:
  bool download_created_ = false;
  bool download_dropped_ = false;
  base::RunLoop run_loop_;
  base::ScopedObservation<content::DownloadManager,
                          content::DownloadManager::Observer>
      observation_{this};
};

}  // namespace

class NetworkPortalSigninControllerBrowserTest : public InProcessBrowserTest {
 public:
  NetworkPortalSigninControllerBrowserTest() = default;
  ~NetworkPortalSigninControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    network_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    // Set ethernet to idle.
    network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                        shill::kStateProperty,
                                        base::Value(shill::kStateIdle));
    // Set WiFi (now the default) to redirect-found.
    network_helper_->SetServiceProperty(
        GetDefaultNetwork().path(), shill::kStateProperty,
        base::Value(shill::kStateRedirectFound));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NetworkHandlerTestHelper> network_helper_;
};

IN_PROC_BROWSER_TEST_F(NetworkPortalSigninControllerBrowserTest,
                       SigninDefault) {
  NetworkPortalSigninController::Get()->ShowSignin(
      NetworkPortalSigninController::SigninSource::kNotification);

  BrowserDelegate* browser =
      NetworkPortalSigninWindow::Get()->GetBrowserForTesting();
  ASSERT_TRUE(browser);
  EXPECT_TRUE(browser->IsOffTheRecord());
}

// Runs on the OOBE screen, i.e. before any user has logged in, where the portal
// signin UI is shown in a dialog that uses the signin profile.
class NetworkPortalSigninControllerOobeTest : public OobeBaseTest {
 public:
  NetworkPortalSigninControllerOobeTest() = default;
  ~NetworkPortalSigninControllerOobeTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    portal_server_.RegisterRequestHandler(
        base::BindRepeating(&HandlePortalRequest));
    ASSERT_TRUE(portal_server_.Start());

    network_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    // Set ethernet to idle.
    network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                        shill::kStateProperty,
                                        base::Value(shill::kStateIdle));
    // Point the probe URL of WiFi (now the default) at the fake portal, as a
    // hostile network intercepting the probe would, and set it to
    // redirect-found.
    const std::string wifi_path = GetDefaultNetwork().path();
    network_helper_->SetServiceProperty(
        wifi_path, shill::kProbeUrlProperty,
        base::Value(portal_server_.GetURL(kPortalPath).spec()));
    network_helper_->SetServiceProperty(
        wifi_path, shill::kStateProperty,
        base::Value(shill::kStateRedirectFound));
  }

 protected:
  net::EmbeddedTestServer portal_server_;
  std::unique_ptr<NetworkHandlerTestHelper> network_helper_;
};

// The portal page is untrusted, network-controlled content and the dialog has
// no download UI, so the page must not be able to download files. This includes
// the first download, which DownloadRequestLimiter would otherwise allow
// without a user gesture.
IN_PROC_BROWSER_TEST_F(NetworkPortalSigninControllerOobeTest,
                       SigninDialogBlocksDownloads) {
  const GURL portal_url = portal_server_.GetURL(kPortalPath);
  content::TestNavigationObserver portal_load_observer(portal_url);
  portal_load_observer.StartWatchingNewWebContents();

  NetworkPortalSigninController::Get()->ShowSignin(
      NetworkPortalSigninController::SigninSource::kSettings);
  portal_load_observer.Wait();
  ASSERT_TRUE(portal_load_observer.last_navigation_succeeded());

  views::Widget* widget =
      NetworkPortalSigninController::Get()->GetDialogWidgetForTesting();
  ASSERT_TRUE(widget);
  auto* web_dialog_view =
      views::AsViewClass<views::WebDialogView>(widget->client_view());
  ASSERT_TRUE(web_dialog_view);
  content::WebContents* web_contents = web_dialog_view->web_contents();
  ASSERT_TRUE(web_contents);
  ASSERT_EQ(portal_url, web_contents->GetLastCommittedURL());

  DownloadOutcomeWaiter download_waiter(
      web_contents->GetBrowserContext()->GetDownloadManager());
  // Trigger a download from the portal page without a user gesture.
  ASSERT_TRUE(
      content::ExecJs(web_contents,
                      content::JsReplace("location.href = $1;",
                                         portal_server_.GetURL(kDownloadPath)),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  download_waiter.Wait();

  EXPECT_TRUE(download_waiter.download_dropped());
  EXPECT_FALSE(download_waiter.download_created());
}

}  // namespace ash
