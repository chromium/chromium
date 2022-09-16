// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

constexpr char kTestPortalUrl[] = "http://www.gstatic.com/generate_204";

class TestSigninController : public NetworkPortalSigninController {
 public:
  TestSigninController() = default;
  TestSigninController(const TestSigninController&) = delete;
  TestSigninController& operator=(const TestSigninController&) = delete;
  ~TestSigninController() override = default;

  // NetworkPortalSigninController
  void ShowDialog(Profile* profile, const GURL& url) override {
    dialog_url_ = url.spec();
  }
  void ShowTab(Profile* profile, const GURL& url) override {
    tab_url_ = url.spec();
  }

  const std::string& dialog_url() const { return dialog_url_; }
  const std::string& tab_url() const { return tab_url_; }

 private:
  std::string dialog_url_;
  std::string tab_url_;
};

}  // namespace

class NetworkPortalSigninControllerTest : public testing::Test {
 public:
  NetworkPortalSigninControllerTest() = default;
  NetworkPortalSigninControllerTest(const NetworkPortalSigninControllerTest&) =
      delete;
  NetworkPortalSigninControllerTest& operator=(
      const NetworkPortalSigninControllerTest&) = delete;
  ~NetworkPortalSigninControllerTest() override = default;

  void SetUp() override {
    network_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    controller_ = std::make_unique<TestSigninController>();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { network_helper_.reset(); }

 protected:
  void SimulateLogin() {
    test_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(test_profile_manager_->SetUp());
    test_profile_manager_->CreateTestingProfile("primary_user");
  }

  std::string SetProbeUrl(const std::string& url) {
    std::string expected_url;
    if (!url.empty()) {
      std::string default_path = NetworkHandler::Get()
                                     ->network_state_handler()
                                     ->DefaultNetwork()
                                     ->path();
      ShillServiceClient::Get()->SetProperty(
          dbus::ObjectPath(default_path), shill::kProbeUrlProperty,
          base::Value(url), base::DoNothing(), base::DoNothing());
      base::RunLoop().RunUntilIdle();
      expected_url = url;
    } else {
      expected_url = captive_portal::CaptivePortalDetector::kDefaultURL;
    }
    return expected_url;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_helper_;
  std::unique_ptr<TestingProfileManager> test_profile_manager_;
  std::unique_ptr<TestSigninController> controller_;
};

TEST_F(NetworkPortalSigninControllerTest, LoginScreen) {
  controller_->ShowSignin();
  EXPECT_FALSE(controller_->dialog_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyTrue) {
  SimulateLogin();
  // kCaptivePortalAuthenticationIgnoresProxy defaults to true
  controller_->ShowSignin();
  EXPECT_FALSE(controller_->dialog_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyFalse) {
  SimulateLogin();
  test_profile_manager_->profile_manager()
      ->GetActiveUserProfile()
      ->GetPrefs()
      ->SetBoolean(prefs::kCaptivePortalAuthenticationIgnoresProxy, false);
  controller_->ShowSignin();
  EXPECT_FALSE(controller_->tab_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, ProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  controller_->ShowSignin();
  EXPECT_EQ(controller_->dialog_url(), expected_url);
}

TEST_F(NetworkPortalSigninControllerTest, NoProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(std::string());
  controller_->ShowSignin();
  EXPECT_EQ(controller_->dialog_url(), expected_url);
}

}  // namespace ash
