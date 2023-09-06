// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/to_vector.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

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

class RequestCallbackReceiver {
 public:
  base::RepeatingCallback<void(const std::string&)> Callback() {
    return base::BindRepeating(&RequestCallbackReceiver::OnRequest,
                               weak_factory_.GetWeakPtr());
  }

  std::string WaitForResult() {
    if (!authenticator_id_) {
      run_loop_->Run();
    }
    std::string ret = std::move(*authenticator_id_);
    authenticator_id_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
    return ret;
  }

 private:
  void OnRequest(const std::string& authenticator_id) {
    authenticator_id_ = authenticator_id;
    run_loop_->Quit();
  }
  absl::optional<std::string> authenticator_id_;
  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
  base::WeakPtrFactory<RequestCallbackReceiver> weak_factory_{this};
};

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
  kOnePhoneRecognizedCred,
  kTwoPhoneRecognizedCred,
  kEmptyAllowList,
  kOnlyInternal,
  kOnlyHybridOrInternal,
  kHasWinNativeAuthenticator,
  kHasCableV1Extension,
  kHasCableV2Extension,
  kRequireResidentKey,
  kIsConditionalUI,
  kAttachmentAny,
  kAttachmentCrossPlatform,
  kBleDisabled,
  kBleAccessDenied,
  kHasICloudKeychain,
  kHasICloudKeychainCreds,
  kCreateInICloudKeychain,
  kNoTouchId,
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
    case TransportAvailabilityParam::kOnePhoneRecognizedCred:
      return "kOnePhoneRecognizedCred";
    case TransportAvailabilityParam::kTwoPhoneRecognizedCred:
      return "kTwoPhoneRecognizedCred";
    case TransportAvailabilityParam::kEmptyAllowList:
      return "kEmptyAllowList";
    case TransportAvailabilityParam::kOnlyInternal:
      return "kOnlyInternal";
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
    case TransportAvailabilityParam::kBleDisabled:
      return "kBleDisabled";
    case TransportAvailabilityParam::kBleAccessDenied:
      return "kBleAccessDenied";
    case TransportAvailabilityParam::kHasICloudKeychain:
      return "kHasICloudKeychain";
    case TransportAvailabilityParam::kHasICloudKeychainCreds:
      return "kHasICloudKeychainCreds";
    case TransportAvailabilityParam::kCreateInICloudKeychain:
      return "kCreateInICloudKeychain";
    case TransportAvailabilityParam::kNoTouchId:
      return "kNoTouchId";
  }
}

template <typename T, base::StringPiece (*F)(T)>
std::string SetToString(base::flat_set<T> s) {
  return base::JoinString(base::test::ToVector(s, F), ", ");
}

std::unique_ptr<device::cablev2::Pairing> GetPairingFromSync() {
  auto pairing = std::make_unique<device::cablev2::Pairing>();
  pairing->name = "Phone from sync";
  pairing->from_sync_deviceinfo = true;
  return pairing;
}

std::unique_ptr<device::cablev2::Pairing> GetPairingFromQR() {
  auto pairing = std::make_unique<device::cablev2::Pairing>();
  pairing->name = "Phone from QR";
  pairing->from_sync_deviceinfo = false;
  return pairing;
}

const device::PublicKeyCredentialUserEntity kUser1({1, 2, 3, 4},
                                                   "A",
                                                   absl::nullopt);
const device::PublicKeyCredentialUserEntity kUser2({5, 6, 7, 8},
                                                   "B",
                                                   absl::nullopt);
const device::PublicKeyCredentialUserEntity kPhoneUser1({9, 0, 1, 2},
                                                        "C",
                                                        absl::nullopt);
const device::PublicKeyCredentialUserEntity kPhoneUser2({3, 4, 5, 6},
                                                        "D",
                                                        absl::nullopt);

const device::DiscoverableCredentialMetadata
    kCred1(device::AuthenticatorType::kOther, "rp.com", {0}, kUser1);
const device::DiscoverableCredentialMetadata kCred1FromICloudKeychain(
    device::AuthenticatorType::kICloudKeychain,
    "rp.com",
    {4},
    kUser1);
const device::DiscoverableCredentialMetadata
    kCred2(device::AuthenticatorType::kOther, "rp.com", {1}, kUser2);
const device::DiscoverableCredentialMetadata
    kPhoneCred1(device::AuthenticatorType::kPhone, "rp.com", {2}, kPhoneUser1);
const device::DiscoverableCredentialMetadata
    kPhoneCred2(device::AuthenticatorType::kPhone, "rp.com", {3}, kPhoneUser2);
const device::DiscoverableCredentialMetadata
    kWinCred1(device::AuthenticatorType::kWinNative, "rp.com", {0}, kUser1);
const device::DiscoverableCredentialMetadata
    kWinCred2(device::AuthenticatorType::kWinNative, "rp.com", {1}, kUser2);

AuthenticatorRequestDialogModel::Mechanism::CredentialInfo CredentialInfoFrom(
    const device::DiscoverableCredentialMetadata& metadata) {
  return AuthenticatorRequestDialogModel::Mechanism::CredentialInfo(
      metadata.source, metadata.user.id);
}

}  // namespace

#if BUILDFLAG(IS_MAC)
#define NEW_UI
#endif

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
  const auto cred1 = CredentialInfoFrom(kCred1);
  const auto cred2 = CredentialInfoFrom(kCred2);
  const auto phonecred1 = CredentialInfoFrom(kPhoneCred1);
  const auto phonecred2 = CredentialInfoFrom(kPhoneCred2);
  const auto ickc_cred1 = CredentialInfoFrom(kCred1FromICloudKeychain);
  const auto wincred1 = CredentialInfoFrom(kWinCred1);
  const auto wincred2 = CredentialInfoFrom(kWinCred2);
  const auto v1 = TransportAvailabilityParam::kHasCableV1Extension;
  const auto v2 = TransportAvailabilityParam::kHasCableV2Extension;
  const auto has_winapi =
      TransportAvailabilityParam::kHasWinNativeAuthenticator;
  const auto has_plat = TransportAvailabilityParam::kHasPlatformCredential;
  const auto maybe_plat =
      TransportAvailabilityParam::kMaybeHasPlatformCredential;
  const auto one_cred = TransportAvailabilityParam::kOneRecognizedCred;
  const auto two_cred = TransportAvailabilityParam::kTwoRecognizedCreds;
  const auto one_phone_cred =
      TransportAvailabilityParam::kOnePhoneRecognizedCred;
  const auto two_phone_cred =
      TransportAvailabilityParam::kTwoPhoneRecognizedCred;
  const auto empty_al = TransportAvailabilityParam::kEmptyAllowList;
  const auto only_internal = TransportAvailabilityParam::kOnlyInternal;
  const auto only_hybrid_or_internal =
      TransportAvailabilityParam::kOnlyHybridOrInternal;
  const auto rk = TransportAvailabilityParam::kRequireResidentKey;
  const auto c_ui = TransportAvailabilityParam::kIsConditionalUI;
  const auto att_any = TransportAvailabilityParam::kAttachmentAny;
  const auto att_xplat = TransportAvailabilityParam::kAttachmentCrossPlatform;
  const auto ble_off = TransportAvailabilityParam::kBleDisabled;
  const auto ble_denied = TransportAvailabilityParam::kBleAccessDenied;
  [[maybe_unused]] const auto has_ickc =
      TransportAvailabilityParam::kHasICloudKeychain;
  [[maybe_unused]] const auto create_ickc =
      TransportAvailabilityParam::kCreateInICloudKeychain;
  [[maybe_unused]] const auto no_touchid =
      TransportAvailabilityParam::kNoTouchId;
  [[maybe_unused]] const auto ickc_creds =
      TransportAvailabilityParam::kHasICloudKeychainCreds;
  using c = AuthenticatorRequestDialogModel::Mechanism::Credential;
  using t = AuthenticatorRequestDialogModel::Mechanism::Transport;
  using p = AuthenticatorRequestDialogModel::Mechanism::Phone;
  const auto winapi = AuthenticatorRequestDialogModel::Mechanism::WindowsAPI();
  const auto add = AuthenticatorRequestDialogModel::Mechanism::AddPhone();
  [[maybe_unused]] const auto ickc =
      AuthenticatorRequestDialogModel::Mechanism::ICloudKeychain();
  const auto usb_ui = Step::kUsbInsertAndActivate;
  const auto mss = Step::kMechanismSelection;
  const auto plat_ui = Step::kNotStarted;
  const auto cable_ui = Step::kCableActivate;
  [[maybe_unused]] const auto create_pk = Step::kCreatePasskey;
  [[maybe_unused]] const auto use_pk = Step::kPreSelectSingleAccount;
  [[maybe_unused]] const auto use_pk_multi = Step::kPreSelectAccount;
  const auto qr = Step::kCableV2QRCode;
  const auto pconf = Step::kPhoneConfirmationSheet;
  const auto hero = Step::kSelectPriorityMechanism;
  using psync = base::StrongAlias<class PhoneFromSyncTag, std::string>;
  using pqr = base::StrongAlias<class PhoneFromQrTag, std::string>;
  using PhoneVariant = absl::variant<psync, pqr>;

  struct Test {
    int line_num;
    RequestType request_type;
    base::flat_set<AuthenticatorTransport> transports;
    base::flat_set<TransportAvailabilityParam> params;
    std::vector<PhoneVariant> phones;
    std::vector<AuthenticatorRequestDialogModel::Mechanism::Type>
        expected_mechanisms;
    Step expected_first_step;
  };

