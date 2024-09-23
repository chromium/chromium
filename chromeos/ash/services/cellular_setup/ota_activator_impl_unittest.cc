// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/ota_activator_impl.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/network/fake_network_activation_handler.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/cellular_setup/public/cpp/fake_activation_delegate.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::cellular_setup {

namespace {

const char kTestCellularDevicePath[] = "/device/wwan0";
const char kTestCellularDeviceName[] = "testDeviceName";
const char kTestCellularDeviceCarrier[] = "testDeviceCarrier";
const char kTestCellularDeviceMeid[] = "testDeviceMeid";
const char kTestCellularDeviceImei[] = "testDeviceImei";
const char kTestCellularDeviceMdn[] = "testDeviceMdn";

const char kTestCellularServicePath[] = "/service/cellular0";
const char kTestCellularServiceIccid[] = "1234567890";
const char kTestCellularServiceGuid[] = "testServiceGuid";
const char kTestCellularServiceName[] = "testServiceName";
const char kTestCellularServicePaymentUrl[] = "testServicePaymentUrl.com";
const char kTestCellularServicePaymentPostData[] = "testServicePostData";

const char kPaymentPortalMethodPost[] = "POST";

}  // namespace

class CellularSetupOtaActivatorImplTest : public testing::Test {
 public:
  CellularSetupOtaActivatorImplTest(const CellularSetupOtaActivatorImplTest&) =
      delete;
  CellularSetupOtaActivatorImplTest& operator=(
      const CellularSetupOtaActivatorImplTest&) = delete;

