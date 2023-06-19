// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/webauthn_api.h"
#endif

namespace {

using testing::ElementsAre;
using RequestType = device::FidoRequestType;

const base::flat_set<AuthenticatorTransport> kAllTransports = {
    AuthenticatorTransport::kUsbHumanInterfaceDevice,
    AuthenticatorTransport::kNearFieldCommunication,
    AuthenticatorTransport::kInternal,
    AuthenticatorTransport::kHybrid,
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

  MOCK_METHOD0(OnStartOver, void());
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
  kMaybeHasPlatformCredential,
  kHasPlatformCredential,
  kOneRecognizedCred,
  kTwoRecognizedCreds,
  kEmptyAllowList,
  kOnlyHybridOrInternal,
  kHasWinNativeAuthenticator,
  kHasCableV1Extension,
  kHasCableV2Extension,
  kRequireResidentKey,
  kIsConditionalUI,
  kAttachmentAny,
  kAttachmentCrossPlatform,
};

base::StringPiece TransportAvailabilityParamToString(
    TransportAvailabilityParam param) {
  switch (param) {
    case TransportAvailabilityParam::kMaybeHasPlatformCredential:
      return "kMaybeHasPlatformCredential";
    case TransportAvailabilityParam::kHasPlatformCredential:
      return "kHasPlatformCredential";
    case TransportAvailabilityParam::kOneRecognizedCred:
      return "kOneRecognizedCred";
    case TransportAvailabilityParam::kTwoRecognizedCreds:
      return "kTwoRecognizedCreds";
    case TransportAvailabilityParam::kEmptyAllowList:
      return "kEmptyAllowList";
    case TransportAvailabilityParam::kOnlyHybridOrInternal:
      return "kOnlyHybridOrInternal";
    case TransportAvailabilityParam::kHasWinNativeAuthenticator:
      return "kHasWinNativeAuthenticator";
    case TransportAvailabilityParam::kHasCableV1Extension:
      return "kHasCableV1Extension";
    case TransportAvailabilityParam::kHasCableV2Extension:
      return "kHasCableV2Extension";
    case TransportAvailabilityParam::kRequireResidentKey:
      return "kRequireResidentKey";
    case TransportAvailabilityParam::kIsConditionalUI:
      return "kIsConditionalUI";
    case TransportAvailabilityParam::kAttachmentAny:
      return "kAttachmentAny";
    case TransportAvailabilityParam::kAttachmentCrossPlatform:
      return "kAttachmentCrossPlatform";
  }
}

template <typename T, base::StringPiece (*F)(T)>
std::string SetToString(base::flat_set<T> s) {
  std::vector<base::StringPiece> names;
  base::ranges::transform(s, std::back_inserter(names), F);
  return base::JoinString(names, ", ");
}

const device::DiscoverableCredentialMetadata kCred1(
    device::AuthenticatorType::kOther,
    "rp.com",
    {0},
    device::PublicKeyCredentialUserEntity({1, 2, 3, 4}));
const device::DiscoverableCredentialMetadata kCred2(
    device::AuthenticatorType::kOther,
    "rp.com",
    {1},
    device::PublicKeyCredentialUserEntity({5, 6, 7, 8}));

}  // namespace

class AuthenticatorRequestDialogModelTest
    : public ChromeRenderViewHostTestHarness {
 public:
  using Step = AuthenticatorRequestDialogModel::Step;

  AuthenticatorRequestDialogModelTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  AuthenticatorRequestDialogModelTest(
      const AuthenticatorRequestDialogModelTest&) = delete;
  AuthenticatorRequestDialogModelTest& operator=(
      const AuthenticatorRequestDialogModelTest&) = delete;
};

