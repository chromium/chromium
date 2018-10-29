// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::flat_set<AuthenticatorTransport> kAllTransports = {
    AuthenticatorTransport::kUsbHumanInterfaceDevice,
    AuthenticatorTransport::kNearFieldCommunication,
    AuthenticatorTransport::kBluetoothLowEnergy,
    AuthenticatorTransport::kInternal,
    AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
};

const base::flat_set<AuthenticatorTransport> kAllTransportsWithoutCable = {
    AuthenticatorTransport::kUsbHumanInterfaceDevice,
    AuthenticatorTransport::kNearFieldCommunication,
    AuthenticatorTransport::kBluetoothLowEnergy,
    AuthenticatorTransport::kInternal,
};

using TransportAvailabilityInfo =
    ::device::FidoRequestHandlerBase::TransportAvailabilityInfo;

class MockDialogModelObserver
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  MockDialogModelObserver() = default;

  MOCK_METHOD0(OnModelDestroyed, void());
  MOCK_METHOD0(OnStepTransition, void());
  MOCK_METHOD0(OnCancelRequest, void());
  MOCK_METHOD0(OnBluetoothPoweredStateChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDialogModelObserver);
};

class BluetoothAdapterPowerOnCallbackReceiver {
 public:
  BluetoothAdapterPowerOnCallbackReceiver() = default;

  base::RepeatingClosure GetCallback() {
    return base::BindRepeating(
        &BluetoothAdapterPowerOnCallbackReceiver::CallbackTarget,
        base::Unretained(this));
  }

  bool was_called() const { return was_called_; }

 private:
  void CallbackTarget() {
    ASSERT_FALSE(was_called_);
    was_called_ = true;
  }

  bool was_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterPowerOnCallbackReceiver);
};

}  // namespace

class AuthenticatorRequestDialogModelTest : public ::testing::Test {
 public:
  using Step = AuthenticatorRequestDialogModel::Step;
  using RequestType = ::device::FidoRequestHandlerBase::RequestType;

  AuthenticatorRequestDialogModelTest() {}
  ~AuthenticatorRequestDialogModelTest() override {}

 protected:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModelTest);
};