#define L __LINE__
  // clang-format off
  Test kTests[]{
      // If there's only a single mechanism, it should activate.
      {L, mc, {usb}, {}, {}, {t(usb)}, usb_ui},
      {L, ga, {usb}, {}, {}, {t(usb)}, usb_ui},
#if defined(NEW_UI)
      {L, ga, {usb, cable}, {}, {}, {add}, qr},
      {L, ga, {usb, cable}, {}, {}, {add}, qr},
#else
      // ... otherwise show the selection sheet.
      {L, ga, {usb, cable}, {}, {}, {add, t(usb)}, mss},
      {L, ga, {usb, cable}, {}, {}, {add, t(usb)}, mss},
#endif

#if defined(NEW_UI)
      // If the platform authenticator has a credential it should activate.
      {L,
       ga,
       {usb, internal},
       {has_plat, one_cred},
       {},
       {c(cred1), t(usb)},
#if BUILDFLAG(IS_MAC)
       plat_ui
#else
       use_pk
#endif
      },
#if BUILDFLAG(IS_MAC)
       // Without Touch ID, the profile authenticator will show a confirmation
       // prompt.
      {L, ga, {usb, internal}, {has_plat, one_cred, no_touchid}, {},
       {c(cred1), t(usb)}, use_pk},
#endif
      // Even with an empty allow list.
      {L,
       ga,
       {usb, internal},
       {has_plat, one_cred, empty_al},
       {},
       {c(cred1), t(usb)},
       hero},
      // Two credentials shows mechanism selection.
      {L,
       ga,
       {usb, internal},
       {has_plat, two_cred, empty_al},
       {},
       {c(cred1), c(cred2), t(usb)},
       mss},
#else
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
#endif

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
#if defined(NEW_UI)
      // ...even if there are discovered Windows credentials.
      {L, ga, {}, {has_winapi, one_cred}, {}, {c(wincred1), winapi}, plat_ui},
#endif

      // A caBLEv1 extension should cause us to go directly to caBLE.
#if defined(NEW_UI)
      {L, ga, {usb, cable}, {v1}, {}, {t(cable), t(usb)}, cable_ui},
      // A caBLEv2 extension should cause us to go directly to caBLE, but also
      // show the AOA option.
      {L,
       ga,
       {usb, aoa, cable},
       {v2},
       {},
       {t(aoa), t(cable), t(usb)},
       cable_ui},
#else
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
#endif

#if defined(NEW_UI)
      // If there are linked phones then AOA doesn't show up, but the phones do,
      // and sorted. The selection sheet should show.
      {L,
       mc,
       {usb, aoa, cable},
       {},
       {pqr("a"), pqr("b")},
       {p("a"), p("b"), add},
       mss},
      {L,
       ga,
       {usb, aoa, cable},
       {},
       {pqr("a"), pqr("b")},
       {p("a"), p("b"), add},
       mss},
#else
      // If there are linked phones then AOA doesn't show up, but the phones do,
      // and sorted. The selection sheet should show.
      {L,
       mc,
       {usb, aoa, cable},
       {},
       {pqr("a"), pqr("b")},
       {p("a"), p("b"), add, t(usb)},
       mss},
      {L,
       ga,
       {usb, aoa, cable},
       {},
       {pqr("a"), pqr("b")},
       {p("a"), p("b"), add, t(usb)},
       mss},
#endif

      // If this is a Conditional UI request, don't offer the platform
      // authenticator.
      {L, ga, {usb, internal}, {c_ui}, {}, {t(usb)}, usb_ui},
#if defined(NEW_UI)
      {L,
       ga,
       {usb, internal, cable},
       {c_ui},
       {pqr("a")},
       {p("a"), add},
       mss},
#else
      {L,
       ga,
       {usb, internal, cable},
       {c_ui},
       {pqr("a")},
       {p("a"), add, t(usb)},
       mss},
#endif

      // On Windows, mc with rk=required jumps to the platform UI when caBLE
      // isn't an option. The case where caBLE is possible is tested below.
      {L, mc, {}, {has_winapi, rk}, {}, {winapi}, plat_ui},
      // For rk=discouraged, always jump to Windows UI.
      {L, mc, {cable}, {has_winapi}, {}, {winapi, add}, plat_ui},
      {L, mc, {}, {has_winapi}, {}, {winapi}, plat_ui},

      // On Windows, ga with an empty allow list goes to the platform UI unless
      // caBLE is an option and resident-key is required, which is tested below.
      {L, ga, {}, {has_winapi, empty_al}, {}, {winapi}, plat_ui},
#if defined(NEW_UI)
      // With a non-empty allow list containing non phone credentials, always
      // jump to Windows UI.
      // TODO(NEWUI): we should maintain this behaviour on Windows.
      {L, ga, {cable}, {has_winapi}, {}, {add, winapi}, mss},
#else
      // With a non-empty allow list containing non phone credentials, always
      // jump to Windows UI.
      {L, ga, {cable}, {has_winapi}, {}, {winapi, add}, plat_ui},
#endif
      {L, ga, {}, {has_winapi}, {}, {winapi}, plat_ui},
      // Except when the request is legacy cable.
#if defined(NEW_UI)
      {L, ga, {cable, aoa}, {has_winapi, v1}, {}, {t(cable), winapi}, cable_ui},
      {L,
       ga,
       {cable, aoa},
       {has_winapi, v2},
       {},
       {t(aoa), t(cable), winapi},
       cable_ui},
#else
      {L, ga, {cable, aoa}, {has_winapi, v1}, {}, {winapi, t(cable)}, cable_ui},
      {L,
       ga,
       {cable, aoa},
       {has_winapi, v2},
       {},
       {winapi, t(aoa), t(cable)},
       cable_ui},
#endif

#if defined(NEW_UI)
      // QR code first: Make credential should jump to the QR code with
      // RK=true.
      {L,
       mc,
       {usb, internal, cable},
       {rk, att_xplat},
       {},
       {add, t(internal)},
       qr},
      // Unless there is a phone paired already.
      {L,
       mc,
       {usb, internal, cable},
       {rk, att_xplat},
       {pqr("a")},
       {p("a"), add, t(internal)},
       mss},
      // Or if attachment=any
      {L,
       mc,
       {usb, internal, cable},
       {rk, att_any},
       {},
       {add, t(internal)},
       mss},
      // If RK=false, go to the default for the platform instead.
      {
          L,
          mc,
          {usb, internal, cable},
          {},
          {},
          {add, t(internal)},
#if BUILDFLAG(IS_MAC)
          create_pk,
#else
          mss,
#endif
      },
#else
      // QR code first: Make credential should jump to the QR code with
      // RK=true.
      {L,
       mc,
       {usb, internal, cable},
       {rk, att_xplat},
       {},
       {add, t(internal), t(usb)},
       qr},
      // Unless there is a phone paired already.
      {L,
       mc,
       {usb, internal, cable},
       {rk, att_xplat},
       {pqr("a")},
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
#endif
      // Windows should also jump to the QR code first.
      {L, mc, {cable}, {rk, has_winapi}, {}, {winapi, add}, qr},

#if defined(NEW_UI)
      // QR code first: Get assertion should jump to the QR code with empty
      // allow-list.
      {L,
       ga,
       {usb, internal, cable},
       {empty_al},
       {},
       {add},
       qr},
      // And if the allow list only contains phones.
      {L,
       ga,
       {internal, cable},
       {only_hybrid_or_internal},
       {},
       {add},
       qr},
      // Unless there is a phone paired already.
      {L,
       ga,
       {usb, internal, cable},
       {empty_al},
       {pqr("a")},
       {p("a"), add},
       mss},
      // Even if the phone is from sync.
      {L,
       ga,
       {usb, internal, cable},
       {empty_al},
       {psync("a")},
       {p("a"), add},
       mss},
      // Or a recognized platform credential.
      {L,
       ga,
       {usb, internal, cable},
       {empty_al, has_plat, one_cred},
       {},
       {c(cred1), add},
       hero},
      // Ignore the platform credential for conditional ui requests
      {L,
       ga,
       {usb, internal, cable},
       {c_ui, empty_al, has_plat, one_cred},
       {},
       {add},
       qr},
      // If there is an allow-list containing USB, go to QR code as well.
      {L, ga, {usb, internal, cable}, {}, {}, {add}, qr},
      // Windows should also jump to the QR code first.
      // TODO: the expectation here (mss) doesn't match the comment.
      {L, ga, {cable}, {empty_al, has_winapi}, {}, {add, winapi}, mss},
      // Unless there is a recognized platform credential.
      {L,
       ga,
       {cable},
       {empty_al, has_winapi, has_plat, one_cred},
       {},
       {c(wincred1), add, winapi},
       hero},
#else
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
       {pqr("a")},
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
#endif
      // For <=Win 10, we can't tell if there is a credential or not. Show the
      // mechanism selection screen instead.
      {L,
       ga,
       {cable},
       {empty_al, has_winapi, maybe_plat},
       {},
       {winapi, add},
       mss},

#if defined(NEW_UI)
      // Phone confirmation sheet: Get assertion should jump to it if there is
      // a single phone paired.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal},
       {pqr("a")},
       {p("a"), add},
       pconf},
      // Even on Windows.
      {L,
       ga,
       {cable},
       {only_hybrid_or_internal, has_winapi},
       {pqr("a")},
       {p("a"), add},
       pconf},
      // Unless there is a recognized platform credential.
      // Or a USB credential.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal, has_plat},
       {pqr("a")},
       {p("a"), add},
       plat_ui},
      {L,
       ga,
       {usb, cable, internal},
       {},
       {pqr("a")},
       {p("a"), add},
       mss},
      // iCloud Keychain counts as a recognised platform credential too.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal, has_ickc, ickc_creds},
       {pqr("a")},
       {c(ickc_cred1), p("a"), add},
       plat_ui},
