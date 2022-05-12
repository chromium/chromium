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
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using testing::ElementsAre;
using RequestType = device::FidoRequestType;

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

  MockDialogModelObserver(const MockDialogModelObserver&) = delete;
  MockDialogModelObserver& operator=(const MockDialogModelObserver&) = delete;

  MOCK_METHOD1(OnModelDestroyed, void(AuthenticatorRequestDialogModel*));
  MOCK_METHOD0(OnStepTransition, void());
  MOCK_METHOD0(OnCancelRequest, void());
  MOCK_METHOD0(OnBluetoothPoweredStateChanged, void());
};

class BluetoothAdapterPowerOnCallbackReceiver {
 public:
  BluetoothAdapterPowerOnCallbackReceiver() = default;

  BluetoothAdapterPowerOnCallbackReceiver(
      const BluetoothAdapterPowerOnCallbackReceiver&) = delete;
  BluetoothAdapterPowerOnCallbackReceiver& operator=(
      const BluetoothAdapterPowerOnCallbackReceiver&) = delete;

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
  kHasCableV1Extension,
  kHasCableV2Extension,
  kPreferNativeAPI,
};

base::StringPiece TransportAvailabilityParamToString(
    TransportAvailabilityParam param) {
  switch (param) {
    case TransportAvailabilityParam::kHasPlatformCredential:
      return "kHasPlatformCredential";
    case TransportAvailabilityParam::kHasWinNativeAuthenticator:
      return "kHasWinNativeAuthenticator";
    case TransportAvailabilityParam::kHasCableV1Extension:
      return "kHasCableV1Extension";
    case TransportAvailabilityParam::kHasCableV2Extension:
      return "kHasCableV2Extension";
    case TransportAvailabilityParam::kPreferNativeAPI:
      return "kPreferNativeAPI";
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

  AuthenticatorRequestDialogModelTest(
      const AuthenticatorRequestDialogModelTest&) = delete;
  AuthenticatorRequestDialogModelTest& operator=(
      const AuthenticatorRequestDialogModelTest&) = delete;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AuthenticatorRequestDialogModelTest, Mechanisms) {
  const auto mc = RequestType::kMakeCredential;
  const auto ga = RequestType::kGetAssertion;
  const auto usb = AuthenticatorTransport::kUsbHumanInterfaceDevice;
  const auto internal = AuthenticatorTransport::kInternal;
  const auto cable = AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy;
  const auto aoa = AuthenticatorTransport::kAndroidAccessory;
  const auto v1 = TransportAvailabilityParam::kHasCableV1Extension;
  const auto v2 = TransportAvailabilityParam::kHasCableV2Extension;
  const auto has_winapi =
      TransportAvailabilityParam::kHasWinNativeAuthenticator;
  const auto has_plat = TransportAvailabilityParam::kHasPlatformCredential;
  const auto native = TransportAvailabilityParam::kPreferNativeAPI;
  using t = AuthenticatorRequestDialogModel::Mechanism::Transport;
  using p = AuthenticatorRequestDialogModel::Mechanism::Phone;
  const auto winapi =
      AuthenticatorRequestDialogModel::Mechanism::WindowsAPI(true);
  const auto add = AuthenticatorRequestDialogModel::Mechanism::AddPhone(false);
  const auto usb_ui = Step::kUsbInsertAndActivate;
  const auto mss = Step::kMechanismSelection;
  const auto plat_ui = Step::kNotStarted;
  const auto cable_ui = Step::kCableActivate;

  const struct {
    RequestType request_type;
    base::flat_set<AuthenticatorTransport> transports;
    base::flat_set<TransportAvailabilityParam> params;
    std::vector<std::string> phone_names;
    std::vector<AuthenticatorRequestDialogModel::Mechanism::Type>
        expected_mechanisms;
    Step expected_first_step;
  } kTests[] = {
      // If there's only a single mechanism, it should activate.
      {mc, {usb}, {}, {}, {t(usb)}, usb_ui},
      {ga, {usb}, {}, {}, {t(usb)}, usb_ui},
      // ... otherwise should the selection sheet.
      {mc, {usb, internal}, {}, {}, {t(usb), t(internal)}, mss},
      {ga, {usb, internal}, {}, {}, {t(usb), t(internal)}, mss},

      // If the platform authenticator has a credential it should activate.
      {ga, {usb, internal}, {has_plat}, {}, {t(usb), t(internal)}, plat_ui},

      // If the Windows API is available without caBLE, it should activate.
      {mc, {}, {has_winapi}, {}, {winapi}, plat_ui},
      {ga, {}, {has_winapi}, {}, {winapi}, plat_ui},
      // ... even if, somehow, there's another transport.
      {mc, {usb}, {has_winapi}, {}, {winapi, t(usb)}, plat_ui},
      {ga, {usb}, {has_winapi}, {}, {winapi, t(usb)}, plat_ui},

      // A caBLEv1 extension should cause us to go directly to caBLE.
      {ga, {usb, cable}, {v1}, {}, {t(usb), t(cable)}, cable_ui},
      // A caBLEv2 extension should cause us to go directly to caBLE, but also
      // show the AOA option.
      {ga, {usb, aoa, cable}, {v2}, {}, {t(usb), t(aoa), t(cable)}, cable_ui},

      // If there are linked phones then AOA doesn't show up, but the phones do,
      // and sorted. The selection sheet should show.
      {mc,
       {usb, aoa, cable},
       {},
       {"a", "b"},
       {add, t(usb), p("a"), p("b")},
       mss},
      {ga,
       {usb, aoa, cable},
       {},
       {"a", "b"},
       {add, t(usb), p("a"), p("b")},
       mss},

      // On Windows, if there are linked phones we'll show a selection sheet.
      {mc, {cable}, {has_winapi}, {"a"}, {winapi, add, p("a")}, mss},
      {ga, {cable}, {has_winapi}, {"a"}, {winapi, add, p("a")}, mss},
      // ... unless the `prefer_native_api` flag is set because Chrome
      // remembered that the last successful security key operation was via the
      // Windows API. In that case we'll still jump directly to the native UI.
      {mc,
       {cable},
       {has_winapi, native},
       {"a"},
       {winapi, add, p("a")},
       plat_ui},
      {ga,
       {cable},
       {has_winapi, native},
       {"a"},
       {winapi, add, p("a")},
       plat_ui},
      // Even without `prefer_native_api`, if there aren't any linked phones
      // we'll still jump directly to the native UI, at least until we enable
      // the "Add phone" option.
      {mc, {cable}, {has_winapi}, {}, {winapi}, plat_ui},
      {ga, {cable}, {has_winapi}, {}, {winapi}, plat_ui},
  };

  unsigned test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(static_cast<int>(test.expected_first_step));
    SCOPED_TRACE(
        (SetToString<TransportAvailabilityParam,
                     TransportAvailabilityParamToString>(test.params)));
    SCOPED_TRACE((SetToString<device::FidoTransportProtocol, device::ToString>(
        test.transports)));
    SCOPED_TRACE(RequestTypeToString(test.request_type));
    SCOPED_TRACE(test_num++);

    TransportAvailabilityInfo transports_info;
    transports_info.is_ble_powered = true;
    transports_info.request_type = test.request_type;
    transports_info.available_transports = test.transports;

    transports_info.has_platform_authenticator_credential =
        base::Contains(test.params,
                       TransportAvailabilityParam::kHasPlatformCredential)
            ? device::FidoRequestHandlerBase::RecognizedCredential::
                  kHasRecognizedCredential
            : device::FidoRequestHandlerBase::RecognizedCredential::
                  kNoRecognizedCredential;

    if (base::Contains(
            test.params,
            TransportAvailabilityParam::kHasWinNativeAuthenticator)) {
      transports_info.has_win_native_api_authenticator = true;
      transports_info.win_native_api_authenticator_id = "some_authenticator_id";
    }

    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");

    absl::optional<bool> has_v2_cable_extension;
    if (base::Contains(test.params,
                       TransportAvailabilityParam::kHasCableV1Extension)) {
      has_v2_cable_extension = false;
    }

    if (base::Contains(test.params,
                       TransportAvailabilityParam::kHasCableV2Extension)) {
      CHECK(!has_v2_cable_extension.has_value());
      has_v2_cable_extension = true;
    }

    if (has_v2_cable_extension.has_value() || !test.phone_names.empty()) {
      std::vector<AuthenticatorRequestDialogModel::PairedPhone> phones;
      for (const auto& name : test.phone_names) {
        std::array<uint8_t, device::kP256X962Length> public_key = {0};
        public_key[0] = base::checked_cast<uint8_t>(phones.size());
        phones.emplace_back(name, /*contact_id=*/0, public_key);
      }
      model.set_cable_transport_info(has_v2_cable_extension, std::move(phones),
                                     base::DoNothing(), absl::nullopt);
    }

    model.StartFlow(
        std::move(transports_info),
        /*use_location_bar_bubble=*/false,
        /*prefer_native_api=*/
        base::Contains(test.params,
                       TransportAvailabilityParam::kPreferNativeAPI));
    EXPECT_EQ(test.expected_first_step, model.current_step());

    std::vector<AuthenticatorRequestDialogModel::Mechanism::Type>
        mechanism_types;
    for (const auto& mech : model.mechanisms()) {
      mechanism_types.push_back(mech.type);
    }
    EXPECT_EQ(test.expected_mechanisms, mechanism_types);

    if (!model.offer_try_again_in_ui()) {
      continue;
    }

    model.StartOver();
    EXPECT_EQ(Step::kMechanismSelection, model.current_step());
  }
}

