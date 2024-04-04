// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/network/network_portal_signin_window.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kDefaultPortalUrl[] = "http://www.gstatic.com/generate_204";

const NetworkState& GetDefaultNetwork() {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  DCHECK(network);
  return *network;
}

}  // namespace

class NetworkPortalSigninControllerBrowserTestBase
    : public InProcessBrowserTest {
 public:
  NetworkPortalSigninControllerBrowserTestBase() = default;
  ~NetworkPortalSigninControllerBrowserTestBase() override = default;

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

  void SetupFeature(bool enabled) {
    feature_list_.InitWithFeatureState(
        chromeos::features::kCaptivePortalPopupWindow, enabled);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NetworkHandlerTestHelper> network_helper_;
};

class NetworkPortalSigninControllerBrowserTest
    : public NetworkPortalSigninControllerBrowserTestBase {
 public:
  NetworkPortalSigninControllerBrowserTest() { SetupFeature(false); }
  ~NetworkPortalSigninControllerBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(NetworkPortalSigninControllerBrowserTest,
                       SigninDefault) {
  GURL portal_url = GURL(kDefaultPortalUrl);
  content::TestNavigationObserver navigation_observer(portal_url);
  navigation_observer.StartWatchingNewWebContents();

  NetworkPortalSigninController::Get()->ShowSignin(
      NetworkPortalSigninController::SigninSource::kNotification);
  base::RunLoop().RunUntilIdle();

  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(), portal_url);
}

class NetworkPortalSigninControllerPopupWindowBrowserTest
    : public NetworkPortalSigninControllerBrowserTestBase {
 public:
  NetworkPortalSigninControllerPopupWindowBrowserTest() { SetupFeature(true); }
  ~NetworkPortalSigninControllerPopupWindowBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(NetworkPortalSigninControllerPopupWindowBrowserTest,
                       SigninDefault) {
  NetworkPortalSigninController::Get()->ShowSignin(
      NetworkPortalSigninController::SigninSource::kNotification);
  base::RunLoop().RunUntilIdle();

  Browser* browser =
      chromeos::NetworkPortalSigninWindow::Get()->GetBrowserForTesting();
  ASSERT_TRUE(browser);
  EXPECT_NE(browser->profile(), ProfileManager::GetActiveUserProfile());
}

}  // namespace ash