#else
      // Phone confirmation sheet: Get assertion should jump to it if there is
      // a single phone paired.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal},
       {pqr("a")},
       {p("a"), add, t(internal)},
       pconf},
      // Even on Windows.
      {L,
       ga,
       {cable},
       {only_hybrid_or_internal, has_winapi},
       {pqr("a")},
       {winapi, p("a"), add},
       pconf},
      // Unless there is a recognized platform credential.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal, has_plat},
       {pqr("a")},
       {p("a"), add, t(internal)},
       plat_ui},
      // Or a USB credential.
      {L,
       ga,
       {usb, cable, internal},
       {},
       {pqr("a")},
       {p("a"), add, t(internal), t(usb)},
       mss},
#endif
      // Or this is a conditional UI request.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal, c_ui},
       {pqr("a")},
       {p("a"), add},
       mss},
#if defined(NEW_UI)
      // Go to the mechanism selection screen if there are more phones paired.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal},
       {pqr("a"), pqr("b")},
       {p("a"), p("b"), add},
       mss},
#else
      // Go to the mechanism selection screen if there are more phones paired.
      {L,
       ga,
       {cable, internal},
       {only_hybrid_or_internal},
       {pqr("a"), pqr("b")},
       {p("a"), p("b"), add, t(internal)},
       mss},
#endif
  };

  // Tests for the new UI that lists synced passkeys mixed with local
  // credentials.
  Test kListSyncedPasskeysTests[]{
      // Mac & Linux:
      // Mix of phone and internal credentials.
      {L,
       ga,
       {usb, cable, internal},
       {one_phone_cred, two_cred},
       {psync("a")},
       {c(cred1), c(cred2), c(phonecred1), add},
       mss},
      // Internal credentials + qr code.
      {L,
       ga,
       {usb, cable, internal},
       {two_cred},
       {psync("a")},
       {c(cred1), c(cred2), add},
       mss},
      // qr code with ble disabled shows usb option.
      {L, ga, {usb, cable}, {ble_off}, {}, {add, t(usb)}, mss},
      // qr code with ble access denied shows usb option.
      {L, ga, {usb, cable}, {ble_denied}, {}, {add, t(usb)}, mss},
      // Internal credentials, no qr code.
      {L,
       ga,
       {usb, internal},
       {two_cred},
       {psync("a")},
       {c(cred1), c(cred2), t(usb)},
       mss},
      // Phone credentials only.
      {L,
       ga,
       {usb, cable, internal},
       {two_phone_cred},
       {psync("a")},
       {c(phonecred1), c(phonecred2), add},
       mss},
      // Single internal credential.
      {L,
       ga,
       {usb, cable, internal},
       {one_cred, has_plat},
       {psync("a")},
       {c(cred1), add},
#if BUILDFLAG(IS_MAC)
       plat_ui,
#else
       use_pk,
#endif
     },
      // Single phone credential with empty allow list.
      {L,
       ga,
       {usb, cable, internal},
       {one_phone_cred, empty_al},
       {psync("a")},
       {c(phonecred1), add},
       hero},

#if BUILDFLAG(IS_MAC)
      // Even with iCloud Keychain present, we shouldn't jump to it without
      // additional flags set.
      {L, mc, {internal}, {rk, has_ickc}, {}, {ickc, t(internal)}, create_pk},
      // iCloud Keychain should be the default if the request delegate
      // configured that.
      {L,
       mc,
       {internal},
       {rk, has_ickc, create_ickc},
       {},
       {ickc, t(internal)},
       plat_ui},
      // ... and only for attachment=platform
      {L,
       mc,
       {internal},
       {rk, att_any, has_ickc, create_ickc},
       {},
       {ickc, t(internal)},
       mss},
#endif
  };

  Test kListSyncedPasskeysTests_Windows_NoWinHybrid[]{
      // Mix of phone and internal credentials, but no USB/NFC.
      // This should not offer dispatching directly to the Windows API.
      {L,
       ga,
       {cable},
       {one_phone_cred, two_cred, has_winapi, only_hybrid_or_internal},
       {psync("a")},
       {c(wincred1), c(wincred2), c(phonecred1), add},
       mss},
      // Mix of phone, internal credentials, and USB/NFC.
      // This should offer dispatching to the Windows API for USB/NFC.
      {L,
       ga,
       {cable},
       {one_phone_cred, two_cred, has_winapi},
       {psync("a")},
       {c(wincred1), c(wincred2), c(phonecred1), add, winapi},
       mss},
      // Phone credentials and unknown Windows Hello credential status.
      // This should offer dispatching to the Windows API for Windows Hello.
      {L,
       ga,
       {cable},
       {two_phone_cred, has_winapi, maybe_plat, only_hybrid_or_internal},
       {psync("a")},
       {c(phonecred1), c(phonecred2), winapi, add},
       mss},
  };

  Test kListSyncedPasskeysTests_Windows_WinHybrid[]{
      // Mix of phone and internal credentials, no USB/NFC.
      // This should offer dispatching directly to the Windows API for hybrid.
      {L,
       ga,
       {cable},
       {one_phone_cred, two_cred, has_winapi, only_hybrid_or_internal},
       {psync("a")},
       {c(wincred1), c(wincred2), c(phonecred1), winapi},
       mss},
      // Internal credentials only.
      // This should dispatch directly to the Windows API.
      {L,
       ga,
       {},
       {two_cred, has_winapi, only_internal},
       {},
       {c(wincred1), c(wincred2)},
       plat_ui},
  };
  // clang-format on
#undef L