TEST_F(AuthenticatorRequestDialogModelTest, Mechanisms) {
  const auto mc = RequestType::kMakeCredential;
  const auto ga = RequestType::kGetAssertion;
  const auto usb = AuthenticatorTransport::kUsbHumanInterfaceDevice;
  const auto internal = AuthenticatorTransport::kInternal;
  const auto cable = AuthenticatorTransport::kHybrid;
  const auto aoa = AuthenticatorTransport::kAndroidAccessory;
  const auto v1 = TransportAvailabilityParam::kHasCableV1Extension;
  const auto v2 = TransportAvailabilityParam::kHasCableV2Extension;
  const auto has_winapi =
      TransportAvailabilityParam::kHasWinNativeAuthenticator;
  const auto has_plat = TransportAvailabilityParam::kHasPlatformCredential;
  const auto maybe_plat =
      TransportAvailabilityParam::kMaybeHasPlatformCredential;
  const auto one_cred = TransportAvailabilityParam::kOneRecognizedCred;
  const auto two_cred = TransportAvailabilityParam::kTwoRecognizedCreds;
  const auto empty_al = TransportAvailabilityParam::kEmptyAllowList;
  const auto only_hybrid_or_internal =
      TransportAvailabilityParam::kOnlyHybridOrInternal;
  const auto rk = TransportAvailabilityParam::kRequireResidentKey;
  const auto c_ui = TransportAvailabilityParam::kIsConditionalUI;
  const auto att_any = TransportAvailabilityParam::kAttachmentAny;
  const auto att_xplat = TransportAvailabilityParam::kAttachmentCrossPlatform;
  using t = AuthenticatorRequestDialogModel::Mechanism::Transport;
  using p = AuthenticatorRequestDialogModel::Mechanism::Phone;
  const auto winapi = AuthenticatorRequestDialogModel::Mechanism::WindowsAPI();
  const auto add = AuthenticatorRequestDialogModel::Mechanism::AddPhone();
  const auto usb_ui = Step::kUsbInsertAndActivate;
  const auto mss = Step::kMechanismSelection;
  const auto plat_ui = Step::kNotStarted;
  const auto cable_ui = Step::kCableActivate;
  [[maybe_unused]] const auto create_pk = Step::kCreatePasskey;
  const auto use_pk = Step::kPreSelectSingleAccount;
  const auto use_pk_multi = Step::kPreSelectAccount;
  const auto qr = Step::kCableV2QRCode;
  const auto pconf = Step::kPhoneConfirmationSheet;

  const struct {
    int line_num;
    RequestType request_type;
    base::flat_set<AuthenticatorTransport> transports;
    base::flat_set<TransportAvailabilityParam> params;
    std::vector<std::string> phone_names;
    std::vector<AuthenticatorRequestDialogModel::Mechanism::Type>
        expected_mechanisms;
    Step expected_first_step;
    std::vector<base::test::FeatureRef> features;
  } kTests[] = {
#define L __LINE__
      // If there's only a single mechanism, it should activate.
      {L, mc, {usb}, {}, {}, {t(usb)}, usb_ui},
      {L, ga, {usb}, {}, {}, {t(usb)}, usb_ui},
      // ... otherwise should the selection sheet.
      {L, ga, {usb, cable}, {}, {}, {add, t(usb)}, mss},
      {L, ga, {usb, cable}, {}, {}, {add, t(usb)}, mss},

      // If the platform authenticator has a credential it should activate.
      {L, ga, {usb, internal}, {has_plat}, {}, {t(internal), t(usb)}, plat_ui},
      // ... but with an empty allow list the user should be prompted first.
      {L,
       ga,
       {usb, internal},
       {has_plat, one_cred, empty_al},
       {},
       {t(internal), t(usb)},
       use_pk},
      {L,
       ga,
       {usb, internal},
       {has_plat, two_cred, empty_al},
       {},
       {t(internal), t(usb)},
       use_pk_multi},

      // MakeCredential with attachment=platform shows the 'Create a passkey'
      // step, but only on macOS. On other OSes, we defer to the platform.
      {L,
       mc,
       {internal},
       {},
       {},
       {t(internal)},
#if BUILDFLAG(IS_MAC)
       create_pk
#else
       plat_ui
#endif
      },
      // MakeCredential with attachment=undefined also shows the 'Create a
      // passkey' step on macOS. On other OSes, we show mechanism selection.
      {L,
       mc,
       {usb, internal},
       {},
       {},
       {t(internal), t(usb)},
#if BUILDFLAG(IS_MAC)
       create_pk
#else
       mss
#endif
      },

      // If the Windows API is available without caBLE, it should activate.
      {L, mc, {}, {has_winapi}, {}, {winapi}, plat_ui},
      {L, ga, {}, {has_winapi}, {}, {winapi}, plat_ui},
      // ... even if, somehow, there's another transport.
      {L, mc, {usb}, {has_winapi}, {}, {winapi, t(usb)}, plat_ui},
      {L, ga, {usb}, {has_winapi}, {}, {winapi, t(usb)}, plat_ui},

      // A caBLEv1 extension should cause us to go directly to caBLE.
      {L, ga, {usb, cable}, {v1}, {}, {t(usb), t(cable)}, cable_ui},
      // A caBLEv2 extension should cause us to go directly to caBLE, but also
      // show the AOA option.
      {L,
       ga,
       {usb, aoa, cable},
       {v2},
       {},
       {t(usb), t(aoa), t(cable)},
       cable_ui},

      // If there are linked phones then AOA doesn't show up, but the phones do,
      // and sorted. The selection sheet should show.
      {L,
       mc,
       {usb, aoa, cable},
       {},
       {"a", "b"},
       {p("a"), p("b"), add, t(usb)},
       mss},
      {L,
       ga,
       {usb, aoa, cable},
       {},
       {"a", "b"},
       {p("a"), p("b"), add, t(usb)},
       mss},

      // If this is a Conditional UI request, don't offer the platform
      // authenticator.
      {L, ga, {usb, internal}, {c_ui}, {}, {t(usb)}, usb_ui},
      {L,
       ga,
       {usb, internal, cable},
       {c_ui},
       {"a"},
       {p("a"), add, t(usb)},
       mss},

      // On Windows, mc with rk=required jumps to the platform UI when caBLE
      // isn't an option. The case where caBLE is possible is tested below.
      {L, mc, {}, {has_winapi, rk}, {}, {winapi}, plat_ui},
      // For rk=discouraged, always jump to Windows UI.
      {L, mc, {cable}, {has_winapi}, {}, {winapi, add}, plat_ui},
      {L, mc, {}, {has_winapi}, {}, {winapi}, plat_ui},

      // On Windows, ga with an empty allow list goes to the platform UI unless
      // caBLE is an option and resident-key is required, which is tested below.
      {L, ga, {}, {has_winapi, empty_al}, {}, {winapi}, plat_ui},
      // With a non-empty allow list containing non phone credentials, always
      // jump to Windows UI.
      {L, ga, {cable}, {has_winapi}, {}, {winapi, add}, plat_ui},
      {L, ga, {}, {has_winapi}, {}, {winapi}, plat_ui},
      // Except when the request is legacy cable.
      {L, ga, {cable, aoa}, {has_winapi, v1}, {}, {winapi, t(cable)}, cable_ui},
      {L,
       ga,
       {cable, aoa},
       {has_winapi, v2},
       {},
       {winapi, t(aoa), t(cable)},
       cable_ui},

      // QR code first: Make credential should jump to the QR code with RK=true.
      {L, mc, {usb, internal, cable}, {rk}, {}, {add, t(internal), t(usb)}, qr},
      // Unless there is a phone paired already.
      {L,
       mc,
       {usb, internal, cable},
       {rk},
       {"a"},
       {p("a"), add, t(internal), t(usb)},
       mss},
      // Or if attachment=any
      {L,
       mc,
       {usb, internal, cable},
       {rk, att_any},
       {},
       {add, t(internal), t(usb)},
       mss},
      // But not for any attachment, like platform
      {L,
       mc,
       {usb, internal, cable},
       {rk, att_xplat},
       {},
       {add, t(internal), t(usb)},
       qr},
      // If RK=false, go to the default for the platform instead.
      {
          L,
          mc,
          {usb, internal, cable},
          {},
          {},
          {add, t(internal), t(usb)},
#if BUILDFLAG(IS_MAC)
          create_pk,
#else
          mss,
#endif
      },
      // Windows should also jump to the QR code first.
      {L, mc, {cable}, {rk, has_winapi}, {}, {winapi, add}, qr},

      // QR code first: Get assertion should jump to the QR code with empty
      // allow-list.
      {L,
       ga,
       {usb, internal, cable},
       {empty_al},
       {},
       {add, t(internal), t(usb)},
       qr},
      // And if the allow list only contains phones.
      {L,
       ga,
       {internal, cable},
       {only_hybrid_or_internal},
       {},
       {add, t(internal)},
       qr},
      // Unless there is a phone paired already.
      {L,
       ga,
       {usb, internal, cable},
       {empty_al},
       {"a"},
       {p("a"), add, t(internal), t(usb)},
       mss},
      // Or a recognized platform credential.
      {L,
       ga,
       {usb, internal, cable},
       {empty_al, has_plat},
       {},
       {add, t(internal), t(usb)},
       plat_ui},
      // Ignore the platform credential for conditional ui requests
      {L,
       ga,
       {usb, internal, cable},
       {c_ui, empty_al, has_plat},
       {},
       {add, t(usb)},
       qr},
      // If there is an allow-list containing USB, go to transport selection
      // instead.
      {L, ga, {usb, internal, cable}, {}, {}, {add, t(internal), t(usb)}, mss},
      // Windows should also jump to the QR code first.
      {L, ga, {cable}, {empty_al, has_winapi}, {}, {winapi, add}, qr},
      // Unless there is a recognized platform credential.
      {L,
       ga,
       {cable},
       {empty_al, has_winapi, has_plat},
       {},
       {winapi, add},
       plat_ui},
      // For <=Win 10, we can't tell if there is a credential or not. Show the
      // mechanism selection screen instead.
      {L,
       ga,
       {cable},
       {empty_al, has_winapi, maybe_plat},
       {},
       {winapi, add},
       mss},

      // Phone confirmation sheet: Get assertion should jump to it if there is a
      // single phone paired.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal},
       {"a"},
       {p("a"), add, t(internal)},
       pconf},
      // Even on Windows.
      {L,
       ga,
       {cable},
       {only_hybrid_or_internal, has_winapi},
       {"a"},
       {winapi, p("a"), add},
       pconf},
      // Unless there is a recognized platform credential.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal, has_plat},
       {"a"},
       {p("a"), add, t(internal)},
       plat_ui},
      // Or a USB credential.
      {L,
       ga,
       {usb, cable, internal},
       {},
       {"a"},
       {p("a"), add, t(internal), t(usb)},
       mss},
      // Or this is a conditional UI request.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal, c_ui},
       {"a"},
       {p("a"), add},
       mss},
      // Go to the mechanism selection screen if there are more phones paired.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal},
       {"a", "b"},
       {p("a"), p("b"), add, t(internal)},
       mss},
