// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using RequestType = device::FidoRequestHandlerBase::RequestType;

const base::flat_set<AuthenticatorTransport> kAllTransports = {
    AuthenticatorTransport::kUsbHumanInterfaceDevice,
    AuthenticatorTransport::kNearFieldCommunication,
    AuthenticatorTransport::kInternal,
    AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
};

const base::flat_set<AuthenticatorTransport> kAllTransportsWithoutCable = {
    AuthenticatorTransport::kUsbHumanInterfaceDevice,
    AuthenticatorTransport::kNearFieldCommunication,
    AuthenticatorTransport::kInternal,
};

using TransportAvailabilityInfo =
    ::device::FidoRequestHandlerBase::TransportAvailabilityInfo;

class MockDialogModelObserver
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  MockDialogModelObserver() = default;

  MOCK_METHOD1(OnModelDestroyed, void(AuthenticatorRequestDialogModel*));
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

base::StringPiece RequestTypeToString(RequestType req_type) {
  switch (req_type) {
    case RequestType::kGetAssertion:
      return "GetAssertion";
    case RequestType::kMakeCredential:
      return "MakeCredential";
  }
}

enum class TransportAvailabilityParam {
  kHasPlatformCredential,
  kHasWinNativeAuthenticator,
  kHasCableExtension,
};

base::StringPiece TransportAvailabilityParamToString(
    TransportAvailabilityParam param) {
  switch (param) {
    case TransportAvailabilityParam::kHasPlatformCredential:
      return "kHasPlatformCredential";
    case TransportAvailabilityParam::kHasWinNativeAuthenticator:
      return "kHasWinNativeAuthenticator";
    case TransportAvailabilityParam::kHasCableExtension:
      return "kHasCableExtension";
  }
}

template <typename T, base::StringPiece (*F)(T)>
std::string SetToString(base::flat_set<T> s) {
  std::vector<base::StringPiece> names;
  std::transform(s.begin(), s.end(), std::back_inserter(names), F);
  return base::JoinString(names, ", ");
}

}  // namespace

class AuthenticatorRequestDialogModelTest : public ::testing::Test {
 public:
  using Step = AuthenticatorRequestDialogModel::Step;

  AuthenticatorRequestDialogModelTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModelTest);
};

