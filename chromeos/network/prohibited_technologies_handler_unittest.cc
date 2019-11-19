// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/prohibited_technologies_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/onc/onc_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class ProhibitedTechnologiesHandlerTest : public testing::Test {
 public:
  ProhibitedTechnologiesHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    LoginState::Initialize();

    helper_.manager_test()->AddTechnology(shill::kTypeCellular,
                                          true /* enabled */);

    network_config_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            helper_.network_state_handler(),
            nullptr /* network_device_handler */));

    network_profile_handler_.reset(new NetworkProfileHandler());
    network_profile_handler_->Init();

    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    prohibited_technologies_handler_.reset(new ProhibitedTechnologiesHandler());

    managed_config_handler_->Init(
        helper_.network_state_handler(), network_profile_handler_.get(),
        network_config_handler_.get(), nullptr /* network_device_handler */,
        prohibited_technologies_handler_.get());

    prohibited_technologies_handler_->Init(managed_config_handler_.get(),
                                           helper_.network_state_handler());

    base::RunLoop().RunUntilIdle();

    PreparePolicies();
  }

  void PreparePolicies() {
    std::unique_ptr<base::ListValue> val(new base::ListValue());
    val->AppendString("WiFi");
    global_config_disable_wifi.Set("DisableNetworkTypes", std::move(val));
    val.reset(new base::ListValue());
    val->AppendString("WiFi");
    val->AppendString("Cellular");
    global_config_disable_wifi_and_cell.Set("DisableNetworkTypes",
                                            std::move(val));
  }

  void TearDown() override {
    prohibited_technologies_handler_.reset();
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_config_handler_.reset();
    LoginState::Shutdown();
  }

 protected:
  void LoginToRegularUser() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
    base::RunLoop().RunUntilIdle();
  }

  void SetupPolicy(const base::DictionaryValue& global_config,
                   bool user_policy) {
    if (user_policy) {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
                                         helper_.UserHash(), base::ListValue(),
                                         global_config);
    } else {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                                         std::string(),  // no username hash
                                         base::ListValue(), global_config);
    }
    base::RunLoop().RunUntilIdle();
  }

  NetworkStateHandler* network_state_handler() {
    return helper_.network_state_handler();
  }

  base::DictionaryValue global_config_disable_wifi;
  base::DictionaryValue global_config_disable_wifi_and_cell;
  std::unique_ptr<ProhibitedTechnologiesHandler>
      prohibited_technologies_handler_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};

  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;

  DISALLOW_COPY_AND_ASSIGN(ProhibitedTechnologiesHandlerTest);
};

TEST_F(ProhibitedTechnologiesHandlerTest,
       ProhibitedTechnologiesAllowedLoginScreen) {
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));
  SetupPolicy(global_config_disable_wifi_and_cell, false);
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));
}

TEST_F(ProhibitedTechnologiesHandlerTest,
       ProhibitedTechnologiesNotAllowedUserSession) {
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));
  SetupPolicy(global_config_disable_wifi_and_cell, false);

  LoginToRegularUser();
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  SetupPolicy(base::DictionaryValue(), true);  // wait for user policy

  // Should be disabled after logged in
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  // Can not enable it back
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), true, network_handler::ErrorCallback());
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Cellular(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  // Can enable Cellular back after modifying policy
  SetupPolicy(global_config_disable_wifi, false);
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), true, network_handler::ErrorCallback());
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Cellular(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));
}

TEST_F(ProhibitedTechnologiesHandlerTest,
       IsGloballyProhibitedTechnologyWorksAfterReenabling) {
  LoginToRegularUser();
  SetupPolicy(base::DictionaryValue(), true);  // wait for user policy

  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  prohibited_technologies_handler_->AddGloballyProhibitedTechnology(
      shill::kTypeWifi);
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // Enabling it back
  prohibited_technologies_handler_->RemoveGloballyProhibitedTechnology(
      shill::kTypeWifi);
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
}

TEST_F(ProhibitedTechnologiesHandlerTest,
       InteractionBetweenGloballyAndSessionsProhibitedTechnologies) {
  // Set session prohibiting of Cellular, should not prohibit on login screen
  SetupPolicy(global_config_disable_wifi_and_cell, false);
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  LoginToRegularUser();
  SetupPolicy(base::DictionaryValue(), true);  // receive user policy
  // Cellular should be prohibited
  EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  // Should be prohibited after adding to globally prohibited list
  prohibited_technologies_handler_->AddGloballyProhibitedTechnology(
      shill::kTypeCellular);
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Cellular(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  // Should be prohibited after removing from globally prohibited list
  prohibited_technologies_handler_->RemoveGloballyProhibitedTechnology(
      shill::kTypeCellular);
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Cellular(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  // Should not be prohibited after updating session prohibited list.
  SetupPolicy(global_config_disable_wifi, false);
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Cellular(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));
}

}  // namespace chromeos