#undef L
  };

#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);
#endif

  // On Windows, all the tests are run twice. Once to check that, when Windows
  // has hybrid support, we always jump the Windows, and then to test the prior
  // behaviour.
  for (const bool windows_has_hybrid : {
         false
#if BUILDFLAG(IS_WIN)
             ,
             true
#endif
       }) {
    for (const auto& test : kTests) {
      SCOPED_TRACE(static_cast<int>(test.expected_first_step));
      SCOPED_TRACE(
          (SetToString<TransportAvailabilityParam,
                       TransportAvailabilityParamToString>(test.params)));
      SCOPED_TRACE(
          (SetToString<device::FidoTransportProtocol, device::ToString>(
              test.transports)));
      SCOPED_TRACE(RequestTypeToString(test.request_type));
      SCOPED_TRACE(testing::Message() << "At line number: " << test.line_num);

#if BUILDFLAG(IS_WIN)
      fake_win_webauthn_api.set_version(windows_has_hybrid ? 6 : 4);
      SCOPED_TRACE(windows_has_hybrid);
#endif

      TransportAvailabilityInfo transports_info;
      transports_info.is_ble_powered = true;
      transports_info.request_type = test.request_type;
      transports_info.available_transports = test.transports;

      if (base::Contains(test.params,
                         TransportAvailabilityParam::kHasPlatformCredential)) {
        transports_info.has_platform_authenticator_credential =
            device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential;
      } else if (base::Contains(
                     test.params,
                     TransportAvailabilityParam::kMaybeHasPlatformCredential)) {
        transports_info.has_platform_authenticator_credential =
            device::FidoRequestHandlerBase::RecognizedCredential::kUnknown;
      } else {
        transports_info.has_platform_authenticator_credential =
            device::FidoRequestHandlerBase::RecognizedCredential::
                kNoRecognizedCredential;
      }

      if (base::Contains(test.params,
                         TransportAvailabilityParam::kOneRecognizedCred)) {
        transports_info.recognized_platform_authenticator_credentials = {
            kCred1};
      } else if (base::Contains(
                     test.params,
                     TransportAvailabilityParam::kTwoRecognizedCreds)) {
        transports_info.recognized_platform_authenticator_credentials = {
            kCred1, kCred2};
      }
      transports_info.has_empty_allow_list = base::Contains(
          test.params, TransportAvailabilityParam::kEmptyAllowList);
      transports_info.is_only_hybrid_or_internal = base::Contains(
          test.params, TransportAvailabilityParam::kOnlyHybridOrInternal);

      if (base::Contains(
              test.params,
              TransportAvailabilityParam::kHasWinNativeAuthenticator) ||
          windows_has_hybrid) {
        transports_info.has_win_native_api_authenticator = true;
        transports_info.win_native_ui_shows_resident_credential_notice = true;
      }
      transports_info.resident_key_requirement =
          base::Contains(test.params,
                         TransportAvailabilityParam::kRequireResidentKey)
              ? device::ResidentKeyRequirement::kRequired
              : device::ResidentKeyRequirement::kDiscouraged;
      if (base::Contains(test.params,
                         TransportAvailabilityParam::kAttachmentAny)) {
        CHECK(transports_info.request_type == RequestType::kMakeCredential);
        transports_info.make_credential_attachment =
            device::AuthenticatorAttachment::kAny;
      }
      if (base::Contains(
              test.params,
              TransportAvailabilityParam::kAttachmentCrossPlatform)) {
        CHECK(transports_info.request_type == RequestType::kMakeCredential);
        CHECK(!transports_info.make_credential_attachment.has_value());
        transports_info.make_credential_attachment =
            device::AuthenticatorAttachment::kCrossPlatform;
      }
      if (!transports_info.make_credential_attachment.has_value() &&
          transports_info.request_type == RequestType::kMakeCredential) {
        transports_info.make_credential_attachment =
            device::AuthenticatorAttachment::kPlatform;
      }

      AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);

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

      if (has_v2_cable_extension.has_value() || !test.phone_names.empty() ||
          base::Contains(test.transports,
                         device::FidoTransportProtocol::kHybrid)) {
        std::vector<AuthenticatorRequestDialogModel::PairedPhone> phones;
        for (const auto& name : test.phone_names) {
          std::array<uint8_t, device::kP256X962Length> public_key = {0};
          public_key[0] = base::checked_cast<uint8_t>(phones.size());
          phones.emplace_back(name, /*contact_id=*/0, public_key);
        }
        model.set_cable_transport_info(has_v2_cable_extension,
                                       std::move(phones), base::DoNothing(),
                                       absl::nullopt);
      }

      bool is_conditional_ui = base::Contains(
          test.params, TransportAvailabilityParam::kIsConditionalUI);
      model.StartFlow(std::move(transports_info), is_conditional_ui);
      if (is_conditional_ui) {
        EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
        model.TransitionToModalWebAuthnRequest();
      }

      if (windows_has_hybrid) {
        // caBLEv1 and server-link are the only cases that Windows _doesn't_
        // handle when it has hybrid support because those are legacy protocol
        // variants.
        if (test.expected_first_step != cable_ui) {
          EXPECT_EQ(plat_ui, model.current_step());
        }
        continue;
      }

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
}