#if BUILDFLAG(IS_WIN)
TEST_F(AuthenticatorRequestDialogModelTest, WinCancel) {
  // Simulate the user canceling the Windows native UI, both with and without
  // that UI being immediately triggered. If it was immediately triggered then
  // canceling it should show the mechanism selection UI.
  for (const bool prefer_native_api : {false, true}) {
    SCOPED_TRACE(prefer_native_api);

    AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
    tai.has_win_native_api_authenticator = true;
    tai.win_native_api_authenticator_id = "ID";
    tai.available_transports.insert(
        device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);

    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.set_cable_transport_info(absl::nullopt, {}, base::DoNothing(),
                                   "fido:/1234");

    model.StartFlow(std::move(tai),
                    /*use_location_bar_bubble=*/false, prefer_native_api);

    if (prefer_native_api) {
      // The Windows native UI should have been triggered.
      EXPECT_EQ(model.current_step(), Step::kNotStarted);
      // Canceling the Windows native UI should be handled.
      EXPECT_TRUE(model.OnWinUserCancelled());
    }

    // The mechanism selection sheet should now be showing.
    EXPECT_EQ(model.current_step(), Step::kMechanismSelection);
    // Canceling the Windows UI ends the request because the user must have
    // selected the Windows option first.
    EXPECT_FALSE(model.OnWinUserCancelled());
  }
}
#endif