TEST_F(AuthenticatorRequestDialogModelTest, TransportAutoSelection) {
  const struct {
    RequestType request_type;
    base::flat_set<AuthenticatorTransport> available_transports;
    base::flat_set<TransportAvailabilityParam> transport_params;
    Step expected_first_step;
  } kTestCases[] = {
      // Only a single transport is available for a GetAssertion call.
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       {},
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       {},
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       {TransportAvailabilityParam::kHasPlatformCredential},
       Step::kNotStarted},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy},
       {TransportAvailabilityParam::kHasCableExtension},
       Step::kCableActivate},

      {RequestType::kGetAssertion,
       kAllTransportsWithoutCable,
       {},
       Step::kTransportSelection},

      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kUsbHumanInterfaceDevice},
       {},
       Step::kTransportSelection},

      // The KeyChain contains an allowed Touch ID credential.
      {RequestType::kGetAssertion,
       kAllTransports,
       {TransportAvailabilityParam::kHasPlatformCredential},
       Step::kNotStarted},

      // The KeyChain does not contain an allowed Touch ID credential.
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       {},
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kInternal},
       {},
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion,
       kAllTransportsWithoutCable,
       {},
       Step::kTransportSelection},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal},
       {},
       Step::kTransportSelection},

      // The KeyChain contains an allowed Touch ID credential, but Touch ID is
      // not enabled by the relying party.
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       {TransportAvailabilityParam::kHasPlatformCredential},
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kNearFieldCommunication},
       {TransportAvailabilityParam::kHasPlatformCredential},
       Step::kTransportSelection},

      // If caBLE is one of the allowed transports, it has second-highest
      // priority after Touch ID, and is auto-selected for GetAssertion
      // operations.
      {RequestType::kGetAssertion,
       kAllTransports,
       {TransportAvailabilityParam::kHasCableExtension},
       Step::kCableActivate},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
        AuthenticatorTransport::kUsbHumanInterfaceDevice},
       {TransportAvailabilityParam::kHasCableExtension},
       Step::kCableActivate},

      // caBLE should not enjoy this same high priority for MakeCredential
      // calls.
      {RequestType::kMakeCredential,
       kAllTransports,
       {},
       Step::kTransportSelection},

      // No transports available.
      {RequestType::kGetAssertion, {}, {}, Step::kErrorNoAvailableTransports},

      // We default to transport selection modal for MakeCredential.
      {RequestType::kMakeCredential,
       kAllTransports,
       {},
       Step::kTransportSelection},

      // When only one transport is available, we still want to skip transport
      // selection view for MakeCredential call.
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       {},
       Step::kUsbInsertAndActivate},
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kInternal},
       {},
       Step::kNotStarted},
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy},
       {TransportAvailabilityParam::kHasCableExtension},
       Step::kCableActivate},
      {RequestType::kMakeCredential,
       {AuthenticatorTransport::kUsbHumanInterfaceDevice},
       {},
       Step::kUsbInsertAndActivate},

      // Windows authenticator will bypass the UI unless caBLE is also
      // available.
      {RequestType::kGetAssertion,
       {},
       {TransportAvailabilityParam::kHasWinNativeAuthenticator},
       Step::kNotStarted},
      {RequestType::kGetAssertion,
       {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy},
       {TransportAvailabilityParam::kHasWinNativeAuthenticator,
        TransportAvailabilityParam::kHasCableExtension},
       Step::kCableActivate},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(static_cast<int>(test_case.expected_first_step));
    SCOPED_TRACE((SetToString<TransportAvailabilityParam,
                              TransportAvailabilityParamToString>(
        test_case.transport_params)));
    SCOPED_TRACE((SetToString<device::FidoTransportProtocol, device::ToString>(
        test_case.available_transports)));
    SCOPED_TRACE(RequestTypeToString(test_case.request_type));

    TransportAvailabilityInfo transports_info;
    transports_info.is_ble_powered = true;
    transports_info.request_type = test_case.request_type;
    transports_info.available_transports = test_case.available_transports;

    transports_info.has_recognized_platform_authenticator_credential =
        base::Contains(test_case.transport_params,
                       TransportAvailabilityParam::kHasPlatformCredential);

    if (base::Contains(
            test_case.transport_params,
            TransportAvailabilityParam::kHasWinNativeAuthenticator)) {
      transports_info.has_win_native_api_authenticator = true;
      transports_info.win_native_api_authenticator_id = "some_authenticator_id";
    }

    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");

    if (base::Contains(test_case.transport_params,
                       TransportAvailabilityParam::kHasCableExtension)) {
      model.set_cable_transport_info(true, false, base::nullopt);
    }

    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false);
    EXPECT_EQ(test_case.expected_first_step, model.current_step());

    if (!model.offer_try_again_in_ui()) {
      continue;
    }

    model.StartOver();
    EXPECT_EQ(Step::kTransportSelection, model.current_step());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, TransportList) {
  for (const bool cable_extension_provided : {false, true}) {
    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = kAllTransports;
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.set_cable_transport_info(cable_extension_provided,
                                   /*has_paired_phones=*/false,
                                   /*cable_qr_string=*/base::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false);
    EXPECT_THAT(model.available_transports(),
                ::testing::UnorderedElementsAre(
                    AuthenticatorTransport::kUsbHumanInterfaceDevice,
                    AuthenticatorTransport::kNearFieldCommunication,
                    AuthenticatorTransport::kInternal,
                    AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy));
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, NoAvailableTransports) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
  model.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.StartFlow(TransportAvailabilityInfo(),
                  /*use_location_bar_bubble=*/false);
  EXPECT_EQ(Step::kErrorNoAvailableTransports, model.current_step());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnCancelRequest());
  model.Cancel();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.OnRequestComplete();
  EXPECT_EQ(Step::kClosed, model.current_step());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnModelDestroyed(&model));
}

