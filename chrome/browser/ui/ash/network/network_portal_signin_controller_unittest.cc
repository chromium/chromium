// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
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
  void ShowSigninDialog(const GURL& url) override {
    signin_dialog_url_ = url.spec();
    WaitForHistograms();
  }
  void ShowSigninWindow(const GURL& url) override {
    signin_window_url_ = url.spec();
    WaitForHistograms();
  }
  void ShowTab(Profile* profile, const GURL& url) override {
    incognito_ = profile && profile->IsOffTheRecord();
    tab_url_ = url.spec();
    WaitForHistograms();
  }
  void ShowActiveProfileTab(const GURL& url) override {
    incognito_ = false;
    tab_url_ = url.spec();
    WaitForHistograms();
  }

  const std::string& signin_dialog_url() const { return signin_dialog_url_; }
  const std::string& signin_window_url() const { return signin_window_url_; }
  const std::string& tab_url() const { return tab_url_; }
  bool incognito() const { return incognito_; }

 private:
  void WaitForHistograms() {
    // Create a delay simulating opening a window for histograms which use 1 ms
    // buckets.
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
    run_loop.Run();
  }

  std::string signin_dialog_url_;
  std::string signin_window_url_;
  std::string tab_url_;
  bool incognito_ = false;
};

}  // namespace

class NetworkPortalSigninControllerTest : public testing::TestWithParam<bool> {
 public:
  NetworkPortalSigninControllerTest() = default;
  NetworkPortalSigninControllerTest(const NetworkPortalSigninControllerTest&) =
      delete;
  NetworkPortalSigninControllerTest& operator=(
      const NetworkPortalSigninControllerTest&) = delete;
  ~NetworkPortalSigninControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatureState(
        chromeos::features::kCaptivePortalPopupWindow,
        CaptivePortalPopupWindowEnabled());

    network_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    controller_ = std::make_unique<TestSigninController>();

    CHECK(test_profile_manager_.SetUp());
    user_manager_ = std::make_unique<FakeChromeUserManager>();
    user_manager_->Initialize();
    task_environment_.RunUntilIdle();

    // Initialize ProfileHelper.
    // TODO(crbug.com/40225390): Migrate it into BrowserContextHelper.
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
    controller_.reset();
    test_profile_manager_.DeleteAllTestingProfiles();
    user_manager_->Shutdown();
    user_manager_->Destroy();
    user_manager_.reset();
    network_helper_.reset();
  }

  bool CaptivePortalPopupWindowEnabled() { return GetParam(); }

 protected:
  using SigninMode = NetworkPortalSigninController::SigninMode;

  void SimulateLogin() {
    const AccountId test_account_id(
        AccountId::FromUserEmail("test_user@gmail.com"));
    test_profile_manager_.CreateTestingProfile(test_account_id.GetUserEmail());

    user_manager_->AddUser(test_account_id);
    user_manager_->LoginUser(test_account_id);
    user_manager_->SwitchActiveUser(test_account_id);
  }

  void SimulateLoginAsUser(user_manager::User* user) {
    const AccountId account_id = user->GetAccountId();
    Profile* profile =
        test_profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    user_manager_->LoginUser(account_id);
    user_manager_->SwitchActiveUser(account_id);
    task_environment_.RunUntilIdle();
  }

  void SimulateLoginAsGuest() {
    user_manager::User* user = user_manager_->AddGuestUser();
    SimulateLoginAsUser(user);
  }

  void SimulateLoginAsKioskApp() {
    const AccountId account_id(
        AccountId::FromUserEmail("kiosk_app_user@gmail.com"));
    user_manager::User* user = user_manager_->AddKioskAppUser(account_id);
    SimulateLoginAsUser(user);
  }

  void SimulateLoginAsChild() {
    const AccountId account_id(
        AccountId::FromUserEmail("child_user@gmail.com"));
    user_manager::User* user = user_manager_->AddChildUser(account_id);
    SimulateLoginAsUser(user);
    user_manager_->set_current_user_child(true);
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

  bool IsWindowForSigninDefault(const std::string& url) {
    // When CaptivePortalPopupWindow is enabled, the signin window should be
    // set and the url set to the probe url.
    if (CaptivePortalPopupWindowEnabled()) {
      return controller_->signin_window_url() == url;
    }
    // Otherwise a normal window with an OTR profile should be used and the url
    // set to the probe url.
    return controller_->incognito() && controller_->tab_url() == url;
  }

  const std::string& DefaultUrl() {
    if (CaptivePortalPopupWindowEnabled()) {
      return controller_->signin_window_url();
    }
    return controller_->tab_url();
  }

  SigninMode GetSigninMode() {
    return controller_->GetSigninMode(GetDefaultNetwork().GetPortalState());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_helper_;
  std::unique_ptr<TestSigninController> controller_;
  std::unique_ptr<FakeChromeUserManager> user_manager_;
  TestingProfileManager test_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(NetworkPortalSigninControllerTest, LoginScreen) {
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDialog);
  ShowSignin();
  EXPECT_FALSE(controller_->signin_dialog_url().empty());
}

TEST_P(NetworkPortalSigninControllerTest, KioskMode) {
  SimulateLoginAsKioskApp();

  SetNetworkProxy();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDialog);
  ShowSignin();
  EXPECT_FALSE(controller_->signin_dialog_url().empty());
}

TEST_P(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyTrue) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  // kCaptivePortalAuthenticationIgnoresProxy defaults to true
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDefault);
  ShowSignin();
  EXPECT_TRUE(IsWindowForSigninDefault(expected_url));
}

