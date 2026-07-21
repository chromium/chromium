// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/captive_portal_view.h"
#include "chrome/browser/ui/ash/login/captive_portal_window_proxy.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/webview/simple_web_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class FakeSingleWebContentsDialogManager
    : public web_modal::SingleWebContentsDialogManager {
 public:
  FakeSingleWebContentsDialogManager(
      gfx::NativeWindow dialog,
      web_modal::SingleWebContentsDialogManagerDelegate* delegate)
      : dialog_(dialog), delegate_(delegate) {}
  void Show() override { is_active_ = true; }
  void Hide() override { is_active_ = false; }
  void Close() override {
    is_active_ = false;
    delegate_->WillClose(dialog_);
  }
  void Focus() override {}
  void Pulse() override {}
  void HostChanged(web_modal::WebContentsModalDialogHost* new_host) override {}
  gfx::NativeWindow dialog() override { return dialog_; }
  bool IsActive() const override { return is_active_; }

 private:
  gfx::NativeWindow dialog_;
  raw_ptr<web_modal::SingleWebContentsDialogManagerDelegate> delegate_;
  bool is_active_ = false;
};

constexpr char kWifiServicePath[] = "/service/wifi1";
const test::UIPath kCaptivePortalLink = {"error-message",
                                         "captive-portal-fix-link"};

}  // namespace

class CaptivePortalWindowTest : public InProcessBrowserTest {
 protected:
  void ShowIfRedirected(const std::string& network_name) {
    captive_portal_window_proxy_->ShowIfRedirected(network_name);
  }

  void Show(const std::string& network_name) {
    captive_portal_window_proxy_->Show(network_name);
  }

  void Close() { captive_portal_window_proxy_->Close(); }

  void OnRedirected(const std::string& network_name) {
    captive_portal_window_proxy_->OnRedirected(network_name);
  }

  void OnOriginalURLLoaded() {
    captive_portal_window_proxy_->OnOriginalURLLoaded();
  }

  void CheckState(bool is_shown, bool in_progress) {
    bool actual_is_shown = (CaptivePortalWindowProxy::STATE_DISPLAYED ==
                            captive_portal_window_proxy_->GetState());
    ASSERT_EQ(is_shown, actual_is_shown);
  }

  CaptivePortalView* GetCaptivePortalView() {
    if (captive_portal_window_proxy_->delegate_) {
      return static_cast<CaptivePortalView*>(
          captive_portal_window_proxy_->delegate_->GetContentsView());
    }
    return captive_portal_window_proxy_->captive_portal_view_.get();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kDisableHIDDetectionOnOOBEForTesting);
  }

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        LoginDisplayHost::default_host()->GetOobeWebContents();
    captive_portal_window_proxy_ =
        std::make_unique<CaptivePortalWindowProxy>(web_contents);
  }

  void TearDownOnMainThread() override { captive_portal_window_proxy_.reset(); }

 private:
  std::unique_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;
};

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, Show) {
  Show(kWifiServicePath);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest,
                       WebContentsModalDialogManagerWired) {
  Show(kWifiServicePath);
  CaptivePortalView* portal_view = GetCaptivePortalView();
  ASSERT_TRUE(portal_view);

  views::SimpleWebView* simple_web_view = portal_view->simple_web_view();
  ASSERT_TRUE(simple_web_view);
  content::WebContents* web_contents = simple_web_view->GetWebViewWebContents();
  ASSERT_TRUE(web_contents);

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  ASSERT_TRUE(manager);
  ASSERT_NE(manager->delegate(), nullptr);
  auto* host = manager->delegate()->GetWebContentsModalDialogHost(web_contents);
  ASSERT_TRUE(host);
  EXPECT_EQ(host->GetHostView(),
            simple_web_view->GetView()->GetWidget()->GetNativeView());
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, ModalDialogTeardown) {
  Show(kWifiServicePath);
  CaptivePortalView* portal_view = GetCaptivePortalView();
  ASSERT_TRUE(portal_view);

  content::WebContents* web_contents =
      portal_view->simple_web_view()->GetWebViewWebContents();
  ASSERT_TRUE(web_contents);

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  ASSERT_TRUE(manager);

  std::unique_ptr<views::Widget> dialog_widget =
      std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.parent = portal_view->GetWidget()->GetNativeView();
  dialog_widget->Init(std::move(params));

  manager->ShowDialogWithManager(
      dialog_widget->GetNativeWindow(),
      std::make_unique<FakeSingleWebContentsDialogManager>(
          dialog_widget->GetNativeWindow(), manager));
  EXPECT_TRUE(manager->IsDialogActive());

  views::test::WidgetDestroyedWaiter waiter(portal_view->GetWidget());
  Close();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, ShowClose) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  Show(kWifiServicePath);
  CheckState(/*is_shown=*/true, /*in_progress=*/false);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/false);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnRedirected) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  ShowIfRedirected(kWifiServicePath);
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  OnRedirected(kWifiServicePath);
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnOriginalURLLoaded) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  ShowIfRedirected(kWifiServicePath);
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  OnRedirected(kWifiServicePath);
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  OnOriginalURLLoaded();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, MultipleCalls) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  ShowIfRedirected(kWifiServicePath);
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  Show(kWifiServicePath);
  CheckState(/*is_shown=*/true, /*in_progress=*/false);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  OnRedirected(kWifiServicePath);
  CheckState(/*is_shown=*/false, /*in_progress=*/true);

  OnOriginalURLLoaded();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);

  Show(kWifiServicePath);
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  OnRedirected(kWifiServicePath);
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);

  OnOriginalURLLoaded();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);
}