#if BUILDFLAG(IS_WIN)
TEST_F(AuthenticatorRequestDialogModelTest, WinCancel) {
  // Simulate the user canceling the Windows native UI, both with and without
  // that UI being immediately triggered. If it was immediately triggered then
  // canceling it should show the mechanism selection UI.

  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);

  for (const int win_webauthn_api_version : {4, 6}) {
    fake_win_webauthn_api.set_version(win_webauthn_api_version);
    for (const bool is_passkey_request : {false, true}) {
      SCOPED_TRACE(is_passkey_request);

      AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
      tai.request_type = device::FidoRequestType::kMakeCredential;
      tai.has_win_native_api_authenticator = true;
      tai.win_native_ui_shows_resident_credential_notice = true;
      tai.available_transports.insert(device::FidoTransportProtocol::kHybrid);
      tai.resident_key_requirement =
          is_passkey_request ? device::ResidentKeyRequirement::kRequired
                             : device::ResidentKeyRequirement::kDiscouraged;
      tai.is_ble_powered = true;

      AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
      model.saved_authenticators().AddAuthenticator(
          AuthenticatorReference("ID", AuthenticatorTransport::kInternal,
                                 device::AuthenticatorType::kWinNative));
      model.set_cable_transport_info(absl::nullopt, {}, base::DoNothing(),
                                     "fido:/1234");

      model.StartFlow(std::move(tai),
                      /*is_conditional_mediation=*/false);

      if (!is_passkey_request || win_webauthn_api_version >= 6) {
        // The Windows native UI should have been triggered.
        EXPECT_EQ(model.current_step(), Step::kNotStarted);

        if (win_webauthn_api_version >= 6) {
          // Windows handles hybrid itself starting with this version, so
          // canceling shouldn't try to show Chrome UI.
          EXPECT_FALSE(model.OnWinUserCancelled());
          continue;
        } else {
          // Canceling the Windows native UI should be handled.
          EXPECT_TRUE(model.OnWinUserCancelled());
        }
      }

      // The mechanism selection sheet should now be showing.
      EXPECT_EQ(model.current_step(), is_passkey_request
                                          ? Step::kCableV2QRCode
                                          : Step::kMechanismSelection);
      // Canceling the Windows UI ends the request because the user must have
      // selected the Windows option first.
      EXPECT_FALSE(model.OnWinUserCancelled());
    }
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, WinNoPlatformAuthenticator) {
  AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
  tai.request_type = device::FidoRequestType::kMakeCredential;
  tai.request_is_internal_only = true;
  tai.win_is_uvpaa = false;
  tai.has_win_native_api_authenticator = true;
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  model.StartFlow(std::move(tai), /*is_conditional_mediation=*/false);
  EXPECT_EQ(
      model.current_step(),
      AuthenticatorRequestDialogModel::Step::kErrorWindowsHelloNotEnabled);
  EXPECT_FALSE(model.offer_try_again_in_ui());
}
#endif

