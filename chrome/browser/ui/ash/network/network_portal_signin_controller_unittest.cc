// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
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
    profile_ = profile;
    dialog_url_ = url.spec();
  }
  void ShowTab(Profile* profile, const GURL& url) override {
    profile_ = profile;
    tab_url_ = url.spec();
  }

  Profile* profile() const { return profile_; }
  const std::string& dialog_url() const { return dialog_url_; }
  const std::string& tab_url() const { return tab_url_; }

 private:
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
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

    CHECK(test_profile_manager_.SetUp());
    user_manager_ = std::make_unique<FakeChromeUserManager>();
    user_manager_->Initialize();
    task_environment_.RunUntilIdle();

    // Initialize ProfileHelper.
    // TODO(crbug.com/1325210): Migrate it into BrowserContextHelper.
    ProfileHelper::Get();

    // Set ethernet to idle.
    network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                        shill::kStateProperty,
                                        base::Value(shill::kStateIdle));

    // Set WiFi (now the default) to redirect-found.
    network_helper_->SetServiceProperty(
        GetDefaultNetwork().path(), shill::kStateProperty,
        base::Value(shill::kStateRedirectFound));
  }

  void TearDown() override {
    user_manager_->Shutdown();
    user_manager_->Destroy();
    user_manager_.reset();
    test_profile_manager_.DeleteAllTestingProfiles();
    network_helper_.reset();
  }

 protected:
  void SimulateLogin() {
    const AccountId test_account_id(
        AccountId::FromUserEmail("test_user@gmail.com"));
    test_profile_manager_.CreateTestingProfile(test_account_id.GetUserEmail());

    user_manager_->AddUser(test_account_id);
    user_manager_->LoginUser(test_account_id);
    user_manager_->SwitchActiveUser(test_account_id);
  }

  void SimulateLoginAsGuest() {
    user_manager::User* user = user_manager_->AddGuestUser();
    Profile* profile = test_profile_manager_.CreateTestingProfile(
        user->GetAccountId().GetUserEmail());

    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    user_manager_->LoginUser(user->GetAccountId());
    user_manager_->SwitchActiveUser(user->GetAccountId());
    task_environment_.RunUntilIdle();
  }

  PrefService* GetPrefs() {
    PrefService* prefs = test_profile_manager_.profile_manager()
                             ->GetActiveUserProfile()
                             ->GetPrefs();
    DCHECK(prefs);
    return prefs;
  }

  const NetworkState& GetDefaultNetwork() {
    const NetworkState* network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    DCHECK(network);
    return *network;
  }

  std::string SetProbeUrl(const std::string& url) {
    std::string expected_url;
    if (!url.empty()) {
      network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                          shill::kProbeUrlProperty,
                                          base::Value(url));
      expected_url = url;
    } else {
      expected_url = captive_portal::CaptivePortalDetector::kDefaultURL;
    }
    return expected_url;
  }

  void SetNetworkProxy() {
    GetPrefs()->SetBoolean(::proxy_config::prefs::kUseSharedProxies, true);
    proxy_config::SetProxyConfigForNetwork(
        ProxyConfigDictionary(ProxyConfigDictionary::CreateAutoDetect()),
        GetDefaultNetwork());
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkProxyDirect() {
    GetPrefs()->SetBoolean(::proxy_config::prefs::kUseSharedProxies, true);
    proxy_config::SetProxyConfigForNetwork(
        ProxyConfigDictionary(ProxyConfigDictionary::CreateDirect()),
        GetDefaultNetwork());
    base::RunLoop().RunUntilIdle();
  }

  void ShowSignin(
      NetworkPortalSigninController::SigninSource source =
          NetworkPortalSigninController::SigninSource::kNotification) {
    // The SigninSource is only used for histograms.
    controller_->ShowSignin(source);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_helper_;
  std::unique_ptr<TestSigninController> controller_;
  std::unique_ptr<FakeChromeUserManager> user_manager_;
  TestingProfileManager test_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
};

TEST_F(NetworkPortalSigninControllerTest, LoginScreen) {
  ShowSignin();
  EXPECT_FALSE(controller_->dialog_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, KioskMode) {
  SimulateLogin();
  const user_manager::User* user = user_manager_->AddKioskAppUser(
      AccountId::FromUserEmail("fake_user@test"));
  user_manager_->LoginUser(user->GetAccountId());

  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyTrue) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  // kCaptivePortalAuthenticationIgnoresProxy defaults to true
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyFalse) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  GetPrefs()->SetBoolean(prefs::kCaptivePortalAuthenticationIgnoresProxy,
                         false);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_FALSE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, ProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, NoProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(std::string());
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
}

TEST_F(NetworkPortalSigninControllerTest, NoProxy) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, ProxyDirect) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxyDirect();
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest,
       AuthenticationIgnoresProxyFalseOTRDisabled) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  IncognitoModePrefs::SetAvailability(
      GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_FALSE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, ProxyPref) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  base::Value::Dict proxy_config;
  proxy_config.Set("mode", ProxyPrefs::kPacScriptProxyModeName);
  proxy_config.Set("pac_url", "http://proxy");
  GetPrefs()->SetDict(::proxy_config::prefs::kProxy, std::move(proxy_config));
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, IsNewOTRProfile) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  Profile* default_otr_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_NE(profile, default_otr_profile);
  ASSERT_TRUE(controller_->profile());
  EXPECT_NE(controller_->profile(), profile);
  EXPECT_NE(controller_->profile(), default_otr_profile);
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, GuestLogin) {
  SimulateLoginAsGuest();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest, NoNetwork) {
  SimulateLogin();
  // Set WiFi to idle
  network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                      shill::kStateProperty,
                                      base::Value(shill::kStateIdle));
  ShowSignin();
  EXPECT_TRUE(controller_->tab_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, NotInPortalState) {
  SimulateLogin();
  // Set WiFi to online
  network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                      shill::kStateProperty,
                                      base::Value(shill::kStateOnline));
  ShowSignin();
  EXPECT_TRUE(controller_->tab_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, Metrics) {
  base::HistogramTester histogram_tester;
  SimulateLogin();
  std::string expected_url = SetProbeUrl(std::string());
  ShowSignin(NetworkPortalSigninController::SigninSource::kSettings);
  EXPECT_EQ(controller_->tab_url(), expected_url);
  ASSERT_TRUE(controller_->profile());
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());

  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninMode", 1);
  histogram_tester.ExpectUniqueSample(
      "Network.NetworkPortalSigninMode",
      NetworkPortalSigninController::SigninMode::kIncognitoTab, 1);
  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninSource", 1);
  histogram_tester.ExpectUniqueSample(
      "Network.NetworkPortalSigninSource",
      NetworkPortalSigninController::SigninSource::kSettings, 1);
}

}  // namespace ash