#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);
#endif

  auto RunTest = [&](const Test& test, bool windows_has_hybrid) {
    SCOPED_TRACE(static_cast<int>(test.expected_first_step));
    SCOPED_TRACE(
        (SetToString<TransportAvailabilityParam,
                     TransportAvailabilityParamToString>(test.params)));
    SCOPED_TRACE((SetToString<device::FidoTransportProtocol, device::ToString>(
        test.transports)));
    SCOPED_TRACE(RequestTypeToString(test.request_type));
    SCOPED_TRACE(testing::Message() << "At line number: " << test.line_num);

#if BUILDFLAG(IS_WIN)
    fake_win_webauthn_api.set_version(windows_has_hybrid ? 6 : 4);
    SCOPED_TRACE(windows_has_hybrid);
#endif

    TransportAvailabilityInfo transports_info;
    transports_info.is_ble_powered =
        !base::Contains(test.params, TransportAvailabilityParam::kBleDisabled);
    transports_info.ble_access_denied = base::Contains(
        test.params, TransportAvailabilityParam::kBleAccessDenied);
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
      transports_info.has_platform_authenticator_credential = device::
          FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential;
    }

    device::DiscoverableCredentialMetadata cred1;
    device::DiscoverableCredentialMetadata cred2;
    if (base::Contains(
            test.params,
            TransportAvailabilityParam::kHasWinNativeAuthenticator)) {
      cred1 = kWinCred1;
      cred2 = kWinCred2;
    } else {
      cred1 = kCred1;
      cred2 = kCred2;
    }
    if (base::Contains(test.params,
                       TransportAvailabilityParam::kHasICloudKeychainCreds)) {
      transports_info.has_icloud_keychain_credential =
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential;
      transports_info.recognized_credentials.emplace_back(
          kCred1FromICloudKeychain);
    } else {
      transports_info.has_icloud_keychain_credential =
          device::FidoRequestHandlerBase::RecognizedCredential::
              kNoRecognizedCredential;
    }

    if (base::Contains(test.params,
                       TransportAvailabilityParam::kOneRecognizedCred)) {
      transports_info.recognized_credentials = {std::move(cred1)};
    } else if (base::Contains(
                   test.params,
                   TransportAvailabilityParam::kTwoRecognizedCreds)) {
      transports_info.recognized_credentials = {std::move(cred1),
                                                std::move(cred2)};
    }
    if (base::Contains(test.params,
                       TransportAvailabilityParam::kOnePhoneRecognizedCred)) {
      transports_info.recognized_credentials.emplace_back(kPhoneCred1);
    }
    if (base::Contains(test.params,
                       TransportAvailabilityParam::kTwoPhoneRecognizedCred)) {
      transports_info.recognized_credentials.emplace_back(kPhoneCred1);
      transports_info.recognized_credentials.emplace_back(kPhoneCred2);
    }
    transports_info.has_icloud_keychain = base::Contains(
        test.params, TransportAvailabilityParam::kHasICloudKeychain);
    transports_info.has_empty_allow_list = base::Contains(
        test.params, TransportAvailabilityParam::kEmptyAllowList);
    if (base::Contains(test.params,
                       TransportAvailabilityParam::kOnlyInternal)) {
      transports_info.request_is_internal_only = true;
      transports_info.transport_list_did_include_hybrid = false;
      transports_info.transport_list_did_include_security_key = false;
    } else if (base::Contains(
                   test.params,
                   TransportAvailabilityParam::kOnlyHybridOrInternal)) {
      transports_info.is_only_hybrid_or_internal = true;
      transports_info.transport_list_did_include_hybrid = true;
      transports_info.transport_list_did_include_security_key = false;
    } else {
      transports_info.transport_list_did_include_hybrid = true;
      transports_info.transport_list_did_include_security_key = true;
    }
    transports_info.transport_list_did_include_internal = true;

    if (base::Contains(
            test.params,
            TransportAvailabilityParam::kHasWinNativeAuthenticator) ||
        windows_has_hybrid) {
      transports_info.has_win_native_api_authenticator = true;
      transports_info.win_native_ui_shows_resident_credential_notice = true;
      transports_info.win_is_uvpaa = true;
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
    if (base::Contains(test.params,
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

    AuthenticatorRequestDialogModel model(main_rfh());

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

    model.set_allow_icloud_keychain(transports_info.has_icloud_keychain);
    if (base::Contains(test.params,
                       TransportAvailabilityParam::kCreateInICloudKeychain)) {
      model.set_should_create_in_icloud_keychain(true);
    }
#if BUILDFLAG(IS_MAC)
    if (base::Contains(test.params, TransportAvailabilityParam::kNoTouchId)) {
      model.set_local_biometrics_override_for_testing(false);
    } else {
      model.set_local_biometrics_override_for_testing(true);
    }
#endif

    model.SetAccountPreselectedCallback(
        base::BindRepeating([](device::PublicKeyCredentialDescriptor cred) {}));

    if (has_v2_cable_extension.has_value() || !test.phones.empty() ||
        base::Contains(test.transports,
                       device::FidoTransportProtocol::kHybrid)) {
      std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
      for (const auto& phone : test.phones) {
        auto pairing = std::make_unique<device::cablev2::Pairing>();
        if (absl::holds_alternative<pqr>(phone)) {
          pairing->name = absl::get<pqr>(phone).value();
          pairing->from_sync_deviceinfo = false;
        } else {
          pairing->name = absl::get<psync>(phone).value();
          pairing->from_sync_deviceinfo = true;
        }
        pairing->peer_public_key_x962 = {0};
        pairing->peer_public_key_x962[0] =
            base::checked_cast<uint8_t>(phones.size());
        phones.emplace_back(std::move(pairing));
      }
      model.set_cable_transport_info(has_v2_cable_extension, std::move(phones),
                                     base::DoNothing(), absl::nullopt);
    }

    bool is_conditional_ui = base::Contains(
        test.params, TransportAvailabilityParam::kIsConditionalUI);
    model.StartFlow(std::move(transports_info), is_conditional_ui);
    if (is_conditional_ui) {
      EXPECT_EQ(model.current_step(), Step::kConditionalMediation);
      model.TransitionToModalWebAuthnRequest();
    }

    if (windows_has_hybrid &&
        !base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI)) {
      // Before the new synced passkeys UI, caBLEv1 and server-link are the only
      // cases that Windows _doesn't_ handle when it has hybrid support because
      // those are legacy protocol variants.
      if (test.expected_first_step != cable_ui) {
        EXPECT_EQ(plat_ui, model.current_step());
      }
      return;
    }

    EXPECT_EQ(test.expected_first_step, model.current_step());

    std::vector<AuthenticatorRequestDialogModel::Mechanism::Type>
        mechanism_types;
    for (const auto& mech : model.mechanisms()) {
      mechanism_types.push_back(mech.type);
    }
    EXPECT_EQ(test.expected_mechanisms, mechanism_types);

    if (!model.offer_try_again_in_ui()) {
      return;
    }

    model.StartOver();
    EXPECT_EQ(Step::kMechanismSelection, model.current_step());
  };

  for (const auto& test : kTests) {
    // On Windows, all the tests are run twice. Once to check that, when Windows
    // has hybrid support, we always jump the Windows, and then to test the
    // prior behaviour.
    for (const bool windows_has_hybrid : {
           false
#if BUILDFLAG(IS_WIN)
               ,
               true
#endif
         }) {
      RunTest(test, windows_has_hybrid);
    }
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {device::kWebAuthnNewPasskeyUI, device::kWebAuthnListSyncedPasskeys},
      /*disabled_features=*/{});
  for (const auto& test : kListSyncedPasskeysTests) {
    RunTest(test, /*windows_has_hybrid=*/false);
  }
#if BUILDFLAG(IS_WIN)
  for (const auto& test : kListSyncedPasskeysTests_Windows_NoWinHybrid) {
    RunTest(test, /*windows_has_hybrid=*/false);
  }
  for (const auto& test : kListSyncedPasskeysTests_Windows_WinHybrid) {
    RunTest(test, /*windows_has_hybrid=*/true);
  }
#endif
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

      AuthenticatorRequestDialogModel model(main_rfh());
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
  AuthenticatorRequestDialogModel model(main_rfh());
  model.StartFlow(std::move(tai), /*is_conditional_mediation=*/false);
  EXPECT_EQ(
      model.current_step(),
      AuthenticatorRequestDialogModel::Step::kErrorWindowsHelloNotEnabled);
  EXPECT_FALSE(model.offer_try_again_in_ui());
}
#endif

TEST_F(AuthenticatorRequestDialogModelTest, NoAvailableTransports) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model(main_rfh());
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
    if (transports_info.request_type == RequestType::kMakeCredential) {
      transports_info.make_credential_attachment =
          device::AuthenticatorAttachment::kAny;
    }
    transports_info.available_transports = {AuthenticatorTransport::kHybrid};
    transports_info.is_off_the_record_context =
        test.profile == Profile::INCOGNITO;

    AuthenticatorRequestDialogModel model(main_rfh());

    std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings;
    pairings.emplace_back(GetPairingFromQR());
    model.set_cable_transport_info(
        /*extension_is_v2=*/absl::nullopt, std::move(pairings),
        base::DoNothing(), absl::nullopt);

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
    AuthenticatorRequestDialogModel model(main_rfh());
    model.AddObserver(&mock_observer);

    TransportAvailabilityInfo transports_info;
    transports_info.request_type = RequestType::kMakeCredential;
    transports_info.make_credential_attachment =
        device::AuthenticatorAttachment::kAny;
    transports_info.available_transports = kAllTransportsWithoutCable;

    EXPECT_CALL(mock_observer, OnStepTransition());
    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
#if BUILDFLAG(IS_MAC)
    EXPECT_EQ(Step::kCreatePasskey, model.current_step());
#else
    EXPECT_EQ(Step::kMechanismSelection, model.current_step());
#endif
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
    AuthenticatorRequestDialogModel model(main_rfh());
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
    AuthenticatorRequestDialogModel model(main_rfh());
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
    AuthenticatorRequestDialogModel model(main_rfh());
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

#if !defined(NEW_UI)

// TODO: reenable this test. I'm not sure that the intended behaviour on
// Windows will be.