TEST_F(AuthenticatorRequestDialogModelTest, AwaitingAcknowledgement) {
  const struct {
    void (AuthenticatorRequestDialogModel::*event)();
    Step expected_sheet;
  } kTestCases[] = {
      {&AuthenticatorRequestDialogModel::OnRequestTimeout, Step::kTimedOut},
      {&AuthenticatorRequestDialogModel::OnActivatedKeyNotRegistered,
       Step::kKeyNotRegistered},
      {&AuthenticatorRequestDialogModel::OnActivatedKeyAlreadyRegistered,
       Step::kKeyAlreadyRegistered},
  };

  for (const auto& test_case : kTestCases) {
    testing::StrictMock<MockDialogModelObserver> mock_observer;
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.AddObserver(&mock_observer);

    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = kAllTransportsWithoutCable;

    EXPECT_CALL(mock_observer, OnStepTransition());
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false);
    EXPECT_EQ(Step::kTransportSelection, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnStepTransition());
    (model.*test_case.event)();
    EXPECT_EQ(test_case.expected_sheet, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnStepTransition());
    EXPECT_CALL(mock_observer, OnCancelRequest());
    model.Cancel();
    EXPECT_EQ(Step::kClosed, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnModelDestroyed(&model));
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, BleAdapterAlreadyPowered) {
  const struct {
    AuthenticatorTransport transport;
    Step expected_final_step;
  } kTestCases[] = {
      {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
       Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = true;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, false, base::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false);
    EXPECT_EQ(test_case.expected_final_step, model.current_step());
    EXPECT_TRUE(model.ble_adapter_is_powered());
    EXPECT_FALSE(power_receiver.was_called());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, BleAdapterNeedToBeManuallyPowered) {
  const struct {
    AuthenticatorTransport transport;
    Step expected_final_step;
  } kTestCases[] = {
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
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.AddObserver(&mock_observer);
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, false, base::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false);

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
       BleAdapterCanBeAutomaticallyPowered) {
  const struct {
    AuthenticatorTransport transport;
    Step expected_final_step;
  } kTestCases[] = {
      {AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy,
       Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = false;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, false, base::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false);

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
  AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
  model.SetRequestCallback(base::BindRepeating(
      [](int* i, const std::string& authenticator_id) { ++(*i); },
      &num_called));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"authenticator", AuthenticatorTransport::kInternal));

  model.StartFlow(std::move(transports_info),
                  /*use_location_bar_bubble=*/false);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kTransportSelection,
            model.current_step());
  EXPECT_EQ(0, num_called);

  // Simulate switching back and forth between transports. The request callback
  // should only be invoked once (USB is not dispatched through the UI).
  model.StartGuidedFlowForTransport(AuthenticatorTransport::kInternal);
  EXPECT_TRUE(model.should_dialog_be_hidden());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartGuidedFlowForTransport(
      AuthenticatorTransport::kUsbHumanInterfaceDevice);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartGuidedFlowForTransport(AuthenticatorTransport::kInternal);
  EXPECT_TRUE(model.should_dialog_be_hidden());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
}

TEST_F(AuthenticatorRequestDialogModelTest,
       RequestCallbackForWindowsAuthenticatorIsInvokedAutomatically) {
  constexpr char kWinAuthenticatorId[] = "some_authenticator_id";

  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
  transports_info.request_type =
      device::FidoRequestHandlerBase::RequestType::kMakeCredential;
  transports_info.available_transports = {};
  transports_info.has_win_native_api_authenticator = true;
  transports_info.win_native_api_authenticator_id = kWinAuthenticatorId;

  std::vector<std::string> dispatched_authenticator_ids;
  AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
  model.SetRequestCallback(base::BindRepeating(
      [](std::vector<std::string>* ids, const std::string& authenticator_id) {
        ids->push_back(authenticator_id);
      },
      &dispatched_authenticator_ids));

  model.StartFlow(std::move(transports_info),
                  /*use_location_bar_bubble=*/false);

  EXPECT_TRUE(model.should_dialog_be_hidden());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_THAT(dispatched_authenticator_ids, ElementsAre(kWinAuthenticatorId));
}

