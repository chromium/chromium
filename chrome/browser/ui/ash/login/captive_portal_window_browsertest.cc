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
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ui/ash/login/captive_portal_window_proxy.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

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

  void SetUpInProcessBrowserTestFixture() override {
    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    network_portal_detector::InitializeForTesting(network_portal_detector_);
  }

  void TearDownOnMainThread() override { captive_portal_window_proxy_.reset(); }

 private:
  std::unique_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;
  raw_ptr<NetworkPortalDetectorTestImpl, DanglingUntriaged>
      network_portal_detector_;
};

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, Show) {
  Show(kWifiServicePath);
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