TEST_F(AuthenticatorRequestDialogModelTest,
       RequestCallbackForWindowsAuthenticatorIsInvokedAutomatically) {
  constexpr char kWinAuthenticatorId[] = "some_authenticator_id";

  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
  transports_info.request_type = RequestType::kMakeCredential;
  transports_info.make_credential_attachment =
      device::AuthenticatorAttachment::kPlatform;
  transports_info.available_transports = {};
  transports_info.has_win_native_api_authenticator = true;

  std::vector<std::string> dispatched_authenticator_ids;
  AuthenticatorRequestDialogModel model(main_rfh());
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

#endif

TEST_F(AuthenticatorRequestDialogModelTest,
       ConditionalUINoRecognizedCredential) {
  AuthenticatorRequestDialogModel model(main_rfh());

  int preselect_num_called = 0;
  model.SetAccountPreselectedCallback(base::BindRepeating(
      [](int* i, device::PublicKeyCredentialDescriptor cred) {
        EXPECT_EQ(cred.id, std::vector<uint8_t>({1, 2, 3, 4}));
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
  AuthenticatorRequestDialogModel model(main_rfh());
  int preselect_num_called = 0;
  model.SetAccountPreselectedCallback(base::BindRepeating(
      [](int* i, device::PublicKeyCredentialDescriptor cred) {
        EXPECT_EQ(cred.id, std::vector<uint8_t>({0}));
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
  transports_info.recognized_credentials = {kCred1, kCred2};
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
  AuthenticatorRequestDialogModel model(main_rfh());
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

// Tests that selecting a phone passkey on Conditional UI contacts the priority
// phone from sync.
TEST_F(AuthenticatorRequestDialogModelTest, ConditionalUIPhonePasskey) {
  constexpr char kLinkedPhoneName[] = "Phone from QR";
  constexpr char kOldSyncedPhoneName[] = "Old synced phone";
  constexpr char kNewSyncedPhoneName[] = "New synced phone";

  absl::optional<std::string> phone_name;
  // Creates a new dialog model for the given list of |phones|.
  auto MakeModel = [&](bool include_old_phone)
      -> std::unique_ptr<AuthenticatorRequestDialogModel> {
    auto model = std::make_unique<AuthenticatorRequestDialogModel>(main_rfh());
    model->SetAccountPreselectedCallback(base::DoNothing());

    // Store the contacted phone.
    base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
        callback = base::BindLambdaForTesting(
            [&](std::unique_ptr<device::cablev2::Pairing> value) {
              ASSERT_FALSE(phone_name);
              phone_name = value->name;
            });
    phone_name.reset();

    // Set up a linked phone and two phones from sync: an "old" one that last
    // contacted sync yesterday, and a "new" one that last contacted sync today.
    base::Time today = base::Time::Now();
    base::Time yesterday = today - base::Days(1);
    std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
    std::unique_ptr<device::cablev2::Pairing> qr_phone = GetPairingFromQR();
    qr_phone->name = kLinkedPhoneName;
    phones.emplace_back(std::move(qr_phone));
    if (include_old_phone) {
      std::unique_ptr<device::cablev2::Pairing> old_synced_phone =
          GetPairingFromSync();
      old_synced_phone->last_updated = yesterday;
      old_synced_phone->name = kOldSyncedPhoneName;
      phones.emplace_back(std::move(old_synced_phone));
    }
    std::unique_ptr<device::cablev2::Pairing> recently_synced_phone =
        GetPairingFromSync();
    recently_synced_phone->last_updated = today;
    recently_synced_phone->name = kNewSyncedPhoneName;
    phones.emplace_back(std::move(recently_synced_phone));
    model->set_cable_transport_info(/*extension_is_v2=*/absl::nullopt,
                                    std::move(phones), std::move(callback),
                                    absl::nullopt);

    // Set up a single credential from a phone.
    device::DiscoverableCredentialMetadata credential = kCred1;
    credential.source = device::AuthenticatorType::kPhone;
    TransportAvailabilityInfo tai;
    tai.recognized_credentials = {credential};
    tai.is_ble_powered = true;
    tai.request_type = device::FidoRequestType::kGetAssertion;
    tai.available_transports = {AuthenticatorTransport::kHybrid};
    model->StartFlow(tai, /*is_conditional_mediation=*/true);
    CHECK_EQ(model->current_step(), Step::kConditionalMediation);
    return model;
  };

  // Preselect the credential. This should select the phone that last contacted
  // sync.
  std::unique_ptr<AuthenticatorRequestDialogModel> model =
      MakeModel(/*include_old_phone=*/true);
  model->OnAccountPreselected(kCred1.cred_id);
  EXPECT_EQ(model->current_step(), Step::kCableActivate);
  EXPECT_EQ(phone_name, kNewSyncedPhoneName);

  // Manually contact the "old" phone from sync. This should give it priority as
  // the most recently used.
  model = MakeModel(/*include_old_phone=*/true);
  model->ContactPhoneForTesting(kOldSyncedPhoneName);
  ASSERT_EQ(phone_name, kOldSyncedPhoneName);

  // Preselect the credential. This should contact the priority phone, which is
  // the "old" phone now.
  model = MakeModel(/*include_old_phone=*/true);
  model->OnAccountPreselected(kCred1.cred_id);
  EXPECT_EQ(model->current_step(), Step::kCableActivate);
  EXPECT_EQ(phone_name, kOldSyncedPhoneName);

  // Remove the "old" phone so that preselecting the credential again picks the
  // "new" one.
  model = MakeModel(/*include_old_phone=*/false);
  model->OnAccountPreselected(kCred1.cred_id);
  EXPECT_EQ(model->current_step(), Step::kCableActivate);
  EXPECT_EQ(phone_name, kNewSyncedPhoneName);
}

// Tests that if the stored preference for the most recently used phone is not
// valid base64, the value is ignored.
TEST_F(AuthenticatorRequestDialogModelTest, InvalidPriorityPhonePref) {
  auto model = std::make_unique<AuthenticatorRequestDialogModel>(main_rfh());
  model->SetAccountPreselectedCallback(base::DoNothing());

  // Store the contacted phone.
  std::unique_ptr<device::cablev2::Pairing> contacted_phone;
  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
      callback = base::BindLambdaForTesting(
          [&](std::unique_ptr<device::cablev2::Pairing> value) {
            ASSERT_FALSE(contacted_phone);
            contacted_phone = std::move(value);
          });

  std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
  phones.emplace_back(GetPairingFromSync());
  model->set_cable_transport_info(/*extension_is_v2=*/absl::nullopt,
                                  std::move(phones), std::move(callback),
                                  absl::nullopt);

  // Set up a single credential from a phone.
  device::DiscoverableCredentialMetadata credential = kCred1;
  credential.source = device::AuthenticatorType::kPhone;
  TransportAvailabilityInfo tai;
  tai.recognized_credentials = {credential};
  tai.is_ble_powered = true;
  tai.request_type = device::FidoRequestType::kGetAssertion;
  tai.available_transports = {AuthenticatorTransport::kHybrid};
  model->StartFlow(tai, /*is_conditional_mediation=*/true);
  ASSERT_EQ(model->current_step(), Step::kConditionalMediation);

  // Set an invalid base64 string as the last used pairing preference.
  profile()->GetPrefs()->SetString(
      webauthn::pref_names::kLastUsedPairingFromSyncPublicKey, "oops!");
  model->OnAccountPreselected(credential.cred_id);
  EXPECT_EQ(model->current_step(), Step::kCableActivate);
  EXPECT_TRUE(contacted_phone);
}

#if BUILDFLAG(IS_WIN)
// Tests that cancelling the Windows Platform authenticator during a Conditional
// UI request restarts it.
TEST_F(AuthenticatorRequestDialogModelTest, ConditionalUIWindowsCancel) {
  testing::StrictMock<MockDialogModelObserver> mock_observer;
  AuthenticatorRequestDialogModel model(main_rfh());
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

TEST_F(AuthenticatorRequestDialogModelTest, PreSelect) {
  for (const bool has_empty_allow_list : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "has_empty_allow_list=" << has_empty_allow_list);

    AuthenticatorRequestDialogModel model(main_rfh());
    int preselect_num_called = 0;
    model.SetAccountPreselectedCallback(base::BindLambdaForTesting(
        [&preselect_num_called](device::PublicKeyCredentialDescriptor cred) {
          EXPECT_EQ(cred.id, std::vector<uint8_t>({1}));
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
        /*device_id=*/"internal-authenticator",
        AuthenticatorTransport::kInternal, device::AuthenticatorType::kOther));

    TransportAvailabilityInfo transports_info;
    transports_info.request_type = device::FidoRequestType::kGetAssertion;
    transports_info.available_transports = kAllTransports;
    transports_info.has_empty_allow_list = has_empty_allow_list;
    transports_info.user_verification_requirement =
        device::UserVerificationRequirement::kRequired;
    transports_info.has_platform_authenticator_credential = device::
        FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
    transports_info.recognized_credentials = {kCred1FromICloudKeychain, kCred2};
    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
#if defined(NEW_UI)
    if (has_empty_allow_list) {
      EXPECT_EQ(model.current_step(), Step::kSelectPriorityMechanism);
    } else {
      EXPECT_EQ(model.current_step(), Step::kNotStarted);
    }
#else
    if (has_empty_allow_list) {
      EXPECT_EQ(model.current_step(), Step::kPreSelectAccount);
    } else {
      EXPECT_EQ(model.current_step(), Step::kPreSelectSingleAccount);
    }
#endif
    task_environment()->RunUntilIdle();

    if (has_empty_allow_list) {
      EXPECT_EQ(preselect_num_called, 0);
      EXPECT_EQ(request_num_called, 0);
      // After preselecting an account, the request should be dispatched to the
      // platform authenticator.
      model.OnAccountPreselected(kCred2.cred_id);
      task_environment()->RunUntilIdle();
      EXPECT_EQ(preselect_num_called, 1);
      EXPECT_EQ(request_num_called, 1);
    } else {
      EXPECT_EQ(request_num_called, 0);
      ASSERT_EQ(model.creds().size(), 1u);
      if (base::FeatureList::IsEnabled(device::kWebAuthnNewPasskeyUI)) {
        // `kCred1FromICloudKeychain` is an iCloud Keychain credential so,
        // even though it's in `recognized_credentials`, it shouldn't have been
        // used by the standard platform authenticator code.
        EXPECT_EQ(model.creds()[0].cred_id, std::vector<uint8_t>({1}));
      } else {
        // Without the new UI flag set, the iCloud Keychain credential won't
        // be filtered out when triggering the platform authenticator.
        EXPECT_EQ(model.creds()[0].cred_id, std::vector<uint8_t>({4}));
      }
    }
  }
}

#if BUILDFLAG(IS_WIN)
// Regression test for crbug.com/1476884.
TEST_F(AuthenticatorRequestDialogModelTest, JumpToWindowsWithNewUI) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kWebAuthnNewPasskeyUI};
  AuthenticatorRequestDialogModel model(main_rfh());

  TransportAvailabilityInfo transports_info;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.available_transports = kAllTransports;
  transports_info.has_win_native_api_authenticator = true;
  transports_info.has_empty_allow_list = false;
  transports_info.has_platform_authenticator_credential = device::
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
  transports_info.recognized_credentials = {kWinCred1, kWinCred2};

  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      /*device_id=*/"win", AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kWinNative));

  RequestCallbackReceiver request_callback;
  model.SetRequestCallback(request_callback.Callback());
  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/false);
  EXPECT_EQ(request_callback.WaitForResult(), "win");
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(AuthenticatorRequestDialogModelTest, ContactPriorityPhone) {
  AuthenticatorRequestDialogModel model(main_rfh());
  std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
  phones.emplace_back(GetPairingFromQR());
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
  EXPECT_EQ(model.selected_phone_name(), "Phone from QR");
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

      AuthenticatorRequestDialogModel model(main_rfh());
      std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
      phones.emplace_back(GetPairingFromQR());
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
  AuthenticatorRequestDialogModel model(main_rfh());
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
  AuthenticatorRequestDialogModel model(main_rfh());
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

class MultiplePlatformAuthenticatorsTest
    : public AuthenticatorRequestDialogModelTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnNewPasskeyUI};
};

template <class Value>
class RepeatingValueCallbackReceiver {
 public:
  base::RepeatingCallback<void(Value)> Callback() {
    return base::BindRepeating(&RepeatingValueCallbackReceiver::OnCallback,
                               base::Unretained(this));
  }

  Value WaitForResult() {
    if (!value_) {
      run_loop_->Run();
    }
    Value ret = std::move(*value_);
    run_loop_ = std::make_unique<base::RunLoop>();
    return ret;
  }

 private:
  void OnCallback(Value value) {
    value_ = std::move(value);
    run_loop_->Quit();
  }
  absl::optional<Value> value_;
  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
};

TEST_F(MultiplePlatformAuthenticatorsTest, DeduplicateAccounts) {
  using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
  const struct {
    std::vector<device::DiscoverableCredentialMetadata> recognized_credentials;
    absl::optional<Mechanism::Type> type_of_priority_mechanism;
  } kTests[] = {
      {{kCred1, kCred2, kPhoneCred1}, absl::nullopt},
      {{kCred1, kCred2}, absl::nullopt},
      {{kCred1, kCred1FromICloudKeychain},
       Mechanism::Credential(CredentialInfoFrom(kCred1FromICloudKeychain))},
      {{kCred1FromICloudKeychain, kCred1},
       Mechanism::Credential(CredentialInfoFrom(kCred1FromICloudKeychain))},
  };

  for (const auto& test : kTests) {
    TransportAvailabilityInfo transports_info;
    transports_info.request_type = device::FidoRequestType::kGetAssertion;
    transports_info.available_transports = {AuthenticatorTransport::kInternal};
    transports_info.recognized_credentials = test.recognized_credentials;
    transports_info.has_empty_allow_list = true;

    AuthenticatorRequestDialogModel model(main_rfh());
    model.set_allow_icloud_keychain(true);
    RepeatingValueCallbackReceiver<device::PublicKeyCredentialDescriptor>
        account_preselected_callback;
    model.SetAccountPreselectedCallback(
        account_preselected_callback.Callback());
    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
    ASSERT_EQ(model.ephemeral_state_.priority_mechanism_index_.has_value(),
              test.type_of_priority_mechanism.has_value());
    if (!test.type_of_priority_mechanism.has_value()) {
      continue;
    }

    EXPECT_EQ(
        *test.type_of_priority_mechanism,
        model.mechanisms_[*model.ephemeral_state_.priority_mechanism_index_]
            .type);
  }
}

#if BUILDFLAG(IS_MAC)

TEST_F(MultiplePlatformAuthenticatorsTest, Dispatch) {
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnICloudKeychain};

  for (const bool should_create_in_icloud_keychain : {false, true}) {
    for (const bool platform_attachment : {false, true}) {
      if (!platform_attachment && should_create_in_icloud_keychain) {
        // Without `platform_attachment`, `should_create_in_icloud_keychain` is
        // moot.
        continue;
      }

      SCOPED_TRACE(testing::Message() << "should_create_in_icloud_keychain: "
                                      << should_create_in_icloud_keychain);
      SCOPED_TRACE(testing::Message()
                   << "platform_attachment: " << platform_attachment);

      TransportAvailabilityInfo transports_info;
      transports_info.has_icloud_keychain = true;
      transports_info.available_transports = {
          AuthenticatorTransport::kInternal,
          AuthenticatorTransport::kUsbHumanInterfaceDevice};
      transports_info.request_type = device::FidoRequestType::kMakeCredential;
      transports_info.resident_key_requirement =
          device::ResidentKeyRequirement::kRequired;
      transports_info.make_credential_attachment =
          platform_attachment ? device::AuthenticatorAttachment::kPlatform
                              : device::AuthenticatorAttachment::kAny;

      AuthenticatorRequestDialogModel model(main_rfh());
      model.set_allow_icloud_keychain(true);
      model.set_should_create_in_icloud_keychain(
          should_create_in_icloud_keychain);

      RequestCallbackReceiver request_callback;
      model.SetRequestCallback(request_callback.Callback());

      const std::string kProfileAuthenticatorId = "platauth";
      model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
          kProfileAuthenticatorId, AuthenticatorTransport::kInternal,
          device::AuthenticatorType::kTouchID));
      const std::string kICloudKeychainId = "ickc";
      model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
          kICloudKeychainId, AuthenticatorTransport::kInternal,
          device::AuthenticatorType::kICloudKeychain));

      model.StartFlow(std::move(transports_info),
                      /*is_conditional_mediation=*/false);
      if (platform_attachment) {
        if (should_create_in_icloud_keychain) {
          EXPECT_EQ(request_callback.WaitForResult(), kICloudKeychainId);
        } else {
          EXPECT_EQ(model.current_step(),
                    AuthenticatorRequestDialogModel::Step::kCreatePasskey);
          model.HideDialogAndDispatchToPlatformAuthenticator();
          EXPECT_EQ(request_callback.WaitForResult(), kProfileAuthenticatorId);
        }
      } else {
        EXPECT_EQ(model.current_step(),
                  AuthenticatorRequestDialogModel::Step::kMechanismSelection);
      }

      if (!platform_attachment) {
        // Dispatch to iCloud Keychain to check that canceling doesn't show
        // a Chrome error dialog.
        model.HideDialogAndDispatchToPlatformAuthenticator(
            device::AuthenticatorType::kICloudKeychain);
      }

      model.OnUserConsentDenied();

      if (platform_attachment) {
        EXPECT_EQ(
            model.current_step(),
            should_create_in_icloud_keychain
                ? AuthenticatorRequestDialogModel::Step::kMechanismSelection
                : AuthenticatorRequestDialogModel::Step::
                      kErrorInternalUnrecognized);

        model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
            kProfileAuthenticatorId, AuthenticatorTransport::kInternal,
            device::AuthenticatorType::kTouchID));
        model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
            kICloudKeychainId, AuthenticatorTransport::kInternal,
            device::AuthenticatorType::kICloudKeychain));

        // Dispatch and cancel again to confirm that canceling the non-automatic
        // dispatch cancels the whole request.
        model.HideDialogAndDispatchToPlatformAuthenticator(
            device::AuthenticatorType::kICloudKeychain);
        model.OnUserConsentDenied();
      }

      // Canceling after a non-automatic dispatch to iCloud Keychain should
      // end the request.
      EXPECT_EQ(model.current_step(),
                AuthenticatorRequestDialogModel::Step::kNotStarted);
    }
  }
}