TEST_F(AuthenticatorRequestDialogModelTest, TransportAutoSelection) {
  const struct {
    RequestType request_type;
    base::flat_set<AuthenticatorTransport> available_transports;
    base::Optional<AuthenticatorTransport> last_used_transport;
    bool has_touch_id_credential;
    Step expected_first_step;
  } kTestCases[] = {
      // Only a single transport is available for a GetAssertion call.
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       base::nullopt,
       false,
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kNearFieldCommunication},
       base::nullopt,
       false,
       Step::kTransportSelection},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kBleActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       AuthenticatorTransport::kUsbHumanInterfaceDevice,
       false,
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       base::nullopt,
       true,
       Step::kTouchId},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kCableActivate},

      // The last used transport is available (and caBLE is not).
      {RequestType::kGetAssertion, kAllTransportsWithoutCable,
       AuthenticatorTransport::kUsbHumanInterfaceDevice, false,
       Step::kUsbInsertAndActivate},

      // The last used transport is not available.
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kBluetoothLowEnergy,
        AuthenticatorTransport::kUsbHumanInterfaceDevice},
       AuthenticatorTransport::kNearFieldCommunication,
       false,
       Step::kTransportSelection},

      // The KeyChain contains an allowed Touch ID credential.
      {RequestType::kGetAssertion, kAllTransports,
       AuthenticatorTransport::kUsbHumanInterfaceDevice, true, Step::kTouchId},

      // The KeyChain does not contain an allowed Touch ID credential.
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       AuthenticatorTransport::kUsbHumanInterfaceDevice,
       false,
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       AuthenticatorTransport::kInternal,
       false,
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion, kAllTransportsWithoutCable, base::nullopt,
       false, Step::kTransportSelection},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal},
       AuthenticatorTransport::kUsbHumanInterfaceDevice,
       false,
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal},
       AuthenticatorTransport::kBluetoothLowEnergy,
       false,
       Step::kTransportSelection},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal},
       AuthenticatorTransport::kInternal,
       false,
       Step::kTransportSelection},

      // The KeyChain contains an allowed Touch ID credential, but Touch ID is
      // not enabled by the relying party.
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       base::nullopt,
       true,
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kBluetoothLowEnergy},
       base::nullopt,
       true,
       Step::kTransportSelection},

      // If caBLE is one of the allowed transports, it has second-highest
      // priority after Touch ID, and is auto-selected for GetAssertion
      // operations even if the last used transport was something else.
      {RequestType::kGetAssertion, kAllTransports, base::nullopt, false,
       Step::kCableActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
        AuthenticatorTransport::kUsbHumanInterfaceDevice},
       AuthenticatorTransport::kUsbHumanInterfaceDevice,
       false,
       Step::kCableActivate},

      // caBLE should not enjoy this same high priority for MakeCredential
      // calls.
      {RequestType::kMakeCredential, kAllTransports, base::nullopt, false,
       Step::kTransportSelection},

      // No transports available.
      {RequestType::kGetAssertion,
       {},
       AuthenticatorTransport::kNearFieldCommunication,
       false,
       Step::kErrorNoAvailableTransports},

      // Even when last transport used is available, we default to transport
      // selection modal for MakeCredential.
      {RequestType::kMakeCredential, kAllTransports,
       AuthenticatorTransport::kUsbHumanInterfaceDevice, false,
       Step::kTransportSelection},

      // When only one transport is available, we still want to skip transport
      // selection view for MakeCredential call.
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       base::nullopt,
       false,
       Step::kUsbInsertAndActivate},
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kInternal},
       base::nullopt,
       false,
       Step::kTouchId},
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kBleActivate},
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kCableActivate},
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       base::nullopt,
       false,
       Step::kUsbInsertAndActivate},
  };

  for (const auto& test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.is_ble_powered = true;
    transports_info.request_type = test_case.request_type;
    transports_info.available_transports = test_case.available_transports;
    transports_info.has_recognized_mac_touch_id_credential =
        test_case.has_touch_id_credential;

    AuthenticatorRequestDialogModel model;
    model.StartFlow(std::move(transports_info), test_case.last_used_transport);
    EXPECT_EQ(test_case.expected_first_step, model.current_step());

    if (model.current_step() == Step::kTransportSelection)
      continue;

    model.Back();
    EXPECT_EQ(Step::kTransportSelection, model.current_step());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, TransportList) {
  TransportAvailabilityInfo transports_info;
  transports_info.available_transports = kAllTransports;

  AuthenticatorRequestDialogModel model;
  model.StartFlow(std::move(transports_info), base::nullopt);
  EXPECT_THAT(model.available_transports(),
              ::testing::UnorderedElementsAre(
                  AuthenticatorTransport::kUsbHumanInterfaceDevice,
                  AuthenticatorTransport::kNearFieldCommunication,
                  AuthenticatorTransport::kBluetoothLowEnergy,
                  AuthenticatorTransport::kInternal,
                  AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy));
}

TEST_F(AuthenticatorRequestDialogModelTest, NoAvailableTransports) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model;
  model.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.StartFlow(TransportAvailabilityInfo(),
                  AuthenticatorTransport::kInternal);
  EXPECT_EQ(Step::kErrorNoAvailableTransports, model.current_step());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnCancelRequest());
  model.Cancel();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.OnRequestComplete();
  EXPECT_EQ(Step::kClosed, model.current_step());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnModelDestroyed());
}