TEST_F(AuthenticatorRequestDialogModelTest, NoAvailableTransports) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
  model.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.StartFlow(TransportAvailabilityInfo(),
                  /*use_location_bar_bubble=*/false,
                  /*prefer_native_api=*/false);
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

TEST_F(AuthenticatorRequestDialogModelTest, Cable2ndFactorFlows) {
  enum class BLEPower {
    ON,
    OFF,
  };
  enum class Profile {
    NORMAL,
    INCOGNITO,
  };

  const auto mc = RequestType::kMakeCredential;
  const auto ga = RequestType::kGetAssertion;
  const auto on_ = BLEPower::ON;
  const auto off = BLEPower::OFF;
  const auto normal = Profile::NORMAL;
  const auto otr___ = Profile::INCOGNITO;
  const auto mss = Step::kMechanismSelection;
  const auto activate = Step::kCableActivate;
  const auto interstitial = Step::kOffTheRecordInterstitial;
  const auto power = Step::kBlePowerOnAutomatic;

  const struct {
    RequestType request_type;
    BLEPower ble_power;
    Profile profile;
    std::vector<Step> steps;
  } kTests[] = {
      //               | Expected UI steps in order.
      {mc, on_, normal, {mss, activate}},
      {mc, on_, otr___, {mss, interstitial, activate}},
      {mc, off, normal, {mss, power, activate}},
      {mc, off, otr___, {mss, interstitial, power, activate}},
      {ga, on_, normal, {mss, activate}},
      {ga, on_, otr___, {mss, activate}},
      {ga, off, normal, {mss, power, activate}},
      {ga, off, otr___, {mss, power, activate}},
  };

  unsigned test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);

    TransportAvailabilityInfo transports_info;
    transports_info.is_ble_powered = test.ble_power == BLEPower::ON;
    transports_info.can_power_on_ble_adapter = true;
    transports_info.request_type = test.request_type;
    transports_info.available_transports = {
        AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy};
    transports_info.is_off_the_record_context =
        test.profile == Profile::INCOGNITO;

    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");

    std::array<uint8_t, device::kP256X962Length> public_key = {0};
    std::vector<AuthenticatorRequestDialogModel::PairedPhone> phones(
        {{"phone", /*contact_id=*/0, public_key}});
    model.set_cable_transport_info(/*extension_is_v2=*/absl::nullopt,
                                   std::move(phones), base::DoNothing(),
                                   absl::nullopt);

    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false,
                    /*prefer_native_api=*/false);
    ASSERT_EQ(model.mechanisms().size(), 2u);

    for (const auto step : test.steps) {
      ASSERT_EQ(step, model.current_step())
          << static_cast<int>(step)
          << " != " << static_cast<int>(model.current_step());

      switch (step) {
        case Step::kMechanismSelection:
          // Click the first (and only) phone.
          for (const auto& mechanism : model.mechanisms()) {
            if (absl::holds_alternative<
                    AuthenticatorRequestDialogModel::Mechanism::Phone>(
                    mechanism.type)) {
              mechanism.callback.Run();
              break;
            }
          }
          break;

        case Step::kBlePowerOnAutomatic:
          model.OnBluetoothPoweredStateChanged(/*powered=*/true);
          break;

        case Step::kOffTheRecordInterstitial:
          model.OnOffTheRecordInterstitialAccepted();
          break;

        case Step::kCableActivate:
          break;

        default:
          NOTREACHED();
      }
    }
  }
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
                    /*use_location_bar_bubble=*/false,
                    /*prefer_native_api=*/false);
    EXPECT_EQ(Step::kMechanismSelection, model.current_step());
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
    transports_info.request_type = RequestType::kGetAssertion;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = true;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, {}, base::DoNothing(), absl::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false,
                    /*prefer_native_api=*/false);
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
    transports_info.request_type = RequestType::kGetAssertion;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = false;
    transports_info.is_ble_powered = false;

    testing::NiceMock<MockDialogModelObserver> mock_observer;
    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.AddObserver(&mock_observer);
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, {}, base::DoNothing(), absl::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false,
                    /*prefer_native_api=*/false);

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
    transports_info.request_type = RequestType::kGetAssertion;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = false;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*relying_party_id=*/"example.com");
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, {}, base::DoNothing(), absl::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*use_location_bar_bubble=*/false,
                    /*prefer_native_api=*/false);

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
  transports_info.request_type = RequestType::kMakeCredential;
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
                  /*use_location_bar_bubble=*/false,
                  /*prefer_native_api=*/false);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kMechanismSelection,
            model.current_step());
  EXPECT_EQ(0, num_called);

  // Simulate switching back and forth between transports. The request callback
  // should only be invoked once (USB is not dispatched through the UI).
  model.StartTransportFlowForTesting(AuthenticatorTransport::kInternal);
  EXPECT_TRUE(model.should_dialog_be_hidden());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartTransportFlowForTesting(
      AuthenticatorTransport::kUsbHumanInterfaceDevice);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartTransportFlowForTesting(AuthenticatorTransport::kInternal);
  EXPECT_TRUE(model.should_dialog_be_hidden());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
}