TEST_F(MultiplePlatformAuthenticatorsTest,
       OnlyShowConfirmationSheetForProfileAuthenticator) {
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnICloudKeychain};

  for (const auto credential_source :
       {device::AuthenticatorType::kTouchID,
        device::AuthenticatorType::kICloudKeychain}) {
    SCOPED_TRACE(static_cast<int>(credential_source));

    TransportAvailabilityInfo transports_info;
    transports_info.has_icloud_keychain = true;
    transports_info.available_transports = {AuthenticatorTransport::kInternal};
    transports_info.request_type = device::FidoRequestType::kGetAssertion;
    transports_info.has_empty_allow_list = false;

    if (credential_source == device::AuthenticatorType::kTouchID) {
      transports_info.recognized_credentials = {kCred2};
      transports_info.has_platform_authenticator_credential =
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential;
    } else {
      transports_info.recognized_credentials = {kCred1FromICloudKeychain};
      transports_info.has_icloud_keychain_credential =
          device::FidoRequestHandlerBase::RecognizedCredential::
              kHasRecognizedCredential;
    }

    AuthenticatorRequestDialogModel model(main_rfh());
    model.set_allow_icloud_keychain(true);
    RepeatingValueCallbackReceiver<device::PublicKeyCredentialDescriptor>
        account_preselected_callback;
    model.SetAccountPreselectedCallback(
        account_preselected_callback.Callback());

    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);

    if (credential_source == device::AuthenticatorType::kTouchID) {
      EXPECT_EQ(model.current_step(), Step::kSelectPriorityMechanism);
    } else {
      EXPECT_EQ(model.current_step(), Step::kNotStarted);
      device::PublicKeyCredentialDescriptor descriptor =
          account_preselected_callback.WaitForResult();
      EXPECT_EQ(descriptor.id, kCred1FromICloudKeychain.cred_id);
    }
  }
}