TEST_F(AuthenticatorRequestDialogModelTest,
       ConditionalUINoRecognizedCredential) {
  AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");

  int num_called = 0;
  model.SetRequestCallback(base::BindRepeating(
      [](int* i, const std::string& authenticator_id) { ++(*i); },
      &num_called));
  model.saved_authenticators().AddAuthenticator(
      AuthenticatorReference(/*device_id=*/"authenticator",
                             AuthenticatorTransport::kUsbHumanInterfaceDevice));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"authenticator", AuthenticatorTransport::kInternal));

  TransportAvailabilityInfo transports_info;
  transports_info.available_transports = kAllTransports;
  transports_info.has_recognized_platform_authenticator_credential = true;
  model.StartFlow(std::move(transports_info),
                  /*use_location_bar_bubble=*/true);
  EXPECT_EQ(model.current_step(), Step::kLocationBarBubble);
  EXPECT_TRUE(model.should_dialog_be_hidden());
  EXPECT_EQ(num_called, 0);
}

TEST_F(AuthenticatorRequestDialogModelTest, ConditionalUIRecognizedCredential) {
  AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");

  int num_called = 0;
  model.SetRequestCallback(base::BindRepeating(
      [](int* i, const std::string& authenticator_id) {
        EXPECT_EQ(authenticator_id, "internal");
        ++(*i);
      },
      &num_called));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"usb", AuthenticatorTransport::kUsbHumanInterfaceDevice));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"internal", AuthenticatorTransport::kInternal));

  TransportAvailabilityInfo transports_info;
  transports_info.available_transports = kAllTransports;
  transports_info.has_recognized_platform_authenticator_credential = true;
  device::PublicKeyCredentialUserEntity user_1({1, 2, 3, 4});
  device::PublicKeyCredentialUserEntity user_2({5, 6, 7, 8});
  transports_info.recognized_platform_authenticator_credentials = {user_1,
                                                                   user_2};
  model.StartFlow(std::move(transports_info),
                  /*is_location_bar_bubble_ui==*/true);
  EXPECT_EQ(model.current_step(), Step::kLocationBarBubble);
  EXPECT_TRUE(model.should_dialog_be_hidden());
  EXPECT_EQ(num_called, 0);

  // After selecting an account, the request should be dispatched to the
  // platform authenticator.
  model.OnAccountSelected(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(num_called, 1);

  static const uint8_t kAppParam[32] = {0};
  static const uint8_t kSignatureCounter[4] = {0};
  device::AuthenticatorData auth_data(kAppParam, /*flags=*/0, kSignatureCounter,
                                      base::nullopt);
  device::AuthenticatorGetAssertionResponse response_1(
      device::AuthenticatorData(kAppParam, /*flags=*/0, kSignatureCounter,
                                base::nullopt),
      /*signature=*/{1});
  response_1.user_entity = user_1;
  device::AuthenticatorGetAssertionResponse response_2(
      device::AuthenticatorData(kAppParam, /*flags=*/0, kSignatureCounter,
                                base::nullopt),
      /*signature=*/{2});
  response_2.user_entity = user_2;

  uint8_t selected_id = -1;
  std::vector<device::AuthenticatorGetAssertionResponse> responses;
  responses.push_back(std::move(response_1));
  responses.push_back(std::move(response_2));
  model.SelectAccount(
      std::move(responses),
      base::BindLambdaForTesting(
          [&](device::AuthenticatorGetAssertionResponse selected) {
            selected_id = selected.signature[0];
          }));
  EXPECT_EQ(selected_id, 1);
}