TEST_F(AuthenticatorRequestDialogModelTest, NoAvailableTransports) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  model.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.StartFlow(TransportAvailabilityInfo(),
                  /*is_conditional_mediation=*/false);
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
    transports_info.available_transports = {AuthenticatorTransport::kHybrid};
    transports_info.is_off_the_record_context =
        test.profile == Profile::INCOGNITO;

    AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);

    std::array<uint8_t, device::kP256X962Length> public_key = {0};
    std::vector<AuthenticatorRequestDialogModel::PairedPhone> phones(
        {{"phone", /*contact_id=*/0, public_key}});
    model.set_cable_transport_info(/*extension_is_v2=*/absl::nullopt,
                                   std::move(phones), base::DoNothing(),
                                   absl::nullopt);

    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
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
    AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
    model.AddObserver(&mock_observer);

    TransportAvailabilityInfo transports_info;
    transports_info.request_type = RequestType::kGetAssertion;
    transports_info.available_transports = kAllTransportsWithoutCable;

    EXPECT_CALL(mock_observer, OnStepTransition());
    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
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
      {AuthenticatorTransport::kHybrid, Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.request_type = RequestType::kGetAssertion;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = true;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, {}, base::DoNothing(), absl::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
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
      {AuthenticatorTransport::kHybrid, Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.request_type = RequestType::kGetAssertion;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = false;
    transports_info.is_ble_powered = false;

    testing::NiceMock<MockDialogModelObserver> mock_observer;
    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
    model.AddObserver(&mock_observer);
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, {}, base::DoNothing(), absl::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);

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
      {AuthenticatorTransport::kHybrid, Step::kCableActivate},
  };

  for (const auto test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.request_type = RequestType::kGetAssertion;
    transports_info.available_transports = {test_case.transport};
    transports_info.can_power_on_ble_adapter = true;
    transports_info.is_ble_powered = false;

    BluetoothAdapterPowerOnCallbackReceiver power_receiver;
    AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
    model.SetBluetoothAdapterPowerOnCallback(power_receiver.GetCallback());
    model.set_cable_transport_info(true, {}, base::DoNothing(), absl::nullopt);
    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);

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
       RequestCallbackForWindowsAuthenticatorIsInvokedAutomatically) {
  constexpr char kWinAuthenticatorId[] = "some_authenticator_id";

  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
  transports_info.request_type = RequestType::kMakeCredential;
  transports_info.available_transports = {};
  transports_info.has_win_native_api_authenticator = true;

  std::vector<std::string> dispatched_authenticator_ids;
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  model.SetRequestCallback(base::BindRepeating(
      [](std::vector<std::string>* ids, const std::string& authenticator_id) {
        ids->push_back(authenticator_id);
      },
      &dispatched_authenticator_ids));

  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      kWinAuthenticatorId, AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kWinNative));
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/false);

  EXPECT_TRUE(model.should_dialog_be_closed());
  task_environment()->RunUntilIdle();
  EXPECT_THAT(dispatched_authenticator_ids, ElementsAre(kWinAuthenticatorId));
}