#endif

class ListPasskeysFromSyncTest : public AuthenticatorRequestDialogModelTest {
 public:
  ListPasskeysFromSyncTest() {
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthnNewPasskeyUI, device::kWebAuthnListSyncedPasskeys},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ListPasskeysFromSyncTest, ListGPMPasskeysInConditionalUI) {
  NavigateAndCommit(GURL("rp.com"));

  // Tests that passkeys are listed in conditional UI, but only if there is a
  // phone from sync available.
  ChromeWebAuthnCredentialsDelegate* delegate =
      ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents())
          ->GetDelegateForFrame(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(delegate);

  TransportAvailabilityInfo transports_info;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.recognized_credentials = {kPhoneCred1};
  {
    AuthenticatorRequestDialogModel model(main_rfh());
    model.StartFlow(transports_info,
                    /*is_conditional_mediation=*/true);

    // There is no phone available, so no passkeys should be sent to autofill.
    EXPECT_TRUE(delegate->GetPasskeys()->empty());
  }
  {
    AuthenticatorRequestDialogModel model(main_rfh());
    std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
    phones.emplace_back(GetPairingFromQR());
    model.set_cable_transport_info(
        /*extension_is_v2=*/absl::nullopt, std::move(phones), base::DoNothing(),
        absl::nullopt);
    model.StartFlow(transports_info,
                    /*is_conditional_mediation=*/true);

    // There is no phone from sync, so no passkeys should be sent to autofill.
    EXPECT_TRUE(delegate->GetPasskeys()->empty());
  }
  {
    AuthenticatorRequestDialogModel model(main_rfh());
    std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
    phones.emplace_back(GetPairingFromSync());
    model.set_cable_transport_info(
        /*extension_is_v2=*/absl::nullopt, std::move(phones), base::DoNothing(),
        absl::nullopt);
    model.StartFlow(transports_info,
                    /*is_conditional_mediation=*/true);

    ASSERT_EQ(delegate->GetPasskeys()->size(), 1u);
    const password_manager::PasskeyCredential& passkey =
        delegate->GetPasskeys()->at(0);
    EXPECT_EQ(passkey.credential_id(), kPhoneCred1.cred_id);
    EXPECT_EQ(passkey.display_name(), "");
    EXPECT_EQ(passkey.username(), kPhoneCred1.user.name);
    EXPECT_EQ(passkey.GetAuthenticatorLabel(),
              l10n_util::GetStringFUTF16(
                  IDS_PASSWORD_MANAGER_PASSKEY_FROM_PHONE, u"Phone from sync"));
    EXPECT_EQ(passkey.user_id(), kPhoneCred1.user.id);
    EXPECT_EQ(passkey.rp_id(), kPhoneCred1.rp_id);
    EXPECT_EQ(passkey.source(),
              password_manager::PasskeyCredential::Source::kAndroidPhone);
  }
}

TEST_F(ListPasskeysFromSyncTest, MechanismsFromUserAccounts) {
  // Set up a model with two local passkeys and a GPM passkey.
  AuthenticatorRequestDialogModel model(main_rfh());
  TransportAvailabilityInfo transports_info;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.available_transports = {AuthenticatorTransport::kInternal};
  transports_info.recognized_credentials = {kCred1, kCred2, kPhoneCred1};
  transports_info.ble_access_denied = false;
  transports_info.is_ble_powered = true;

  std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
  phones.emplace_back(GetPairingFromSync());
  RepeatingValueCallbackReceiver<std::unique_ptr<device::cablev2::Pairing>>
      contact_phone_callback;
  model.set_cable_transport_info(
      /*extension_is_v2=*/absl::nullopt, std::move(phones),
      contact_phone_callback.Callback(), absl::nullopt);
  RepeatingValueCallbackReceiver<device::PublicKeyCredentialDescriptor>
      account_preselected_callback;
  model.SetAccountPreselectedCallback(account_preselected_callback.Callback());

  RequestCallbackReceiver request_callback;
  model.SetRequestCallback(request_callback.Callback());
  const std::string kLocalAuthenticatorId = "local-authenticator";
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      kLocalAuthenticatorId, AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  model.StartFlow(std::move(transports_info),
                  /*is_conditional_mediation=*/false);

  // Entries will be sorted by username. So the first entry should correspond to
  // the first local passkey.
  const AuthenticatorRequestDialogModel::Mechanism& mech1 =
      model.mechanisms()[0];
  EXPECT_EQ(mech1.name, base::UTF8ToUTF16(*kUser1.name));
  EXPECT_EQ(mech1.short_name, base::UTF8ToUTF16(*kUser1.name));
  EXPECT_EQ(mech1.description,
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_GENERIC_DEVICE));
  EXPECT_EQ(mech1.icon, vector_icons::kPasskeyIcon);
  mech1.callback.Run();
  device::PublicKeyCredentialDescriptor result =
      account_preselected_callback.WaitForResult();
  EXPECT_EQ(result.id, kCred1.cred_id);
  EXPECT_THAT(result.transports,
              testing::ElementsAre(device::FidoTransportProtocol::kInternal));
  EXPECT_EQ(request_callback.WaitForResult(), kLocalAuthenticatorId);

  // Reset the model as if the user had cancelled out of the operation.
  model.StartOver();
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      kLocalAuthenticatorId, AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  // The second entry will be `kCred2`.
  const AuthenticatorRequestDialogModel::Mechanism& mech2 =
      model.mechanisms()[1];
  EXPECT_EQ(mech2.name, base::UTF8ToUTF16(*kUser2.name));
  EXPECT_EQ(mech2.short_name, base::UTF8ToUTF16(*kUser2.name));
  EXPECT_EQ(mech2.description, u"Use device sign-in");
  EXPECT_EQ(mech2.icon, vector_icons::kPasskeyIcon);
  mech2.callback.Run();
  result = account_preselected_callback.WaitForResult();
  EXPECT_EQ(result.id, kCred2.cred_id);
  EXPECT_THAT(result.transports,
              testing::ElementsAre(device::FidoTransportProtocol::kInternal));
  EXPECT_EQ(request_callback.WaitForResult(), kLocalAuthenticatorId);

  // Reset the model as if the user had cancelled out of the operation.
  model.StartOver();
  model.saved_authenticators().AddAuthenticator(AuthenticatorReference(
      kLocalAuthenticatorId, AuthenticatorTransport::kInternal,
      device::AuthenticatorType::kOther));

  // The third entry should correspond to `kPhoneCred1`.
  const AuthenticatorRequestDialogModel::Mechanism& mech3 =
      model.mechanisms()[2];
  EXPECT_EQ(mech3.name, base::UTF8ToUTF16(*kPhoneUser1.name));
  EXPECT_EQ(mech3.short_name, base::UTF8ToUTF16(*kPhoneUser1.name));
  EXPECT_EQ(mech3.description,
            l10n_util::GetStringFUTF16(IDS_WEBAUTHN_SOURCE_PHONE,
                                       u"Phone from sync"));
  EXPECT_EQ(mech3.icon, kSmartphoneIcon);
  mech3.callback.Run();
  result = account_preselected_callback.WaitForResult();
  EXPECT_EQ(result.id, kPhoneCred1.cred_id);
  EXPECT_THAT(result.transports,
              testing::ElementsAre(device::FidoTransportProtocol::kHybrid));
  EXPECT_TRUE(contact_phone_callback.WaitForResult());
}