TEST_F(AuthenticatorRequestDialogModelTest, PostMortems) {
  const struct {
    void (AuthenticatorRequestDialogModel::*event)();
    Step expected_post_mortem_sheet;
  } kTestCases[] = {
      {&AuthenticatorRequestDialogModel::OnRequestTimeout,
       Step::kPostMortemTimedOut},
      {&AuthenticatorRequestDialogModel::OnActivatedKeyNotRegistered,
       Step::kPostMortemKeyNotRegistered},
      {&AuthenticatorRequestDialogModel::OnActivatedKeyAlreadyRegistered,
       Step::kPostMortemKeyAlreadyRegistered},
  };

  for (const auto& test_case : kTestCases) {
    testing::StrictMock<MockDialogModelObserver> mock_observer;
    AuthenticatorRequestDialogModel model;
    model.AddObserver(&mock_observer);

    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = kAllTransportsWithoutCable;

    EXPECT_CALL(mock_observer, OnStepTransition());
    model.StartFlow(std::move(transports_info), base::nullopt);
    EXPECT_EQ(Step::kTransportSelection, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnStepTransition());
    (model.*test_case.event)();
    model.OnRequestComplete();
    EXPECT_EQ(test_case.expected_post_mortem_sheet, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnStepTransition());
    model.Cancel();
    EXPECT_EQ(Step::kClosed, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnModelDestroyed());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, BleAdapaterAlreadyPowered) {
  const struct {
    AuthenticatorTransport transport;
    Step expected_final_step;
  } kTestCases[] = {
      {AuthenticatorTransport::kBluetoothLowEnergy, Step::kBleActivate},
      {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
       Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = true;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model;
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.StartFlow(std::move(transports_info), base::nullopt);
    EXPECT_EQ(test_case.expected_final_step, model.current_step());
    EXPECT_TRUE(model.ble_adapter_is_powered());
    EXPECT_FALSE(power_receiver.was_called());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest,
       BleAdapaterNeedToBeManuallyPowered) {
  const struct {
    AuthenticatorTransport transport;
    Step expected_final_step;
  } kTestCases[] = {
      {AuthenticatorTransport::kBluetoothLowEnergy, Step::kBleActivate},
      {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
       Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = false;
    transports_info.is_ble_powered = false;

    testing::NiceMock<MockDialogModelObserver> mock_observer;
    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model;
    model.AddObserver(&mock_observer);
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.StartFlow(std::move(transports_info), base::nullopt);

    EXPECT_EQ(Step::kBlePowerOnManual, model.current_step());
    EXPECT_FALSE(model.ble_adapter_is_powered());

    EXPECT_CALL(mock_observer, OnBluetoothPoweredStateChanged());
    model.OnBluetoothPoweredStateChanged(true /* powered */);

    EXPECT_EQ(Step::kBlePowerOnManual, model.current_step());
    EXPECT_TRUE(model.ble_adapter_is_powered());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    model.ContinueWithFlowAfterBleAdapterPowered();

    EXPECT_EQ(test_case.expected_final_step, model.current_step());
    EXPECT_FALSE(power_receiver.was_called());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest,
       BleAdapaterCanBeAutomaticallyPowered) {
  const struct {
    AuthenticatorTransport transport;
    Step expected_final_step;
  } kTestCases[] = {
      {AuthenticatorTransport::kBluetoothLowEnergy, Step::kBleActivate},
      {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
       Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = false;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model;
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.StartFlow(std::move(transports_info), base::nullopt);

    EXPECT_EQ(Step::kBlePowerOnAutomatic, model.current_step());

    model.PowerOnBleAdapter();

    EXPECT_EQ(Step::kBlePowerOnAutomatic, model.current_step());
    EXPECT_TRUE(power_receiver.was_called());
    EXPECT_FALSE(model.ble_adapter_is_powered());

    model.OnBluetoothPoweredStateChanged(true /* powered */);

    EXPECT_EQ(test_case.expected_final_step, model.current_step());
    EXPECT_TRUE(model.ble_adapter_is_powered());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest,
       RequestCallbackOnlyCalledOncePerAuthenticator) {
  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
  transports_info.request_type =
      device::FidoRequestHandlerBase::RequestType::kMakeCredential;
  transports_info.available_transports = {
      AuthenticatorTransport::kInternal,
      AuthenticatorTransport::kUsbHumanInterfaceDevice};

  int num_called = 0;
  AuthenticatorRequestDialogModel model;
  model.SetRequestCallback(base::BindRepeating(
      [](int* i, const std::string& authenticator_id) { ++(*i); },
      &num_called));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      "authenticator" /* authenticator_id */,
      base::string16() /* authenticator_display_name */,
      AuthenticatorTransport::kInternal, false /* is_in_pairing_mode */));

  model.StartFlow(std::move(transports_info), base::nullopt);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kTransportSelection,
            model.current_step());
  EXPECT_EQ(0, num_called);

  // Simulate switching back and forth between transports. The request callback
  // should only be invoked once (USB is not dispatched through the UI).
  model.StartGuidedFlowForTransport(AuthenticatorTransport::kInternal);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kTouchId,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartGuidedFlowForTransport(
      AuthenticatorTransport::kUsbHumanInterfaceDevice);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartGuidedFlowForTransport(AuthenticatorTransport::kInternal);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kTouchId,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
}