TEST_F(AuthenticatorRequestDialogModelTest,
       ConditionalUINoRecognizedCredential) {
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);

  int preselect_num_called = 0;
  model.SetAccountPreselectedCallback(base::BindRepeating(
      [](int* i, std::vector<uint8_t> credential_id) {
        EXPECT_EQ(credential_id, std::vector<uint8_t>({1, 2, 3, 4}));
        ++(*i);
      },
      &preselect_num_called));
  int request_num_called = 0;
  model.SetRequestCallback(base::BindRepeating(
      [](int* i, const std::string& authenticator_id) { ++(*i); },
      &request_num_called));
  model.saved_authenticators().AddAuthenticator(
      AuthenticatorReference(/*device_id=*/"authenticator",
                             AuthenticatorTransport::kUsbHumanInterfaceDevice,
                             device::AuthenticatorType::kOther));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"authenticator", AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  TransportAvailabilityInfo transports_info;
  transports_info.available_transports = kAllTransports;
  transports_info.has_platform_authenticator_credential = device::
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/true);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
  EXPECT_TRUE(model.should_dialog_be_closed());
  EXPECT_EQ(preselect_num_called, 0);
  EXPECT_EQ(request_num_called, 0);
}

TEST_F(AuthenticatorRequestDialogModelTest, ConditionalUIRecognizedCredential) {
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  int preselect_num_called = 0;
  model.SetAccountPreselectedCallback(base::BindRepeating(
      [](int* i, std::vector<uint8_t> credential_id) {
        EXPECT_EQ(credential_id, std::vector<uint8_t>({0}));
        ++(*i);
      },
      &preselect_num_called));
  int request_num_called = 0;
  model.SetRequestCallback(base::BindRepeating(
      [](int* i, const std::string& authenticator_id) {
        EXPECT_EQ(authenticator_id, "internal");
        ++(*i);
      },
      &request_num_called));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"usb", AuthenticatorTransport::kUsbHumanInterfaceDevice,
      device::AuthenticatorType::kOther));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"internal", AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  TransportAvailabilityInfo transports_info;
  transports_info.available_transports = kAllTransports;
  transports_info.has_platform_authenticator_credential = device::
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
  transports_info.recognized_platform_authenticator_credentials = {kCred1,
                                                                   kCred2};
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/true);
  EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
  EXPECT_TRUE(model.should_dialog_be_closed());
  EXPECT_EQ(request_num_called, 0);

  // After preselecting an account, the request should be dispatched to the
  // platform authenticator.
  model.OnAccountPreselected(kCred1.cred_id);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(preselect_num_called, 1);
  EXPECT_EQ(request_num_called, 1);
}