 protected:
  CellularSetupOtaActivatorImplTest()
      : test_helper_(/*use_default_devices_and_services=*/false) {}
  ~CellularSetupOtaActivatorImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_activation_delegate_ = std::make_unique<FakeActivationDelegate>();
    fake_network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    fake_network_activation_handler_ =
        std::make_unique<FakeNetworkActivationHandler>();
  }

  void BuildOtaActivator() {
    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    ota_activator_ = OtaActivatorImpl::Factory::Create(
        fake_activation_delegate_->GenerateRemote(),
        base::BindOnce(&CellularSetupOtaActivatorImplTest::OnFinished,
                       base::Unretained(this)),
        test_helper_.network_state_handler(),
        fake_network_connection_handler_.get(),
        fake_network_activation_handler_.get(), test_task_runner);
    test_task_runner->RunUntilIdle();
    carrier_portal_handler_remote_.Bind(ota_activator_->GenerateRemote());
  }

  void AddCellularDevice(bool has_valid_sim, bool has_physical_slots = true) {
    ShillDeviceClient::TestInterface* device_test = test_helper_.device_test();
    device_test->AddDevice(kTestCellularDevicePath, shill::kTypeCellular,
                           kTestCellularDeviceName);

    if (has_valid_sim) {
      device_test->SetDeviceProperty(
          kTestCellularDevicePath, shill::kSIMPresentProperty,
          base::Value(true), false /* notify_changed */);

      device_test->SetDeviceProperty(
          kTestCellularDevicePath, shill::kSIMSlotInfoProperty,
          CreateCellularSIMSlotInfo(kTestCellularServiceIccid),
          false /* notify_changed */);

      base::Value::Dict home_provider;
      home_provider.Set(shill::kOperatorNameKey, kTestCellularDeviceCarrier);
      device_test->SetDeviceProperty(
          kTestCellularDevicePath, shill::kHomeProviderProperty,
          base::Value(std::move(home_provider)), false /* notify_changed */);
      device_test->SetDeviceProperty(
          kTestCellularDevicePath, shill::kMeidProperty,
          base::Value(kTestCellularDeviceMeid), false /* notify_changed */);
      device_test->SetDeviceProperty(
          kTestCellularDevicePath, shill::kImeiProperty,
          base::Value(kTestCellularDeviceImei), false /* notify_changed */);
      device_test->SetDeviceProperty(
          kTestCellularDevicePath, shill::kMdnProperty,
          base::Value(kTestCellularDeviceMdn), false /* notify_changed */);
    } else {
      std::string eid = has_physical_slots ? std::string() : "test_eid";
      device_test->SetDeviceProperty(
          kTestCellularDevicePath, shill::kSIMSlotInfoProperty,
          CreateCellularSIMSlotInfo(kTestCellularServiceIccid, eid),
          false /* notify_changed */);
    }

    base::RunLoop().RunUntilIdle();
  }

  void RemoveCellularDevice() {
    test_helper_.device_test()->RemoveDevice(kTestCellularDevicePath);
    base::RunLoop().RunUntilIdle();
  }

  void AddCellularNetwork(bool has_valid_payment_info,
                          bool is_connected,
                          bool is_already_activated) {
    ShillServiceClient::TestInterface* service_test =
        test_helper_.service_test();
    service_test->AddService(
        kTestCellularServicePath, kTestCellularServiceGuid,
        kTestCellularServiceName, shill::kTypeCellular,
        is_connected ? shill::kStateOnline : shill::kStateIdle, true);
    service_test->SetServiceProperty(kTestCellularServicePath,
                                     shill::kIccidProperty,
                                     base::Value(kTestCellularServiceIccid));
    service_test->SetServiceProperty(
        kTestCellularServicePath, shill::kActivationStateProperty,
        is_already_activated
            ? base::Value(shill::kActivationStateActivated)
            : base::Value(shill::kActivationStateNotActivated));

    if (has_valid_payment_info) {
      base::Value::Dict payment_portal;
      payment_portal.Set(shill::kPaymentPortalURL,
                         kTestCellularServicePaymentUrl);
      payment_portal.Set(shill::kPaymentPortalMethod, kPaymentPortalMethodPost);
      payment_portal.Set(shill::kPaymentPortalPostData,
                         kTestCellularServicePaymentPostData);
      service_test->SetServiceProperty(kTestCellularServicePath,
                                       shill::kPaymentPortalProperty,
                                       base::Value(std::move(payment_portal)));
    }

    base::RunLoop().RunUntilIdle();
  }

  void RemoveCellularNetwork() {
    test_helper_.service_test()->RemoveService(kTestCellularServicePath);
    base::RunLoop().RunUntilIdle();
  }

  void FlushForTesting() {
    static_cast<OtaActivatorImpl*>(ota_activator_.get())->FlushForTesting();
  }

  void VerifyCellularMetadataReceivedByDelegate() {
    const std::vector<mojom::CellularMetadataPtr>& cellular_metadata_list =
        fake_activation_delegate_->cellular_metadata_list();
    ASSERT_EQ(1u, cellular_metadata_list.size());

    EXPECT_EQ(GURL(kTestCellularServicePaymentUrl),
              cellular_metadata_list[0]->payment_url);
    EXPECT_EQ(kTestCellularDeviceCarrier, cellular_metadata_list[0]->carrier);
    EXPECT_EQ(kTestCellularDeviceMeid, cellular_metadata_list[0]->meid);
    EXPECT_EQ(kTestCellularDeviceImei, cellular_metadata_list[0]->imei);
    EXPECT_EQ(kTestCellularDeviceMdn, cellular_metadata_list[0]->mdn);
  }

  void UpdateCarrierPortalState(
      mojom::CarrierPortalStatus carrier_portal_status) {
    carrier_portal_handler_remote_->OnCarrierPortalStatusChange(
        carrier_portal_status);
    carrier_portal_handler_remote_.FlushForTesting();
  }

  void ConnectCellularNetwork() {
    const std::vector<FakeNetworkConnectionHandler::ConnectionParams>&
        connect_calls = fake_network_connection_handler_->connect_calls();
    ASSERT_FALSE(connect_calls.empty());

    // A connection should have been requested by |ota_activator_|.
    EXPECT_EQ(kTestCellularServicePath, connect_calls.back().service_path());

    // Simulate the connection succeeding.
    test_helper_.service_test()->SetServiceProperty(
        kTestCellularServicePath, shill::kStateProperty,
        base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  void FailCellularNetworkConnect() {
    std::vector<FakeNetworkConnectionHandler::ConnectionParams>& connect_calls =
        fake_network_connection_handler_->connect_calls();
    ASSERT_FALSE(connect_calls.empty());

    // A connection should have been requested by |ota_activator_|.
    EXPECT_EQ(kTestCellularServicePath, connect_calls.back().service_path());
    EXPECT_EQ(ConnectCallbackMode::ON_COMPLETED,
              connect_calls.back().connect_callback_mode());
    connect_calls.back().InvokeErrorCallback("fake_error");
    base::RunLoop().RunUntilIdle();
  }

  size_t ConnectCallCount() {
    return fake_network_connection_handler_->connect_calls().size();
  }

  void InvokePendingActivationCallback(bool success) {
    std::vector<FakeNetworkActivationHandler::ActivationParams>&
        complete_activation_calls =
            fake_network_activation_handler_->complete_activation_calls();
    ASSERT_EQ(1u, complete_activation_calls.size());
    EXPECT_EQ(kTestCellularServicePath,
              complete_activation_calls[0].service_path());

    if (success) {
      complete_activation_calls[0].InvokeSuccessCallback();
    } else {
      complete_activation_calls[0].InvokeErrorCallback("error");
    }
  }

  void VerifyActivationFinished(mojom::ActivationResult activation_result) {
    const std::vector<mojom::ActivationResult>& activation_results =
        fake_activation_delegate_->activation_results();
    ASSERT_EQ(1u, activation_results.size());
    EXPECT_EQ(activation_result, activation_results[0]);

    histogram_tester_.ExpectBucketCount(
        "Network.Cellular.PSim.OtaActivationResult", activation_result,
        /*expected_count=*/1);

    EXPECT_TRUE(is_finished_);
  }

  void DisconnectDelegate() {
    fake_activation_delegate_->DisconnectReceivers();
  }

  void FastForwardByConnectRetryDelay() {
    task_environment_.FastForwardBy(OtaActivatorImpl::kConnectRetryDelay);
  }

  bool is_finished() { return is_finished_; }

 private:
  void OnFinished() {
    EXPECT_FALSE(is_finished_);
    is_finished_ = true;
  }

  base::Value CreateCellularSIMSlotInfo(
      const std::string& iccid,
      const std::string& eid = std::string()) {
    base::Value::List sim_slot_infos;
    base::Value::Dict slot_info_item;
    slot_info_item.Set(shill::kSIMSlotInfoEID, eid);
    slot_info_item.Set(shill::kSIMSlotInfoICCID, iccid);
    slot_info_item.Set(shill::kSIMSlotInfoPrimary, false);
    sim_slot_infos.Append(std::move(slot_info_item));
    return base::Value(std::move(sim_slot_infos));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NetworkStateTestHelper test_helper_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<FakeActivationDelegate> fake_activation_delegate_;
  std::unique_ptr<FakeNetworkConnectionHandler>
      fake_network_connection_handler_;
  std::unique_ptr<FakeNetworkActivationHandler>
      fake_network_activation_handler_;

  std::unique_ptr<OtaActivator> ota_activator_;
  mojo::Remote<mojom::CarrierPortalHandler> carrier_portal_handler_remote_;

  bool is_finished_ = false;
};

TEST_F(CellularSetupOtaActivatorImplTest, Success) {
  AddCellularDevice(true /* has_valid_sim */);
  AddCellularNetwork(true /* has_valid_payment_info */, true /* is_connected */,
                     false /* is_already_activated */);

  BuildOtaActivator();

  FlushForTesting();
  VerifyCellularMetadataReceivedByDelegate();

  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedWithoutPaidUser);
  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedAndUserCompletedPayment);

  InvokePendingActivationCallback(true /* success */);

  FlushForTesting();
  VerifyActivationFinished(
      mojom::ActivationResult::kSuccessfullyStartedActivation);
}

