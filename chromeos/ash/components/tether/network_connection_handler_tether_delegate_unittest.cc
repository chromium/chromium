// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/network_connection_handler_tether_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_tether_connector.h"
#include "chromeos/ash/components/tether/fake_tether_disconnector.h"
#include "chromeos/ash/components/tether/tether_disconnector.h"
#include "chromeos/ash/components/tether/tether_session_completion_logger.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

const char kSuccessResult[] = "success";

// Does nothing when a connection is requested.
class DummyTetherConnector : public FakeTetherConnector {
 public:
  // TetherConnector:
  void ConnectToNetwork(const std::string& tether_network_guid,
                        base::OnceClosure success_callback,
                        StringErrorCallback error_callback) override {}
};

// Does nothing when a disconnection is requested.
class DummyTetherDisconnector : public FakeTetherDisconnector {
 public:
  // TetherDisconnector:
  void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      base::OnceClosure success_callback,
      StringErrorCallback error_callback,
      const TetherSessionCompletionLogger::SessionCompletionReason&
          session_completion_reason) override {}
};

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  TestNetworkConnectionHandler() : NetworkConnectionHandler() {}
  ~TestNetworkConnectionHandler() override = default;

  void CallTetherConnect(const std::string& tether_network_guid,
                         base::OnceClosure success_callback,
                         network_handler::ErrorCallback error_callback) {
    InitiateTetherNetworkConnection(tether_network_guid,
                                    std::move(success_callback),
                                    std::move(error_callback));
  }

  void CallTetherDisconnect(const std::string& tether_network_guid,
                            base::OnceClosure success_callback,
                            network_handler::ErrorCallback error_callback) {
    InitiateTetherNetworkDisconnection(tether_network_guid,
                                       std::move(success_callback),
                                       std::move(error_callback));
  }

  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        base::OnceClosure success_callback,
                        network_handler::ErrorCallback error_callback,
                        bool check_error_state,
                        ConnectCallbackMode mode) override {}

  void DisconnectNetwork(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) override {}

  void Init(
      NetworkStateHandler* network_state_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      CellularConnectionHandler* cellular_connection_handler) override {}

  void OnAutoConnectedInitiated(int auto_connect_reasons) override {}
};

}  // namespace

class NetworkConnectionHandlerTetherDelegateTest : public testing::Test {
 public:
  NetworkConnectionHandlerTetherDelegateTest(
      const NetworkConnectionHandlerTetherDelegateTest&) = delete;
  NetworkConnectionHandlerTetherDelegateTest& operator=(
      const NetworkConnectionHandlerTetherDelegateTest&) = delete;

 protected:
  NetworkConnectionHandlerTetherDelegateTest() = default;

  void SetUp() override {
    result_.clear();

    test_network_connection_handler_ =
        base::WrapUnique(new TestNetworkConnectionHandler());

    fake_active_host_ = std::make_unique<FakeActiveHost>();
    fake_tether_connector_ = std::make_unique<FakeTetherConnector>();
    fake_tether_disconnector_ = std::make_unique<FakeTetherDisconnector>();

    delegate_ = std::make_unique<NetworkConnectionHandlerTetherDelegate>(
        test_network_connection_handler_.get(), fake_active_host_.get(),
        fake_tether_connector_.get(), fake_tether_disconnector_.get());
  }

  void TearDown() override {
    // No more callbacks should occur after deletion.
    delegate_.reset();
    EXPECT_EQ(std::string(), GetResultAndReset());
  }

  void CallTetherConnect(const std::string& guid) {
    test_network_connection_handler_->CallTetherConnect(
        guid,
        base::BindOnce(&NetworkConnectionHandlerTetherDelegateTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&NetworkConnectionHandlerTetherDelegateTest::OnError,
                       base::Unretained(this)));
  }

  void CallTetherDisconnect(const std::string& guid) {
    test_network_connection_handler_->CallTetherDisconnect(
        guid,
        base::BindOnce(&NetworkConnectionHandlerTetherDelegateTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&NetworkConnectionHandlerTetherDelegateTest::OnError,
                       base::Unretained(this)));
  }

  void OnSuccess() { result_ = kSuccessResult; }

  void OnError(const std::string& error) { result_ = error; }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(result_);
    return result;
  }

  std::unique_ptr<TestNetworkConnectionHandler>
      test_network_connection_handler_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeTetherConnector> fake_tether_connector_;
  std::unique_ptr<FakeTetherDisconnector> fake_tether_disconnector_;

  std::string result_;

  std::unique_ptr<NetworkConnectionHandlerTetherDelegate> delegate_;
};

TEST_F(NetworkConnectionHandlerTetherDelegateTest,
       TestConnect_NotAlreadyConnected) {
  CallTetherConnect("tetherNetworkGuid");
  EXPECT_EQ("tetherNetworkGuid",
            fake_tether_connector_->last_connected_tether_network_guid());
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTetherDelegateTest,
       TestConnect_AlreadyConnectedToSameDevice) {
  fake_active_host_->SetActiveHostConnected("activeHostId", "tetherNetworkGuid",
                                            "wifiNetworkGuid");
  CallTetherConnect("tetherNetworkGuid");
  EXPECT_TRUE(
      fake_tether_connector_->last_connected_tether_network_guid().empty());
  EXPECT_TRUE(fake_tether_disconnector_->last_disconnected_tether_network_guid()
                  .empty());
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnected, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTetherDelegateTest,
       TestConnect_AlreadyConnectedToDifferentDevice) {
  fake_active_host_->SetActiveHostConnected("activeHostId", "tetherNetworkGuid",
                                            "wifiNetworkGuid");

  CallTetherConnect("newTetherNetworkGuid");
  EXPECT_EQ("tetherNetworkGuid",
            fake_tether_disconnector_->last_disconnected_tether_network_guid());
  EXPECT_EQ("newTetherNetworkGuid",
            fake_tether_connector_->last_connected_tether_network_guid());
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTetherDelegateTest, TestDisconnect) {
  CallTetherDisconnect("tetherNetworkGuid");
  EXPECT_EQ("tetherNetworkGuid",
            fake_tether_disconnector_->last_disconnected_tether_network_guid());
  EXPECT_EQ(
      TetherSessionCompletionLogger::SessionCompletionReason::USER_DISCONNECTED,
      *fake_tether_disconnector_->last_session_completion_reason());
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTetherDelegateTest,
       TestPendingCallbacksInvokedWhenDeleted) {
  delegate_.reset();

  // Use "dummy" connector/disconnector.
  std::unique_ptr<DummyTetherConnector> dummy_connector =
      base::WrapUnique(new DummyTetherConnector());
  std::unique_ptr<DummyTetherDisconnector> dummy_disconnector =
      base::WrapUnique(new DummyTetherDisconnector());

  test_network_connection_handler_ =
      base::WrapUnique(new TestNetworkConnectionHandler());
  delegate_ = std::make_unique<NetworkConnectionHandlerTetherDelegate>(
      test_network_connection_handler_.get(), fake_active_host_.get(),
      dummy_connector.get(), dummy_disconnector.get());

  CallTetherConnect("tetherNetworkGuid");

  // No callbacks should have been invoked.
  EXPECT_TRUE(result_.empty());

  // Now, delete the delegate. It should fire the error callback.
  delegate_.reset();
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
}

}  // namespace tether

}  // namespace ash