TEST_F(AuthenticatorRequestDialogModelTest,
       RequestCallbackForWindowsAuthenticatorIsInvokedAutomatically) {
  constexpr char kWinAuthenticatorId[] = "some_authenticator_id";

  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
  transports_info.request_type = RequestType::kMakeCredential;
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
                  /*use_location_bar_bubble=*/false,
                  /*prefer_native_api=*/false);

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
  transports_info.has_platform_authenticator_credential = device::
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
  model.StartFlow(std::move(transports_info),
                  /*use_location_bar_bubble=*/true,
                  /*prefer_native_api=*/false);
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
  transports_info.has_platform_authenticator_credential = device::
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
  device::DiscoverableCredentialMetadata cred_1(
      {0}, device::PublicKeyCredentialUserEntity({1, 2, 3, 4}));
  device::DiscoverableCredentialMetadata cred_2(
      {1}, device::PublicKeyCredentialUserEntity({5, 6, 7, 8}));
  transports_info.recognized_platform_authenticator_credentials = {cred_1,
                                                                   cred_2};
  model.StartFlow(std::move(transports_info),
                  /*is_location_bar_bubble_ui==*/true,
                  /*prefer_native_api=*/false);
  EXPECT_EQ(model.current_step(), Step::kLocationBarBubble);
  EXPECT_TRUE(model.should_dialog_be_hidden());
  EXPECT_EQ(num_called, 0);

  // After preselecting an account, the request should be dispatched to the
  // platform authenticator.
  model.OnAccountPreselected({1, 2, 3, 4});
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(num_called, 1);

  static const uint8_t kAppParam[32] = {0};
  static const uint8_t kSignatureCounter[4] = {0};
  device::AuthenticatorData auth_data(kAppParam, /*flags=*/0, kSignatureCounter,
                                      absl::nullopt);
  device::AuthenticatorGetAssertionResponse response_1(
      device::AuthenticatorData(kAppParam, /*flags=*/0, kSignatureCounter,
                                absl::nullopt),
      /*signature=*/{1});
  response_1.user_entity = cred_1.user;
  device::AuthenticatorGetAssertionResponse response_2(
      device::AuthenticatorData(kAppParam, /*flags=*/0, kSignatureCounter,
                                absl::nullopt),
      /*signature=*/{2});
  response_2.user_entity = cred_2.user;

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