class CaptivePortalWindowCtorDtorTest : public LoginManagerTest {
 public:
  CaptivePortalWindowCtorDtorTest() = default;

  CaptivePortalWindowCtorDtorTest(const CaptivePortalWindowCtorDtorTest&) =
      delete;
  CaptivePortalWindowCtorDtorTest& operator=(
      const CaptivePortalWindowCtorDtorTest&) = delete;

  ~CaptivePortalWindowCtorDtorTest() override = default;

  void SetUpOnMainThread() override {
    // Set up fake networks.
    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/true);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();

    LoginManagerTest::SetUpOnMainThread();
  }
  void TearDownOnMainThread() override {
    network_state_test_helper_.reset();
    LoginManagerTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
  // Use fake GAIA to avoid potential flakiness when real GAIA would not
  // load and Error screen would be shown instead of Login screen.
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

// Flaky on multiple builders, see crbug.com/1244162
IN_PROC_BROWSER_TEST_F(CaptivePortalWindowCtorDtorTest, OpenPortalDialog) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  ASSERT_TRUE(host);
  OobeUI* oobe = host->GetOobeUI();
  ASSERT_TRUE(oobe);

  // Skip to gaia screen.
  host->GetWizardController()->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  // Disconnect from all networks in order to trigger the network screen.
  network_state_test_helper_->service_test()->ClearServices();
  base::RunLoop().RunUntilIdle();

  // Add an offline WiFi network.
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateIdle, /*visible=*/true);
  base::RunLoop().RunUntilIdle();

  // Wait for ErrorScreen to appear.
  ErrorScreen* error_screen = oobe->GetErrorScreen();
  ASSERT_TRUE(error_screen);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  // Change network to be behind a captive portal.
  network_state_test_helper_->service_test()->SetServiceProperty(
      kWifiServicePath, shill::kStateProperty,
      base::Value(shill::kStateRedirectFound));
  base::RunLoop().RunUntilIdle();

  // As we haven't specified the actual captive portal page, redirect won't
  // happen automatically, but the message to open the captive portal login page
  // must be available.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kCaptivePortalLink)
      ->Wait();

  // Click on the link to open captive portal page.
  test::OobeJS().ClickOnPath(kCaptivePortalLink);
  EXPECT_TRUE(
      error_screen->captive_portal_window_proxy()->IsDisplayedForTesting());
}

}  // namespace ash