#if BUILDFLAG(IS_WIN)

using HasCreds = device::FidoRequestHandlerBase::RecognizedCredential;
constexpr int kNoWinButton = -1;
constexpr int kHelloOrSk = IDS_WEBAUTHN_TRANSPORT_WINDOWS_HELLO_OR_SECURITY_KEY;
constexpr int kHello = IDS_WEBAUTHN_TRANSPORT_WINDOWS_HELLO;
constexpr int kSk = IDS_WEBAUTHN_TRANSPORT_EXTERNAL_SECURITY_KEY;
constexpr int kPhoneOrSk =
    IDS_WEBAUTHN_PASSKEY_PHONE_TABLET_OR_SECURITY_KEY_LABEL;
constexpr int kPhone = IDS_WEBAUTHN_PASSKEY_PHONE_OR_TABLET_LABEL;
#define L __LINE__
struct {
  int line_num;
  bool has_sk;
  bool has_hybrid;
  bool has_internal;
  bool supports_hybrid;
  HasCreds has_creds;
  int expected_button;
} kWinHelloButtonGetAssertionTestCases[] = {
    // Windows v7+ with all transports.
    {L, true, true, true, true, HasCreds::kHasRecognizedCredential, kPhoneOrSk},

    // Windows v7+ with only security keys.
    {L, true, false, false, true, HasCreds::kNoRecognizedCredential, kSk},

    // Windows v7+ with only phones.
    {L, false, true, false, true, HasCreds::kNoRecognizedCredential, kPhone},

    // Windows v7+ with only internal creds.
    {L, false, false, true, true, HasCreds::kHasRecognizedCredential,
     kNoWinButton},

    // Windows v7+ with empty allow-list.
    {L, false, false, false, true, HasCreds::kHasRecognizedCredential,
     kPhoneOrSk},

    // Windows v5+ with all transports.
    {L, true, true, true, false, HasCreds::kHasRecognizedCredential, kSk},

    // Windows v5+ with only security keys
    {L, true, false, false, false, HasCreds::kNoRecognizedCredential, kSk},

    // Windows v5+ with only phones.
    {L, false, true, false, false, HasCreds::kNoRecognizedCredential,
     kNoWinButton},

    // Windows v5+ with only internal creds.
    {L, false, false, true, false, HasCreds::kHasRecognizedCredential,
     kNoWinButton},

    // Windows v5+ with empty allow-list.
    {L, false, false, false, false, HasCreds::kHasRecognizedCredential, kSk},

    // Windows <v4 with all transports.
    {L, true, true, true, false, HasCreds::kUnknown, kHelloOrSk},

    // Windows <v4 with only security keys.
    {L, true, false, false, false, HasCreds::kUnknown, kSk},

    // Windows <v4 with only phones.
    {L, false, true, false, false, HasCreds::kUnknown, kNoWinButton},

    // Windows <v4 with only internal creds.
    {L, false, false, true, false, HasCreds::kUnknown, kHello},

    // Windows <v4 with empty allow-list.
    {L, false, false, false, false, HasCreds::kUnknown, kHelloOrSk},
};
#undef L

TEST_F(ListPasskeysFromSyncTest, WindowsHelloButtonLabel_GetAssertion) {
  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);
  for (const auto& test_case : kWinHelloButtonGetAssertionTestCases) {
    AuthenticatorRequestDialogModel model(main_rfh());
    TransportAvailabilityInfo transports_info;
    transports_info.has_win_native_api_authenticator = true;
    transports_info.request_type = device::FidoRequestType::kGetAssertion;
    transports_info.transport_list_did_include_security_key = test_case.has_sk;
    transports_info.transport_list_did_include_hybrid = test_case.has_hybrid;
    transports_info.transport_list_did_include_internal =
        test_case.has_internal;
    transports_info.has_platform_authenticator_credential = test_case.has_creds;
    if (test_case.has_creds == HasCreds::kHasRecognizedCredential) {
      transports_info.recognized_credentials = {kCred1};
    }
    if (!test_case.has_sk && !test_case.has_hybrid && !test_case.has_internal) {
      transports_info.has_empty_allow_list = true;
    }
    fake_win_webauthn_api.set_version(test_case.supports_hybrid ? 7 : 4);
    SCOPED_TRACE(testing::Message() << "Line number: " << test_case.line_num);
    SCOPED_TRACE(testing::Message() << "SK: " << test_case.has_sk);
    SCOPED_TRACE(testing::Message() << "Hybrid: " << test_case.has_hybrid);
    SCOPED_TRACE(testing::Message() << "Internal: " << test_case.has_internal);
    SCOPED_TRACE(testing::Message()
                 << "Has creds: " << static_cast<int>(test_case.has_creds));
    SCOPED_TRACE(testing::Message()
                 << "Handles hybrid: " << test_case.supports_hybrid);

    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
    auto win_button_it =
        base::ranges::find_if(model.mechanisms(), [](const auto& m) {
          return absl::holds_alternative<
              AuthenticatorRequestDialogModel::Mechanism::WindowsAPI>(m.type);
        });
    if (test_case.expected_button == kNoWinButton) {
      EXPECT_EQ(win_button_it, model.mechanisms().end());
    } else {
      ASSERT_NE(win_button_it, model.mechanisms().end());
      EXPECT_EQ(win_button_it->name,
                l10n_util::GetStringUTF16(test_case.expected_button));
      EXPECT_EQ(win_button_it->short_name,
                l10n_util::GetStringUTF16(test_case.expected_button));
      switch (test_case.expected_button) {
        case kHelloOrSk:
        case kHello:
          EXPECT_EQ(win_button_it->icon, kLaptopIcon);
          break;
        case kSk:
          EXPECT_EQ(win_button_it->icon, vector_icons::kUsbIcon);
          break;
        case kPhoneOrSk:
        case kPhone:
          EXPECT_EQ(win_button_it->icon, kSmartphoneIcon);
          break;
        default:
          NOTREACHED();
      }
    }
  }
}

struct {
  device::AuthenticatorAttachment attachment;
  int expected_button;
} kWinHelloButtonMakeCredentialTestCases[] = {
    // For make credential, we will only show the authenticator picker when
    // Windows does not do hybrid. Therefore, there is no option for "Hello,
    // Security Key, or Phone".
    {device::AuthenticatorAttachment::kAny, kHelloOrSk},
    {device::AuthenticatorAttachment::kCrossPlatform, kSk},
    {device::AuthenticatorAttachment::kPlatform, kHello},
};

TEST_F(ListPasskeysFromSyncTest, WindowsHelloButtonLabel_MakeCredential) {
  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);
  for (const auto& test_case : kWinHelloButtonMakeCredentialTestCases) {
    AuthenticatorRequestDialogModel model(main_rfh());
    TransportAvailabilityInfo transports_info;
    transports_info.has_win_native_api_authenticator = true;
    transports_info.request_type = device::FidoRequestType::kMakeCredential;
    transports_info.make_credential_attachment = test_case.attachment;
    fake_win_webauthn_api.set_version(4);
    SCOPED_TRACE(testing::Message()
                 << "Attachment: " << static_cast<int>(test_case.attachment));

    model.StartFlow(std::move(transports_info),
                    /*is_conditional_mediation=*/false);
    auto win_button_it =
        base::ranges::find_if(model.mechanisms(), [](const auto& m) {
          return absl::holds_alternative<
              AuthenticatorRequestDialogModel::Mechanism::WindowsAPI>(m.type);
        });
    ASSERT_NE(win_button_it, model.mechanisms().end());
    EXPECT_EQ(win_button_it->name,
              l10n_util::GetStringUTF16(test_case.expected_button));
    EXPECT_EQ(win_button_it->short_name,
              l10n_util::GetStringUTF16(test_case.expected_button));
    switch (test_case.expected_button) {
      case kHelloOrSk:
      case kHello:
        EXPECT_EQ(win_button_it->icon, kLaptopIcon);
        break;
      case kSk:
        EXPECT_EQ(win_button_it->icon, vector_icons::kUsbIcon);
        break;
      default:
        NOTREACHED();
    }
  }
}

#endif  // BUILDFLAG(IS_WIN)