TEST_P(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyFalse) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  GetPrefs()->SetBoolean(
      chromeos::prefs::kCaptivePortalAuthenticationIgnoresProxy, false);
  EXPECT_EQ(GetSigninMode(), SigninMode::kNormalTab);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_FALSE(controller_->incognito());
}

TEST_P(NetworkPortalSigninControllerTest, ProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDefault);
  ShowSignin();
  EXPECT_TRUE(IsWindowForSigninDefault(expected_url));
}

TEST_P(NetworkPortalSigninControllerTest, NoProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(std::string());
  ShowSignin();
  EXPECT_EQ(DefaultUrl(), expected_url);
}

TEST_P(NetworkPortalSigninControllerTest, NoProxy) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDefault);
  ShowSignin();
  EXPECT_TRUE(IsWindowForSigninDefault(expected_url));
}

TEST_P(NetworkPortalSigninControllerTest, ProxyDirect) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxyDirect();
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDefault);
  ShowSignin();
  EXPECT_TRUE(IsWindowForSigninDefault(expected_url));
}

TEST_P(NetworkPortalSigninControllerTest, IncognitoDisabledByPolicy) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  IncognitoModePrefs::SetAvailability(
      GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  EXPECT_EQ(GetSigninMode(), SigninMode::kIncognitoDisabledByPolicy);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_FALSE(controller_->incognito());
}

TEST_P(NetworkPortalSigninControllerTest,
       IncognitoDisabledByParentialControls) {
  SimulateLoginAsChild();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  EXPECT_EQ(GetSigninMode(), SigninMode::kIncognitoDisabledByParentalControls);
  ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_FALSE(controller_->incognito());
}

TEST_P(NetworkPortalSigninControllerTest, ProxyPref) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  base::Value::Dict proxy_config;
  proxy_config.Set("mode", ProxyPrefs::kPacScriptProxyModeName);
  proxy_config.Set("pac_url", "http://proxy");
  GetPrefs()->SetDict(::proxy_config::prefs::kProxy, std::move(proxy_config));
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDefault);
  ShowSignin();
  EXPECT_TRUE(IsWindowForSigninDefault(expected_url));
}

TEST_P(NetworkPortalSigninControllerTest, IsNewSigninProfile) {
  if (CaptivePortalPopupWindowEnabled()) {
    return;  // The portal signin profile is always used.
  }
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  ShowSignin();
  EXPECT_EQ(DefaultUrl(), expected_url);
  EXPECT_TRUE(controller_->incognito());
}

TEST_P(NetworkPortalSigninControllerTest, GuestLogin) {
  SimulateLoginAsGuest();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  EXPECT_EQ(GetSigninMode(), SigninMode::kSigninDefault);
  ShowSignin();
  EXPECT_TRUE(IsWindowForSigninDefault(expected_url));
}

TEST_P(NetworkPortalSigninControllerTest, NoNetwork) {
  SimulateLogin();
  // Set WiFi to idle
  network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                      shill::kStateProperty,
                                      base::Value(shill::kStateIdle));
  ShowSignin();
  EXPECT_TRUE(DefaultUrl().empty());
}

TEST_P(NetworkPortalSigninControllerTest, NotInPortalState) {
  SimulateLogin();
  // Set WiFi to online
  network_helper_->SetServiceProperty(GetDefaultNetwork().path(),
                                      shill::kStateProperty,
                                      base::Value(shill::kStateOnline));
  ShowSignin();
  EXPECT_TRUE(DefaultUrl().empty());
}

TEST_P(NetworkPortalSigninControllerTest, Metrics) {
  base::HistogramTester histogram_tester;
  SimulateLogin();
  std::string expected_url = SetProbeUrl(std::string());
  ShowSignin(NetworkPortalSigninController::SigninSource::kSettings);
  EXPECT_TRUE(IsWindowForSigninDefault(expected_url));

  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninMode", 1);
  histogram_tester.ExpectUniqueSample(
      "Network.NetworkPortalSigninMode",
      NetworkPortalSigninController::SigninMode::kSigninDefault, 1);
  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninSource", 1);
  histogram_tester.ExpectUniqueSample(
      "Network.NetworkPortalSigninSource",
      NetworkPortalSigninController::SigninSource::kSettings, 1);
  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninTime", 0);

  // Set WiFi to online
  std::string wifi_path = GetDefaultNetwork().path();
  network_helper_->SetServiceProperty(wifi_path, shill::kStateProperty,
                                      base::Value(shill::kStateOnline));
  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninTime", 1);
  // Entry should not be in the 0 bucket.
  histogram_tester.ExpectTimeBucketCount("Network.NetworkPortalSigninTime",
                                         base::TimeDelta(), 0);
  // Set WiFi to idle, no additional SigninTime metric should occur.
  network_helper_->SetServiceProperty(wifi_path, shill::kStateProperty,
                                      base::Value(shill::kStateIdle));
  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninTime", 1);

  // Set WiFi to portal, show the signin page, than set it to idle.
  // An entry in the 0 bucket should occur.
  network_helper_->SetServiceProperty(wifi_path, shill::kStateProperty,
                                      base::Value(shill::kStateRedirectFound));
  ShowSignin(NetworkPortalSigninController::SigninSource::kSettings);
  network_helper_->SetServiceProperty(wifi_path, shill::kStateProperty,
                                      base::Value(shill::kStateIdle));
  histogram_tester.ExpectTotalCount("Network.NetworkPortalSigninTime", 2);
  histogram_tester.ExpectTimeBucketCount("Network.NetworkPortalSigninTime",
                                         base::TimeDelta(), 1);
}

INSTANTIATE_TEST_SUITE_P(NetworkPortalSigninControllerTests,
                         NetworkPortalSigninControllerTest,
                         ::testing::Values(false, true));

}  // namespace ash