TEST_F(CellularSetupOtaActivatorImplTest, SuccessWithPaymentError) {
  AddCellularDevice(true /* has_valid_sim */);
  AddCellularNetwork(true /* has_valid_payment_info */, true /* is_connected */,
                     false /* is_already_activated */);

  BuildOtaActivator();

  FlushForTesting();
  VerifyCellularMetadataReceivedByDelegate();

  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedWithoutPaidUser);
  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedButErrorOccurredDuringPayment);

  InvokePendingActivationCallback(true /* success */);

  FlushForTesting();
  VerifyActivationFinished(
      mojom::ActivationResult::kSuccessfullyStartedActivation);
}

TEST_F(CellularSetupOtaActivatorImplTest, HasNoPhysicalSlots) {
  AddCellularDevice(false /* has_valid_sim */, false /* has_physical_slots */);

  BuildOtaActivator();

  FlushForTesting();

  VerifyActivationFinished(mojom::ActivationResult::kFailedToActivate);
}

TEST_F(CellularSetupOtaActivatorImplTest, ConnectRetry) {
  AddCellularDevice(true /* has_valid_sim */);
  AddCellularNetwork(true /* has_valid_payment_info */,
                     false /* is_connected */,
                     false /* is_already_activated */);

  BuildOtaActivator();

  EXPECT_EQ(1u, ConnectCallCount());
  FailCellularNetworkConnect();
  // Ensure the a new connect call is not issued immediately.
  EXPECT_EQ(1u, ConnectCallCount());

  FastForwardByConnectRetryDelay();
  EXPECT_EQ(2u, ConnectCallCount());

  ConnectCellularNetwork();
  FlushForTesting();
  VerifyCellularMetadataReceivedByDelegate();

  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedWithoutPaidUser);
  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedAndUserCompletedPayment);

  InvokePendingActivationCallback(true /* success */);

  FlushForTesting();
  VerifyActivationFinished(
      mojom::ActivationResult::kSuccessfullyStartedActivation);
}