// Tests that cancelling a Conditional UI request that has completed restarts
// it.
TEST_F(AuthenticatorRequestDialogModelTest, ConditionalUICancelRequest) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  model.AddObserver(&mock_observer);
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"internal", AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.StartFlow(std::move(TransportAvailabilityInfo()),
                  /*is_conditional_mediation=*/true);
  EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Cancel an ongoing request (as if e.g. the user clicked the accept button).
  // The request should be restarted.
  EXPECT_CALL(mock_observer, OnStartOver());
  EXPECT_CALL(mock_observer, OnStepTransition()).Times(2);
  model.SetCurrentStepForTesting(Step::kKeyAlreadyRegistered);
  model.Cancel();
  EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  model.RemoveObserver(&mock_observer);
}

#if BUILDFLAG(IS_WIN)
// Tests that cancelling the Windows Platform authenticator during a Conditional
// UI request restarts it.
TEST_F(AuthenticatorRequestDialogModelTest, ConditionalUIWindowsCancel) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  model.AddObserver(&mock_observer);
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"internal", AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.StartFlow(std::move(TransportAvailabilityInfo()),
                  /*is_conditional_mediation=*/true);
  EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Simulate the Windows authenticator cancelling.
  EXPECT_CALL(mock_observer, OnStepTransition());
  EXPECT_CALL(mock_observer, OnStartOver());
  model.OnWinUserCancelled();
  EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  model.RemoveObserver(&mock_observer);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(AuthenticatorRequestDialogModelTest, PreSelectWithEmptyAllowList) {
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  int preselect_num_called = 0;
  model.SetAccountPreselectedCallback(base::BindLambdaForTesting(
      [&preselect_num_called](std::vector<uint8_t> credential_id) {
        EXPECT_EQ(credential_id, std::vector<uint8_t>({0}));
        ++preselect_num_called;
      }));
  int request_num_called = 0;
  model.SetRequestCallback(base::BindLambdaForTesting(
      [&request_num_called](const std::string& authenticator_id) {
        EXPECT_EQ(authenticator_id, "internal-authenticator");
        ++request_num_called;
      }));

  model.saved_authenticators().AddAuthenticator(
      AuthenticatorReference(/*device_id=*/"usb-authenticator",
                             AuthenticatorTransport::kUsbHumanInterfaceDevice,
                             device::AuthenticatorType::kOther));
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"internal-authenticator", AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  TransportAvailabilityInfo transports_info;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.available_transports = kAllTransports;
  transports_info.has_empty_allow_list = true;
  transports_info.has_platform_authenticator_credential = device::
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
  transports_info.recognized_platform_authenticator_credentials = {kCred1,
                                                                   kCred2};
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/false);
  EXPECT_EQ(model.current_step(), Step::kPreSelectAccount);
  EXPECT_EQ(request_num_called, 0);

  // After preselecting an account, the request should be dispatched to the
  // platform authenticator.
  model.OnAccountPreselected(kCred1.cred_id);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(preselect_num_called, 1);
  EXPECT_EQ(request_num_called, 1);
}

TEST_F(AuthenticatorRequestDialogModelTest, ContactPriorityPhone) {
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  std::vector<AuthenticatorRequestDialogModel::PairedPhone> phones(
      {{"phone", /*contact_id=*/0, /*public_key_x962=*/{{0}}}});
  model.set_cable_transport_info(/*extension_is_v2=*/absl::nullopt,
                                 std::move(phones), base::DoNothing(),
                                 absl::nullopt);
  TransportAvailabilityInfo transports_info;
  transports_info.is_ble_powered = true;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.available_transports = {AuthenticatorTransport::kHybrid};
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/false);
  model.ContactPriorityPhone();
  EXPECT_EQ(model.current_step(), Step::kCableActivate);
  EXPECT_EQ(model.selected_phone_name(), "phone");
}