TEST_F(CellularSetupOtaActivatorImplTest,
       SimAndPaymentInfoNotInitiallyPresent_AndNetworkNotConnected) {
  AddCellularDevice(false /* has_valid_sim */);
  AddCellularNetwork(false /* has_valid_payment_info */,
                     false /* is_connected */,
                     false /* is_already_activated */);

  BuildOtaActivator();

  RemoveCellularDevice();
  RemoveCellularNetwork();

  AddCellularDevice(true /* has_valid_sim */);
  AddCellularNetwork(true /* has_valid_payment_info */,
                     false /* is_connected */,
                     false /* is_already_activated */);

  ConnectCellularNetwork();

  FlushForTesting();
  VerifyCellularMetadataReceivedByDelegate();

  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedWithoutPaidUser);
  UpdateCarrierPortalState(
      mojom::CarrierPortalStatus::kPortalLoadedAndUserCompletedPayment);

  InvokePendingActivationCallback(true /* success */);

  FlushForTesting();
  VerifyActivationFinished(
      mojom::ActivationResult::kSuccessfullyStartedActivation);
}

TEST_F(CellularSetupOtaActivatorImplTest, AlreadyActivated) {
  AddCellularDevice(true /* has_valid_sim */);
  AddCellularNetwork(true /* has_valid_payment_info */, true /* is_connected */,
                     true /* is_already_activated */);

  BuildOtaActivator();

  FlushForTesting();
  VerifyActivationFinished(mojom::ActivationResult::kAlreadyActivated);
}

TEST_F(CellularSetupOtaActivatorImplTest, DelegateBecomesDisconnected) {
  AddCellularDevice(true /* has_valid_sim */);
  AddCellularNetwork(true /* has_valid_payment_info */, true /* is_connected */,
                     false /* is_already_activated */);

  BuildOtaActivator();
  DisconnectDelegate();
  FlushForTesting();

  // Note: Cannot check the ActivationResult received by the delegate because
  // the delegate was disconnected and did not receive the result.
  EXPECT_TRUE(is_finished());
}

}  // namespace ash::cellular_setup