#if BUILDFLAG(IS_MAC)
TEST_F(AuthenticatorRequestDialogModelTest, BluetoothPermissionPrompt) {
  // When BLE permission is denied on macOS, we should jump to the sheet that
  // explains that if the user tries to use a linked phone or tries to show the
  // QR code.
  for (const bool ble_access_denied : {false, true}) {
    for (const bool click_specific_phone : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "ble_access_denied=" << ble_access_denied);
      SCOPED_TRACE(::testing::Message()
                   << "click_specific_phone=" << click_specific_phone);

      AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
      std::vector<AuthenticatorRequestDialogModel::PairedPhone> phones(
          {{"phone", /*contact_id=*/0, /*public_key_x962=*/{{0}}}});
      model.set_cable_transport_info(/*extension_is_v2=*/absl::nullopt,
                                     std::move(phones), base::DoNothing(),
                                     absl::nullopt);
      TransportAvailabilityInfo transports_info;
      transports_info.is_ble_powered = true;
      transports_info.ble_access_denied = ble_access_denied;
      transports_info.request_type = device::FidoRequestType::kGetAssertion;
      transports_info.available_transports = {
          AuthenticatorTransport::kHybrid,
          AuthenticatorTransport::kUsbHumanInterfaceDevice};
      model.StartFlow(std::move(transports_info),
                      /*is_conditional_mediation=*/false);

      base::ranges::find_if(
          model.mechanisms(),
          [click_specific_phone](const auto& m) -> bool {
            if (click_specific_phone) {
              return absl::holds_alternative<
                  AuthenticatorRequestDialogModel::Mechanism::Phone>(m.type);
            } else {
              return absl::holds_alternative<
                  AuthenticatorRequestDialogModel::Mechanism::AddPhone>(m.type);
            }
          })
          ->callback.Run();

      if (ble_access_denied) {
        EXPECT_EQ(model.current_step(), Step::kBlePermissionMac);
      } else if (click_specific_phone) {
        EXPECT_EQ(model.current_step(), Step::kCableActivate);
      } else {
        EXPECT_EQ(model.current_step(), Step::kCableV2QRCode);
      }
    }
  }
}
#endif

TEST_F(AuthenticatorRequestDialogModelTest, AdvanceThroughCableV2States) {
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  model.set_cable_transport_info(/*extension_is_v2=*/absl::nullopt, {},
                                 base::DoNothing(), absl::nullopt);
  TransportAvailabilityInfo transports_info;
  transports_info.is_ble_powered = true;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.available_transports = {AuthenticatorTransport::kHybrid};
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/false);

  model.OnCableEvent(device::cablev2::Event::kPhoneConnected);
  EXPECT_EQ(model.current_step(), Step::kCableV2Connecting);
  model.OnCableEvent(device::cablev2::Event::kBLEAdvertReceived);
  EXPECT_EQ(model.current_step(), Step::kCableV2Connecting);
  model.OnCableEvent(device::cablev2::Event::kReady);
  // kCableV2Connecting won't flash by too quickly, so it'll still be showing.
  EXPECT_EQ(model.current_step(), Step::kCableV2Connecting);

  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_EQ(model.current_step(), Step::kCableV2Connected);
}

TEST_F(AuthenticatorRequestDialogModelTest,
       AdvanceThroughCableV2StatesStopTimer) {
  AuthenticatorRequestDialogModel model(/*render_frame_host=*/nullptr);
  model.set_cable_transport_info(/*extension_is_v2=*/absl::nullopt, {},
                                 base::DoNothing(), absl::nullopt);
  TransportAvailabilityInfo transports_info;
  transports_info.is_ble_powered = true;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.available_transports = {AuthenticatorTransport::kHybrid};
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/false);

  model.OnCableEvent(device::cablev2::Event::kPhoneConnected);
  EXPECT_EQ(model.current_step(), Step::kCableV2Connecting);
  model.OnCableEvent(device::cablev2::Event::kBLEAdvertReceived);
  EXPECT_EQ(model.current_step(), Step::kCableV2Connecting);
  model.OnCableEvent(device::cablev2::Event::kReady);
  // kCableV2Connecting won't flash by too quickly, so it'll still be showing.
  EXPECT_EQ(model.current_step(), Step::kCableV2Connecting);

  // Moving to a different step should stop the timer so that kCableV2Connected
  // never shows.
  model.SetCurrentStepForTesting(Step::kCableActivate);

  task_environment()->FastForwardBy(base::Seconds(10));
  EXPECT_EQ(model.current_step(), Step::kCableActivate);
}
