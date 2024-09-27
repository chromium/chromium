// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/fake_magic_arch.h"
#include "chrome/browser/webauthn/fake_recovery_key_store.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/mock_trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/features.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/util.h"
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/test/test_future.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"
#include "chrome/common/chrome_version.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/enclave/icloud_recovery_key_mac.h"
#include "device/fido/mac/fake_icloud_keychain.h"
#include "device/fido/mac/util.h"
#endif  // BUILDFLAG(IS_MAC)

// These tests are disabled under MSAN. The enclave subprocess is written in
// Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace {

using trusted_vault::MockTrustedVaultConnection;

constexpr int32_t kSecretVersion = 417;
constexpr uint8_t kSecurityDomainSecret[32] = {0};
constexpr char kEmail[] = "test@gmail.com";
constexpr char kEmailLocalPartOnly[] = "test";
// This value is derived by the Sync testing code from `kEmail` but is needed
// directly in these tests in order to simulate the `StoreKeys` calls to the
// `EnclaveManager`.
constexpr char kGaiaId[] = "gaia_id_for_test_gmail.com";

// Protobuf generated by printing one generated by an enclave using
// `kSecurityDomainSecret`.
constexpr uint8_t kTestProtobuf[] = {
    0x0A, 0x10, 0x8E, 0x48, 0x4B, 0x1C, 0x4F, 0xF9, 0x01, 0x14, 0xEF, 0xEA,
    0xB3, 0x18, 0x40, 0x21, 0xEB, 0xF9, 0x12, 0x10, 0x48, 0x74, 0x02, 0x2C,
    0xC5, 0x85, 0x38, 0xDA, 0x22, 0xD8, 0x8C, 0xAF, 0xD4, 0x05, 0x29, 0x84,
    0x1A, 0x0F, 0x77, 0x77, 0x77, 0x2E, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C,
    0x65, 0x2E, 0x63, 0x6F, 0x6D, 0x22, 0x01, 0x00, 0x30, 0xE4, 0xFA, 0x86,
    0x8D, 0xAC, 0x86, 0xDD, 0x17, 0x3A, 0x03, 0x66, 0x6F, 0x6F, 0x42, 0x00,
    0x62, 0xCB, 0x01, 0x30, 0x89, 0x28, 0x56, 0xC4, 0x9C, 0xC4, 0xAD, 0x19,
    0x4D, 0x4B, 0x91, 0x12, 0xD4, 0xA0, 0x05, 0xF0, 0xA4, 0xCA, 0x87, 0x66,
    0x4C, 0x9E, 0x49, 0x58, 0xED, 0x08, 0x92, 0xB9, 0x5C, 0x5C, 0xCD, 0x7D,
    0xA7, 0xD4, 0xEA, 0x54, 0xE9, 0x7E, 0xF2, 0x93, 0xDA, 0x17, 0x43, 0x7F,
    0x41, 0x15, 0x25, 0x94, 0xB8, 0x04, 0x08, 0xAD, 0xE7, 0x67, 0xFA, 0xE2,
    0x38, 0xD3, 0x37, 0xCE, 0x68, 0x1C, 0x2C, 0x82, 0xCA, 0xED, 0x8D, 0x10,
    0x32, 0x31, 0xD9, 0xED, 0x7F, 0x51, 0x74, 0x66, 0x63, 0x14, 0x12, 0xD3,
    0xA1, 0xC0, 0xFE, 0x52, 0xA3, 0x07, 0x01, 0x58, 0xDD, 0x3F, 0xD4, 0x97,
    0xD8, 0xFA, 0x7F, 0x9A, 0xB2, 0xC1, 0x65, 0x36, 0xE2, 0xBE, 0xDF, 0x00,
    0xFB, 0xAC, 0x59, 0xFE, 0x93, 0x25, 0x18, 0xA3, 0x92, 0xBF, 0x06, 0x8E,
    0x0F, 0x2E, 0xD6, 0xE8, 0xFE, 0xCD, 0xE5, 0x76, 0xB8, 0x92, 0x3D, 0xB1,
    0x42, 0xE9, 0xBB, 0x54, 0x36, 0x99, 0x5C, 0x21, 0xB7, 0x63, 0x33, 0x20,
    0x8E, 0x93, 0xAA, 0x00, 0x83, 0xC6, 0xCC, 0x23, 0xAD, 0x63, 0x2B, 0x34,
    0xAA, 0x4F, 0x8E, 0x9B, 0xFA, 0x40, 0x0E, 0xDB, 0x30, 0x37, 0x58, 0xE4,
    0x60, 0xA2, 0xDF, 0x99, 0x85, 0x4B, 0x5C, 0xDD, 0x44, 0x23, 0x12, 0x64,
    0x4C, 0x50, 0x34, 0x9D, 0x24, 0x1B, 0x37, 0x40, 0xC5, 0xB5, 0xA1, 0x5A,
    0x70, 0x33, 0xF7, 0x80, 0x75, 0x1D, 0x22, 0x13, 0x37, 0xCD, 0x1F, 0x24,
    0x40, 0xDA, 0x70, 0xA1, 0x03};

base::span<const uint8_t, 16> TestProtobufCredId() {
  return base::span<const uint8_t>(kTestProtobuf).subspan<20, 16>();
}

static constexpr char kIsUVPAA[] = R"((() => {
  window.PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable().
    then(result => window.domAutomationController.send('IsUVPAA: ' + result),
         error  => window.domAutomationController.send('error '    + error));
})())";

static constexpr char kMakeCredentialUvDiscouraged[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: 'discouraged',
      requireResidentKey: true,
    },
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialSecurePaymentConfirmation[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: 'required',
      residentKey: 'preferred',
      authenticatorAttachment: 'platform',
    },
    extensions: {payment: {isPayment: true}},
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialReturnId[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: 'discouraged',
      requireResidentKey: true,
    },
  }}).then(c => window.domAutomationController.send(
                  'webauthn: ' + c.id),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialWithExcludedCredential[] = R"((() => {
  const base64ToArrayBuffer = (base64) => {
    const bytes = window.atob(base64);
    const len = bytes.length;
    const ret = new Uint8Array(len);
    for (var i = 0; i < len; i++) {
        ret[i] = bytes.charCodeAt(i);
    }
    return ret.buffer;
  }
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: 'required',
      requireResidentKey: true,
      authenticatorAttachment: "platform",
    },
    excludeCredentials: [{type: "public-key",
                          transports: [],
                          id: base64ToArrayBuffer("$1")}],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialCrossPlatform[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: 'discouraged',
      requireResidentKey: true,
      authenticatorAttachment: "cross-platform",
    },
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialAttachmentPlatform[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      authenticatorAttachment: 'platform',
      userVerification: 'discouraged',
      requireResidentKey: true,
    },
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialUvRequired[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      requireResidentKey: true,
      userVerification: 'required',
    },
  }}).then(c => window.domAutomationController.send(
              'webauthn: uv=' +
              // This gets the UV bit from the response.
              ((new Uint8Array(c.response.getAuthenticatorData())[32]&4) != 0)),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialWithPrf[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      requireResidentKey: true,
      userVerification: 'discouraged',
    },
    extensions: {
      prf: {$1},
    },
  }}).then(c => {
    const showValue = (results, key) => {
        if (results === undefined) {
            return "none";
        }
        return btoa(String.fromCharCode.apply(
                        null, new Uint8Array(results[key])));
    };
    const results = c.getClientExtensionResults().prf.results;
    window.domAutomationController.send(
              'prf ' +
              c.getClientExtensionResults().prf.enabled +
              ' ' +
              showValue(results, 'first') +
              ' ' +
              showValue(results, 'second'));
    },
    e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialGoogle[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { id: "google.com", name: "google.com" },
    user: { id: new Uint8Array([0]), name: "$1", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: 'discouraged',
      requireResidentKey: true,
    },
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionWithPrf[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [],
    extensions: {
      prf: {$1},
    },
  }}).then(c => {
    const showValue = (results, key) => {
        if (results === undefined) {
            return "none";
        }
        return btoa(String.fromCharCode.apply(
            null, new Uint8Array(results[key])));
    };
    const results = c.getClientExtensionResults().prf.results;
    window.domAutomationController.send(
              'prf ' +
              c.getClientExtensionResults().prf.enabled +
              ' ' +
              showValue(results, 'first') +
              ' ' +
              showValue(results, 'second'));
    },
    e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionUvDiscouraged[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kAbortableGetAssertion[] = R"((() => {
  window.enclaveAbortSignal = new AbortController();
  navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [],
  },
  signal: window.enclaveAbortSignal.signal,
  });
})())";

static constexpr char kAbort[] = R"((() => {
  window.enclaveAbortSignal.abort();
})())";

static constexpr char kGetAssertionUvDiscouragedWithCredId[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [{
      'type': 'public-key',
      'transports': ['internal', 'hybrid', 'usb'],
      'id': Uint8Array.from(atob("$1"), c => c.charCodeAt(0)).buffer}],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char
    kGetAssertionUvDiscouragedWithCredIdAndInternalTransport[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [{
      'type': 'public-key',
      'transports': ['internal'],
      'id': Uint8Array.from(atob("$1"), c => c.charCodeAt(0)).buffer}],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionUvRequired[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'required',
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionUvPreferred[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'preferred',
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send(
              'webauthn: uv=' +
              // This gets the UV bit from the response.
              ((new Uint8Array(c.response.authenticatorData)[32]&4) != 0)),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionConditionalUI[] = R"((() => {
  return navigator.credentials.get({
    mediation: "conditional",
    publicKey: {
        challenge: new Uint8Array([0]),
        timeout: 10000,
        userVerification: 'discouraged',
        allowCredentials: [],
    }}).then(c => window.domAutomationController.send('webauthn: OK'),
             e => window.domAutomationController.send('error ' + e));
})())";

bool IsReady(GPMEnclaveController::AccountState state) {
  switch (state) {
    case GPMEnclaveController::AccountState::kReady:
      return true;
    default:
      LOG(ERROR) << "State " << static_cast<int>(state)
                 << " is not a ready state";
      return false;
  }
}

bool IsMechanismEnclaveCredential(
    const AuthenticatorRequestDialogModel::Mechanism& mechanism) {
  if (absl::holds_alternative<
          AuthenticatorRequestDialogModel::Mechanism::Credential>(
          mechanism.type)) {
    return absl::get<AuthenticatorRequestDialogModel::Mechanism::Credential>(
               mechanism.type)
               ->source == device::AuthenticatorType::kEnclave;
  }
  return false;
}

struct TempDir {
 public:
  TempDir() { CHECK(dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

class EnclaveAuthenticatorBrowserTest : public SyncTest {
 public:
  class DelegateObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    explicit DelegateObserver(EnclaveAuthenticatorBrowserTest* test_instance)
        : test_instance_(test_instance) {
      run_loop_ = std::make_unique<base::RunLoop>();
      destruction_run_loop_ = std::make_unique<base::RunLoop>();
    }
    virtual ~DelegateObserver() = default;

    void WaitForUI() {
      run_loop_->Run();
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    void WaitForDelegateDestruction() {
      destruction_run_loop_->Run();
      destruction_run_loop_ = std::make_unique<base::RunLoop>();
    }

    void AddAdditionalTransport(device::FidoTransportProtocol transport) {
      additional_transport_ = transport;
    }

    void SetPendingTrustedVaultConnection(
        std::unique_ptr<trusted_vault::TrustedVaultConnection> connection) {
      pending_connection_ = std::move(connection);
    }

    void SetUseSyncedDeviceCablePairing(bool use_pairing) {
      use_synced_device_cable_pairing_ = use_pairing;
    }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(delegate);
      if (pending_connection_) {
        delegate->SetTrustedVaultConnectionForTesting(
            std::move(pending_connection_));
      }
      delegate->SetClockForTesting(&test_instance_->clock_);
    }

    void OnDestroy(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(nullptr);
      destruction_run_loop_->QuitWhenIdle();
    }

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      std::vector<std::unique_ptr<device::cablev2::Pairing>> ret;
      if (use_synced_device_cable_pairing_) {
        ret.emplace_back(TestPhone("phone", /*public_key=*/0,
                                   /*last_updated=*/base::Time::FromTimeT(1),
                                   /*channel_priority=*/1));
      }
      return ret;
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {
      if (additional_transport_.has_value()) {
        tai->available_transports.insert(*additional_transport_);
      }
    }

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      run_loop_->QuitWhenIdle();
    }

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) override {}

    void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>& responses)
        override {}

   private:
    raw_ptr<EnclaveAuthenticatorBrowserTest> test_instance_;
    std::optional<device::FidoTransportProtocol> additional_transport_;
    std::unique_ptr<trusted_vault::TrustedVaultConnection> pending_connection_;
    bool use_synced_device_cable_pairing_ = false;
    std::unique_ptr<base::RunLoop> run_loop_;
    std::unique_ptr<base::RunLoop> destruction_run_loop_;
  };

  class ModelObserver : public AuthenticatorRequestDialogModel::Observer {
   public:
    explicit ModelObserver(AuthenticatorRequestDialogModel* model)
        : model_(model) {
      model_->observers.AddObserver(this);
    }

    ~ModelObserver() override {
      if (model_) {
        model_->observers.RemoveObserver(this);
        model_ = nullptr;
      }
    }

    void WaitForLoadingEnclaveTimeout() {
      run_loop_ = std::make_unique<base::RunLoop>();
      waiting_for_loading_enclave_timeout_ = true;
      run_loop_->Run();
      Reset();
    }

    // Call this before the state transition you are looking to observe.
    void SetStepToObserve(AuthenticatorRequestDialogModel::Step step) {
      ASSERT_FALSE(run_loop_);
      step_ = step;
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    // Call this to observer the next step change, whatever it might be.
    void ObserveNextStep() {
      ASSERT_FALSE(run_loop_);
      observe_next_step_ = true;
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    // This will return after a transition to the state previously specified by
    // `SetStepToObserver`. Returns immediately if the current step matches.
    void WaitForStep() {
      if (!observe_next_step_ && model_->step() == step_) {
        run_loop_.reset();
        return;
      }
      ASSERT_TRUE(run_loop_);
      run_loop_->Run();
      // When waiting for `kClosed` the model is deleted at this point.
      if (!observe_next_step_ &&
          step_ != AuthenticatorRequestDialogModel::Step::kClosed) {
        CHECK_EQ(step_, model_->step());
      }
      Reset();
    }

    // AuthenticatorRequestDialogModel::Observer:
    void OnStepTransition() override {
      all_steps_.push_back(model_->step());

      if (run_loop_ && (observe_next_step_ || step_ == model_->step())) {
        run_loop_->QuitWhenIdle();
      }
    }

    void OnLoadingEnclaveTimeout() override {
      if (run_loop_ && waiting_for_loading_enclave_timeout_) {
        run_loop_->QuitWhenIdle();
      }
    }

    void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
      model_ = nullptr;
    }

    void Reset() {
      step_ = AuthenticatorRequestDialogModel::Step::kNotStarted;
      observe_next_step_ = false;
      run_loop_.reset();
    }

    base::span<const AuthenticatorRequestDialogModel::Step> all_steps() const {
      return all_steps_;
    }

   private:
    raw_ptr<AuthenticatorRequestDialogModel> model_;
    AuthenticatorRequestDialogModel::Step step_ =
        AuthenticatorRequestDialogModel::Step::kNotStarted;
    std::vector<AuthenticatorRequestDialogModel::Step> all_steps_;
    bool waiting_for_loading_enclave_timeout_ = false;
    bool observe_next_step_ = false;
    std::unique_ptr<base::RunLoop> run_loop_;
  };

  EnclaveAuthenticatorBrowserTest()
      : SyncTest(SINGLE_CLIENT),
        process_and_port_(StartWebAuthnEnclave(temp_dir_.GetPath())),
        enclave_override_(
            TestWebAuthnEnclaveIdentity(process_and_port_.second)),
        security_domain_service_(
            FakeSecurityDomainService::New(kSecretVersion)),
#if BUILDFLAG(IS_WIN)
        webauthn_dll_override_(&fake_webauthn_dll_),
#endif
        recovery_key_store_(FakeRecoveryKeyStore::New()),
        mock_hw_provider_(
            std::make_unique<crypto::ScopedMockUnexportableKeyProvider>()) {
#if BUILDFLAG(IS_WIN)
    // Make webauthn.dll unavailable to ensure a consistent test environment on
    // Windows. Otherwise the version of webauthn.dll can differ between
    // builders causing differences / failures.
    fake_webauthn_dll_.set_available(false);
    biometrics_override_ =
        std::make_unique<device::fido::win::ScopedBiometricsOverride>(false);
#elif BUILDFLAG(IS_MAC)
    // By default, Touch ID is disabled in these tests. Specific tests can
    // replace this if they need.
    biometrics_override_ =
        std::make_unique<device::fido::mac::ScopedBiometricsOverride>(false);
    if (__builtin_available(macOS 13.5, *)) {
      fake_icloud_keychain_ = device::fido::icloud_keychain::NewFake();
    }
    scoped_icloud_drive_override_ = OverrideICloudDriveEnabled(false);
#endif
    clock_.SetNow(base::Time::FromTimeT(1000));
    OSCryptMocker::SetUp();
    // Log call `FIDO_LOG` messages.
    scoped_vmodule_.InitWithSwitches("device_event_log_impl=2");

    auto security_domain_service_callback =
        security_domain_service_->GetCallback();
    auto recovery_key_store_callback = recovery_key_store_->GetCallback();
    url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [sds_callback = std::move(security_domain_service_callback),
         rks_callback = std::move(recovery_key_store_callback),
         this](const network::ResourceRequest& request) {
          std::optional<std::pair<net::HttpStatusCode, std::string>> response =
              sds_callback.Run(request);
          if (!response) {
            response = rks_callback.Run(request);
          }
          if (response) {
            url_loader_factory_.AddResponse(request.url.spec(),
                                            std::move(response->second),
                                            response->first);
          }
        }));

    fake_uv_provider_.emplace<crypto::ScopedNullUserVerifyingKeyProvider>();

    // Disabling Bluetooth significantly speeds up tests on Linux.
    bluetooth_values_for_testing_ =
        device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
    bluetooth_values_for_testing_->SetLESupported(false);
  }

  ~EnclaveAuthenticatorBrowserTest() override {
    EnclaveManagerFactory::SetUrlLoaderFactoryForTesting(nullptr);
    CHECK(process_and_port_.first.Terminate(/*exit_code=*/1, /*wait=*/true));
    OSCryptMocker::TearDown();
  }

  EnclaveAuthenticatorBrowserTest(const EnclaveAuthenticatorBrowserTest&) =
      delete;
  EnclaveAuthenticatorBrowserTest& operator=(
      const EnclaveAuthenticatorBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    EnclaveManagerFactory::SetUrlLoaderFactoryForTesting(
        url_loader_factory_.GetSafeWeakWrapper().get());

    SyncTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  IdentityTestEnvironmentProfileAdaptor::
                      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                          context);
                }));
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

    syncer::SyncServiceImpl* sync_service =
        SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
            browser()->profile());
    sync_service->OverrideNetworkForTest(
        fake_server::CreateFakeServerHttpPostProviderFactory(
            GetFakeServer()->AsWeakPtr()));
    sync_harness_ = SyncServiceImplHarness::Create(
        browser()->profile(), kEmail, "password",
        SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    ASSERT_TRUE(sync_harness_->SetupSync());
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kPasswords});

    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_.StartAcceptingConnections();
    host_resolver()->AddRule("*", "127.0.0.1");

    delegate_observer_ = std::make_unique<DelegateObserver>(this);
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        delegate_observer_.get());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  }

  void TearDownOnMainThread() override { identity_test_env_adaptor_.reset(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  void UpdateRequestDelegate(ChromeAuthenticatorRequestDelegate* delegate) {
    request_delegate_ = delegate;
    if (request_delegate_) {
      model_observer_ = std::make_unique<ModelObserver>(dialog_model());
    }
  }

  ChromeAuthenticatorRequestDelegate* request_delegate() {
    return request_delegate_;
  }

  DelegateObserver* delegate_observer() { return delegate_observer_.get(); }

  AuthenticatorRequestDialogModel* dialog_model() {
    return request_delegate()->dialog_model();
  }

  ModelObserver* model_observer() { return model_observer_.get(); }

  webauthn::PasskeyModel* passkey_model() {
    return PasskeyModelFactory::GetInstance()->GetForProfile(
        browser()->profile());
  }

  void SimulateEnclaveMechanismSelection() {
    ASSERT_TRUE(request_delegate_);
    for (const auto& mechanism :
         request_delegate_->dialog_model()->mechanisms) {
      if (mechanism.type ==
          AuthenticatorRequestDialogModel::Mechanism::Type(
              AuthenticatorRequestDialogModel::Mechanism::Enclave())) {
        mechanism.callback.Run();
        return;
      }
    }
    EXPECT_TRUE(false) << "No Enclave mechanism found";
  }

  void AddTestPasskeyToModel() {
    sync_pb::WebauthnCredentialSpecifics passkey;
    CHECK(passkey.ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
    passkey_model()->AddNewPasskeyForTesting(passkey);
  }

  void SetMockVaultConnectionOnRequestDelegate(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result) {
    std::unique_ptr<testing::NiceMock<MockTrustedVaultConnection>> connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                                 testing::_, testing::_))
        .WillOnce(
            [result = std::move(result)](
                const CoreAccountInfo&,
                base::OnceCallback<void(
                    trusted_vault::
                        DownloadAuthenticationFactorsRegistrationStateResult)>
                    callback) mutable {
              std::move(callback).Run(std::move(result));
              return std::make_unique<
                  trusted_vault::TrustedVaultConnection::Request>();
            });
    // If the delegate hasn't been created yet, the mock will be assigned upon
    // creation.
    if (request_delegate_) {
      request_delegate_->SetTrustedVaultConnectionForTesting(
          std::move(connection));
    } else {
      delegate_observer_->SetPendingTrustedVaultConnection(
          std::move(connection));
    }
  }

  void SetVaultConnectionToTimeout() {
    std::unique_ptr<testing::NiceMock<MockTrustedVaultConnection>> connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                                 testing::_, testing::_))
        .WillOnce(
            [](const CoreAccountInfo&,
               base::OnceCallback<void(
                   trusted_vault::
                       DownloadAuthenticationFactorsRegistrationStateResult)>
                   callback) mutable {
              return std::make_unique<
                  trusted_vault::TrustedVaultConnection::Request>();
            });
    // If the delegate hasn't been created yet, the mock will be assigned upon
    // creation.
    if (request_delegate_) {
      request_delegate_->SetTrustedVaultConnectionForTesting(
          std::move(connection));
    } else {
      delegate_observer_->SetPendingTrustedVaultConnection(
          std::move(connection));
    }
  }

  void CheckRegistrationStateNotRequested() {
    std::unique_ptr<testing::NiceMock<MockTrustedVaultConnection>> connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                                 testing::_, testing::_))
        .WillRepeatedly(
            [](const CoreAccountInfo&,
               base::OnceCallback<void(
                   trusted_vault::
                       DownloadAuthenticationFactorsRegistrationStateResult)>
                   callback)
                -> std::unique_ptr<
                    trusted_vault::TrustedVaultConnection::Request> {
              CHECK(false) << "account state unexpectedly requested";
              return nullptr;
            });
    CHECK(!request_delegate_);
    delegate_observer_->SetPendingTrustedVaultConnection(std::move(connection));
  }

  void EnableUVKeySupport() {
    fake_uv_provider_.emplace<crypto::ScopedFakeUserVerifyingKeyProvider>();
  }

  bool IsUVPAA() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kIsUVPAA);

    std::string script_result;
    CHECK(message_queue.WaitForMessage(&script_result));
    if (script_result == "\"IsUVPAA: true\"") {
      return true;
    } else if (script_result == "\"IsUVPAA: false\"") {
      return false;
    }
    NOTREACHED() << "unexpected IsUVPAA result: " << script_result;
  }

  void SetBiometricsEnabled(bool enabled) {
#if BUILDFLAG(IS_MAC)
    biometrics_override_.reset();
    biometrics_override_ =
        std::make_unique<device::fido::mac::ScopedBiometricsOverride>(enabled);
#elif BUILDFLAG(IS_WIN)
    biometrics_override_.reset();
    biometrics_override_ =
        std::make_unique<device::fido::win::ScopedBiometricsOverride>(enabled);
#endif
  }

 protected:
  base::SimpleTestClock clock_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  const TempDir temp_dir_;
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<SyncServiceImplHarness> sync_harness_;
  const std::pair<base::Process, uint16_t> process_and_port_;
  const device::enclave::ScopedEnclaveOverride enclave_override_;
  std::unique_ptr<FakeSecurityDomainService> security_domain_service_;
#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_webauthn_dll_;
  device::WinWebAuthnApi::ScopedOverride webauthn_dll_override_;
  std::unique_ptr<device::fido::win::ScopedBiometricsOverride>
      biometrics_override_;
#elif BUILDFLAG(IS_MAC)
  std::unique_ptr<device::fido::mac::ScopedBiometricsOverride>
      biometrics_override_;
  std::unique_ptr<device::fido::icloud_keychain::Fake> fake_icloud_keychain_;
  std::unique_ptr<ScopedICloudDriveOverride> scoped_icloud_drive_override_;
#endif
  std::unique_ptr<FakeRecoveryKeyStore> recovery_key_store_;
  std::unique_ptr<crypto::ScopedMockUnexportableKeyProvider> mock_hw_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<DelegateObserver> delegate_observer_;
  std::unique_ptr<ModelObserver> model_observer_;
  raw_ptr<ChromeAuthenticatorRequestDelegate> request_delegate_;
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_values_for_testing_;
  absl::variant<crypto::ScopedNullUserVerifyingKeyProvider,
                crypto::ScopedFakeUserVerifyingKeyProvider,
                crypto::ScopedFailingUserVerifyingKeyProvider>
      fake_uv_provider_;
  logging::ScopedVmoduleSwitches scoped_vmodule_;
};

class EnclaveAuthenticatorWithPinBrowserTest
    : public EnclaveAuthenticatorBrowserTest {
 public:
  EnclaveAuthenticatorWithPinBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
             device::kWebAuthnEnclaveAuthenticator,
             {{device::kWebAuthnGpmPin.name, "true"}},
         },
         {device::kWebAuthnRecoverFromICloudRecoveryKey, {}},
         {device::kWebAuthnICloudRecoveryKey, {}}},
        /*disabled_features=*/{
            device::kWebAuthnUseInsecureSoftwareUnexportableKeys});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Parses the string resulting from the Javascript snippets that exercise the
// PRF extension.
std::tuple<bool, std::string, std::string> ParsePrfResult(
    std::string_view result_view) {
  // Javascript strings end up with "" around them. Trim that.
  if (!result_view.empty() && result_view[0] == '"') {
    result_view.remove_prefix(1);
  }
  if (!result_view.empty() && result_view.back() == '"') {
    result_view.remove_suffix(1);
  }

  // The string is now a series of space-deliminated tokens:
  //   * "prf"
  //   * the `enabled` value: "true" / "false" / "undefined"
  //   * the base64-encoded `first` result, or "none".
  //   * the base64-encoded `second` result, or "none".
  const std::string result(result_view);
  auto tokenizer = base::StringTokenizer(result, " ");
  CHECK(tokenizer.GetNext());
  CHECK_EQ(tokenizer.token(), "prf");

  CHECK(tokenizer.GetNext());
  const std::string& enabled_str = tokenizer.token();
  CHECK(enabled_str == "true" || enabled_str == "false" ||
        enabled_str == "undefined")
      << enabled_str;
  const bool enabled = enabled_str == "true";

  CHECK(tokenizer.GetNext());
  const std::string first = tokenizer.token();
  CHECK(tokenizer.GetNext());
  const std::string second = tokenizer.token();

  return std::make_tuple(enabled, std::move(first), std::move(second));
}

// Parses the output of `kMakeCredentialReturnId` and returns the credential ID
// that was created.
std::optional<std::vector<uint8_t>> ParseCredentialId(
    std::string_view result_view) {
  // Javascript strings end up with "" around them. Trim that.
  if (!result_view.empty() && result_view[0] == '"') {
    result_view.remove_prefix(1);
  }
  if (!result_view.empty() && result_view.back() == '"') {
    result_view.remove_suffix(1);
  }

  // The string is now "webauthn: " followed by the base64url credential ID,
  // which is returned from this function.
  if (!base::StartsWith(result_view, "webauthn: ")) {
    return std::nullopt;
  }
  std::string_view base64url_cred_id = result_view.substr(10);
  return base::Base64UrlDecode(base64url_cred_id,
                               base::Base64UrlDecodePolicy::IGNORE_PADDING);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       RegisterDeviceWithGpmPin_MakeCredential_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       No existing security domain secrets
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP
   * 2. Mechanism selection appears; test chooses enclave credential
   * 3. UI for creating passkey appears; test chooses create
   * 4. UI for creating GPM Pin appears; test selects pin
   * 5. Device registration with enclave succeeds
   * 6. MakeCredential succeeds
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       NonWebauthnRequest) {
  if (!base::FeatureList::IsEnabled(features::kSecurePaymentConfirmation)) {
    // SPC is not enabled in this configuration and so the `payment` extension
    // in the Javascript will be ignored.
    return;
  }

  CheckRegistrationStateNotRequested();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents,
                              kMakeCredentialSecurePaymentConfirmation);
  delegate_observer()->WaitForUI();

  // Non-WebAuthn requests (e.g. Secure Payment Confirmation and credit-card
  // confirmation) must not use the enclave. In some cases, they will disable
  // the UI, which is not simulated here.
  EXPECT_TRUE(
      dialog_model()->step() ==
          AuthenticatorRequestDialogModel::Step::kCreatePasskey ||
      dialog_model()->step() ==
          AuthenticatorRequestDialogModel::Step::kErrorNoAvailableTransports);
}

#if BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       RegisterICloudDriveEnabled_NoGPMDefault) {
  if (__builtin_available(macOS 13.5, *)) {
    // Override iCloud Drive to appear enabled. Because of this GPM should not
    // be the default since none of the other conditions apply.
    scoped_icloud_drive_override_.reset();
    scoped_icloud_drive_override_ = OverrideICloudDriveEnabled(true);

    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        registration_state_result;
    registration_state_result.state = trusted_vault::
        DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
    SetMockVaultConnectionOnRequestDelegate(
        std::move(registration_state_result));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
    delegate_observer()->WaitForUI();

    EXPECT_EQ(dialog_model()->step(),
              AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  }
}

IN_PROC_BROWSER_TEST_F(
    EnclaveAuthenticatorWithPinBrowserTest,
    RegisterICloudDriveEnabledButAlsoPasskeyPresent_GPMDefault) {
  if (__builtin_available(macOS 13.5, *)) {
    // Override iCloud Drive to appear enabled, but also add a passkey to the
    // store. That should be sufficient for GPM to be the default.
    scoped_icloud_drive_override_.reset();
    scoped_icloud_drive_override_ = OverrideICloudDriveEnabled(true);
    AddTestPasskeyToModel();

    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        registration_state_result;
    registration_state_result.state = trusted_vault::
        DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
    SetMockVaultConnectionOnRequestDelegate(
        std::move(registration_state_result));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
    delegate_observer()->WaitForUI();

    EXPECT_EQ(dialog_model()->step(),
              AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  }
}

IN_PROC_BROWSER_TEST_F(
    EnclaveAuthenticatorWithPinBrowserTest,
    RegisterICloudDriveEnabledButPermissionDenied_GPMDefault) {
  // Override iCloud Drive to appear enabled, but override the iCloud Keychain
  // permission to appear as if the user denied Chrome permission. That should
  // cause GPM to be the default.
  if (__builtin_available(macOS 13.5, *)) {
    scoped_icloud_drive_override_.reset();
    scoped_icloud_drive_override_ = OverrideICloudDriveEnabled(true);
    fake_icloud_keychain_.reset();
    fake_icloud_keychain_ =
        device::fido::icloud_keychain::NewFakeWithPermission(false);

    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        registration_state_result;
    registration_state_result.state = trusted_vault::
        DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
    SetMockVaultConnectionOnRequestDelegate(
        std::move(registration_state_result));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
    delegate_observer()->WaitForUI();

    EXPECT_EQ(dialog_model()->step(),
              AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  }
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       MacOs13_4_OrLess_GPMDefault) {
  if (__builtin_available(macOS 13.5, *)) {
    // __builtin_available cannot be negated thus an `else` block has to be
    // used.
  } else {
    // For versions of macOS < 13.5, it doesn't matter if iCloud Drive is
    // enabled, there's no iCloud Keychain support and so GPM should always be
    // the default.
    scoped_icloud_drive_override_.reset();
    scoped_icloud_drive_override_ = OverrideICloudDriveEnabled(true);

    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        registration_state_result;
    registration_state_result.state = trusted_vault::
        DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
    SetMockVaultConnectionOnRequestDelegate(
        std::move(registration_state_result));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
    delegate_observer()->WaitForUI();

    EXPECT_EQ(dialog_model()->step(),
              AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  }
}

#endif

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       MakeCredentialWithPrf) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       No existing security domain secrets
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential with PRF request invoked by RP
   * 2. Mechanism selection appears; test chooses enclave credential
   * 3. UI for creating passkey appears; test chooses create
   * 4. UI for creating GPM pin appears; test selects pin
   * 5. Device registration with enclave succeeds
   * 6. MakeCredential succeeds and evaluates PRF.
   * 7. Second MakeCredential is made, just enabling PRF.
   * 8. Mechanism selection appears; test chooses enclave credential
   * 9. User confirms creation.
   * 10. MakeCredential succeeds and PRF reports enabled.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  const std::string kEval =
      "eval: { first: new Uint8Array([1]), second: new Uint8Array([2]) }";
  content::ExecuteScriptAsync(
      web_contents, base::ReplaceStringPlaceholders(
                        kMakeCredentialWithPrf, {kEval}, /*offsets=*/nullptr));
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));

  bool enabled;
  std::string first, second;
  std::tie(enabled, first, second) = ParsePrfResult(script_result);
  EXPECT_TRUE(enabled);
  // Since the HMAC key is randomly generated the two values are random. But we
  // can assert that they are distinct.
  EXPECT_NE(first, "none");
  EXPECT_NE(second, "none");
  EXPECT_NE(first, second);

  const std::string kEnable = "enable: true";
  content::ExecuteScriptAsync(
      web_contents,
      base::ReplaceStringPlaceholders(kMakeCredentialWithPrf, {kEnable},
                                      /*offsets=*/nullptr));

  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  dialog_model()->OnGPMCreatePasskey();

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));

  std::tie(enabled, first, second) = ParsePrfResult(script_result);
  EXPECT_TRUE(enabled);
  EXPECT_EQ(first, "none");
  EXPECT_EQ(second, "none");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       GetAssertionWithPrf) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       Trusted vault state is recoverable
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   *       Synced passkey for the RP available
   * 1. Modal GetAssertion request invoked by RP, includes PRF request.
   * 2. Priority mechanism selection for synced passkey appears, test confirms
   * 3. Window to recover security domain appears, test simulates reauth
   * 4. Test selects a GPM PIN
   * 5. Device registration with enclave succeeds
   * 6. GetAssertion succeeds with PRF results.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  const std::string kEval =
      "eval: { first: new Uint8Array([1]), second: new Uint8Array([2]) }";
  content::ExecuteScriptAsync(
      web_contents, base::ReplaceStringPlaceholders(
                        kGetAssertionWithPrf, {kEval}, /*offsets=*/nullptr));
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  bool enabled;
  std::string first, second;
  std::tie(enabled, first, second) = ParsePrfResult(script_result);
  EXPECT_FALSE(enabled);
  EXPECT_EQ(first, "wxrrL9DHkZivKyIp/cg2mRfnB2v4J+M8EevFaBqxpRc=");
  EXPECT_EQ(second, "zx7riv8qxdelsyWdRRSZSrzFji35j4fZFnr30gKf8r8=");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       RegisterDeviceWithGpmPin_MakeCredentialWithUV_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       No existing security domain secrets
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP, requires UV.
   * 2. Mechanism selection appears; test chooses enclave.
   * 3. UI for creating passkey appears; test chooses create
   * 4. UI for creating GPM pin appears; test selects pin
   * 5. Device registration with enclave succeeds
   * 6. MakeCredential succeeds
   *
   * Notably, user verification is asserted without a second GPM PIN prompt.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       MakeCredential_RecoverWithGPMPIN_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       Security domain exists with GPM PIN.
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP, requires UV.
   * 2. Mechanism selection appears; test chooses enclave.
   * 3. UI for onboarding appears; test accepts it
   * 4. Test simiulates MagicArch
   * 5. Test selects a GPM PIN
   * 6. Device registration with enclave succeeds
   * 7. MakeCredential succeeds
   *
   * Notably, user verification is asserted without a second GPM PIN prompt.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  registration_state_result.gpm_pin_metadata = trusted_vault::GpmPinMetadata(
      "public key",
      EnclaveManager::MakeWrappedPINForTesting(kSecurityDomainSecret, "123456"),
      /*expiry=*/base::Time::Now() + base::Seconds(10000));
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  security_domain_service_->pretend_there_are_members();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       MakeCredential_RecoverWithLSKF_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       Security domain exists with GPM PIN.
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP, requires UV.
   * 2. Mechanism selection appears; test chooses enclave.
   * 3. UI for onboarding appears; test accepts it
   * 4. Test simiulates MagicArch
   * 5. Test selects a GPM PIN
   * 6. Device registration with enclave succeeds
   * 7. MakeCredential succeeds
   *
   * Notably, user verification is asserted without a second GPM PIN prompt.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  security_domain_service_->pretend_there_are_members();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       CreatingDuplicateGivesInvalidStateError) {
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  security_domain_service_->pretend_there_are_members();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialReturnId);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  std::optional<std::vector<uint8_t>> cred_id =
      ParseCredentialId(script_result);
  ASSERT_TRUE(cred_id);

  content::ExecuteScriptAsync(
      web_contents,
      base::ReplaceStringPlaceholders(kMakeCredentialWithExcludedCredential,
                                      {base::Base64Encode(*cred_id)},
                                      /*offsets=*/nullptr));
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnGPMCreatePasskey();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("InvalidStateError"));
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       RecoverWithLSKF_GetAssertion_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       Trusted vault state is recoverable
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   *       Synced passkey for the RP available
   * 1. Modal GetAssertion request invoked by RP
   * 2. Priority mechanism selection for synced passkey appears, test confirms
   * 3. Window to recover security domain appears, test simulates reauth
   * 4. Test selects a GPM PIN
   * 5. Device registration with enclave succeeds
   * 6. GetAssertion succeeds
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       RegisterDeviceWithGpmPin_UVRequestsWithWrongPIN) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       No existing security domain secrets
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1.  Modal MakeCredential request invoked by RP, requires UV.
   * 2.  Mechanism selection appears; test chooses enclave.
   * 3.  UI for creating passkey appears; test chooses create
   * 4.  UI for creating GPM pin appears; test selects pin
   * 5.  Device registration with enclave succeeds
   * 6.  MakeCredential succeeds
   * 7.  Another modal MakeCredential request is invoked by RP, requiring UV
   * 8.  Test enters wrong PIN
   * 9.  PIN entry dialog appears again, test enters correct PIN
   * 10. Modal GetAssertion request invoked by RP, requires UV
   * 11. Test enters wrong PIN
   * 12. PIN entry dialog appears again, test enters correct PIN
   *
   * Notably, user verification is asserted without a second GPM PIN prompt.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  // A second MakeCredential, with incorrect PIN and then correct PIN.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnGPMCreatePasskey();
  model_observer()->WaitForStep();

  EXPECT_EQ(browser()->profile()->GetPrefs()->GetInteger(
                webauthn::pref_names::kEnclaveFailedPINAttemptsCount),
            0);
  model_observer()->ObserveNextStep();
  dialog_model()->OnGPMPinEntered(u"111111");
  model_observer()->WaitForStep();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  EXPECT_EQ(browser()->profile()->GetPrefs()->GetInteger(
                webauthn::pref_names::kEnclaveFailedPINAttemptsCount),
            1);
  dialog_model()->OnGPMPinEntered(u"123456");

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  // GetAssertion, with incorrect and then correct PIN.
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  EXPECT_EQ(browser()->profile()->GetPrefs()->GetInteger(
                webauthn::pref_names::kEnclaveFailedPINAttemptsCount),
            0);
  model_observer()->ObserveNextStep();
  dialog_model()->OnGPMPinEntered(u"111111");
  model_observer()->WaitForStep();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  EXPECT_EQ(browser()->profile()->GetPrefs()->GetInteger(
                webauthn::pref_names::kEnclaveFailedPINAttemptsCount),
            1);
  dialog_model()->OnGPMPinEntered(u"123456");

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  EXPECT_EQ(browser()->profile()->GetPrefs()->GetInteger(
                webauthn::pref_names::kEnclaveFailedPINAttemptsCount),
            0);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       GpmPinRegistrationPersistAcrossRestart) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       No existing security domain secrets
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP
   * 2. Mechanism selection appears; test chooses enclave credential
   * 3. UI for creating passkey appears; test chooses create
   * 4. UI for creating GPM pin appears; test selects pin
   * 5. Device registration with enclave succeeds
   * 6. MakeCredential succeeds
   * 7. Test clears the EnclaveManager state to force load from disk
   * 8. Modal MakeCredential request invoked by RP
   * 9. Mechanism selection appears; test chooses enclave credential
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  delegate_observer()->WaitForDelegateDestruction();

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->ResetForTesting();

  EXPECT_EQ(
      EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
          ->is_loaded(),
      false);

  // Checks that a following request goes straight to ready state.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();
  EXPECT_TRUE(IsReady(request_delegate()
                          ->enclave_controller_for_testing()
                          ->account_state_for_testing()));
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest, UserCancelsUV) {
  EnableUVKeySupport();

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  // Do a get() to ensure that any deferred UV key creation has happened.

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  // Do a get() where the signing fails, simulating the user canceling the
  // request. There should not be any Chrome error UI.

  fake_uv_provider_.emplace<crypto::ScopedFailingUserVerifyingKeyProvider>();
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->ClearCachedKeysForTesting();

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kClosed);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("\"error NotAllowedError"));
}

// Tests that if the enclave is still loading when the user taps a passkey from
// autofill, Chrome does not jump to the modal loading UI as autofill can
// display that instead. Regression test for crbug.com/343480031.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       ConditionalMediationLoading) {
  // Set up a trusted vault connection that lets us control the time it
  // resolves.
  base::OnceCallback<void(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult)>
      connection_cb;
  std::unique_ptr<testing::NiceMock<MockTrustedVaultConnection>> connection =
      std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
  EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                               testing::_, testing::_))
      .WillOnce(
          [&connection_cb](
              const CoreAccountInfo&,
              base::OnceCallback<void(
                  trusted_vault::
                      DownloadAuthenticationFactorsRegistrationStateResult)>
                  callback) mutable {
            connection_cb = std::move(callback);
            return std::make_unique<
                trusted_vault::TrustedVaultConnection::Request>();
          });
  delegate_observer_->SetPendingTrustedVaultConnection(std::move(connection));

  // Execute a conditional UI request.
  AddTestPasskeyToModel();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionConditionalUI);
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kConditionalMediation);

  dialog_model()->OnAccountPreselectedIndex(0);

  // The modal UI should not be shown yet.
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kConditionalMediation);

  // Resolve the connection and wait for the next step.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  std::move(connection_cb).Run(std::move(registration_state_result));
  model_observer()->WaitForStep();
}

// Tests tapping a passkey from autofill after the trusted vault service times
// out. Regression test for crbug.com/343669719.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       SelectPasskeyAfterTimeout) {
  SetVaultConnectionToTimeout();

  // Execute a conditional UI request.
  AddTestPasskeyToModel();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionConditionalUI);
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kConditionalMediation);

  // Wait for the request to time out.
  clock_.Advance(GPMEnclaveController::kDownloadAccountStateTimeout);
  model_observer()->WaitForLoadingEnclaveTimeout();

  // Tap the passkey and expect an error.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMError);
  dialog_model()->OnAccountPreselectedIndex(0);
  model_observer()->WaitForStep();
}

// Tests a trusted vault service timeout after tapping a passkey from autofill.
// Regression test for crbug.com/343669719.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       SelectPasskeyThenTimeout) {
  SetVaultConnectionToTimeout();

  // Execute a conditional UI request.
  AddTestPasskeyToModel();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionConditionalUI);
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kConditionalMediation);

  // Tap the passkey. The step should still be conditional mediation while
  // autofill shows a loading indicator.
  dialog_model()->OnAccountPreselectedIndex(0);
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kConditionalMediation);

  // Wait for the request to time out.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMError);
  clock_.Advance(GPMEnclaveController::kDownloadAccountStateTimeout);
  model_observer()->WaitForStep();
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       GpmEnclaveNeedsReauth) {
  // Set the account state to a recoverable signin error.
  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  CoreAccountId account =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Add a passkey to make sure it's not shown.
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  // The dialog should always jump to the mechanism selection for signin errors.
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  ASSERT_EQ(browser()->tab_strip_model()->GetTabCount(), 1);

  // No credentials should be displayed since tapping on them won't work.
  EXPECT_FALSE(
      base::ranges::any_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Credential>(m.type);
      }));

  // The button has text indicating the user they need to sign in.
  const auto sign_in_again_mech =
      base::ranges::find_if(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::SignInAgain>(m.type);
      });
  ASSERT_NE(sign_in_again_mech, dialog_model()->mechanisms.end());
  EXPECT_EQ(sign_in_again_mech->name,
            l10n_util::GetStringUTF16(IDS_WEBAUTHN_SIGN_IN_AGAIN_TITLE));
  std::move(sign_in_again_mech->callback).Run();

  // Tapping the button should cancel the request and open a new tab for reauth.
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_TRUE(script_result.starts_with("\"error NotAllowedError"))
      << script_result;
  EXPECT_EQ(browser()->tab_strip_model()->GetTabCount(), 2);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       UserResetsSecurityDomain) {
  EnableUVKeySupport();

  // Setup the EnclaveManager with a security domain secret.
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  // Now simulate a security domain reset by increasing the epoch. Chrome must
  // not create a credential encrypted to the previous security domain secret.
  // Instead it should notice the reset and try to set up the security domain
  // again.

  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  registration_state_result.key_version = kSecretVersion + 1;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       SecurityDomainCheckTimesOut) {
  EnableUVKeySupport();

  // Setup the EnclaveManager with a security domain secret.
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  // Now set the security domain check to timeout. Chrome should operate
  // normally.

  SetVaultConnectionToTimeout();

  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       BiometricsInPWA) {
  // When requesting biometrics in a PWA, Touch ID should never be used.

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();
  EnableUVKeySupport();
  SetBiometricsEnabled(true);

  // Create a Browser of type `TYPE_APP`, like a PWA.
  Browser* app_browser = Browser::Create(Browser::CreateParams::CreateForApp(
      "appname", /*trusted_source=*/true, gfx::Rect(0, 0, 500, 500),
      browser()->profile(),
      /*user_gesture=*/true));
  ASSERT_EQ(app_browser->type(), Browser::Type::TYPE_APP);
  app_browser->window()->Show();

  ASSERT_TRUE(NavigateToURLWithDisposition(
      app_browser, https_server_.GetURL("www.example.com", "/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Trigger a get() call to initialize the enclave. UV will be satisfied by
  // entering the PIN.

  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  model_observer()->WaitForStep();

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  // Do a get() call that uses biometrics. Check that Touch ID wasn't used.

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  EXPECT_FALSE(
      base::Contains(model_observer()->all_steps(),
                     AuthenticatorRequestDialogModel::Step::kGPMTouchID));
}
#endif

class EnclaveAuthenticatorWithoutPinBrowserTest
    : public EnclaveAuthenticatorBrowserTest {
 public:
  EnclaveAuthenticatorWithoutPinBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{device::kWebAuthnEnclaveAuthenticator,
          {{device::kWebAuthnGpmPin.name, "false"}}}},
        /*disabled_features=*/{
            device::kWebAuthnUseInsecureSoftwareUnexportableKeys});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Without a Windows-on-ARM device we've been unable to debug why these
// tests fail in that that context.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NotAvailableWithoutUV DISABLED_NotAvailableWithoutUV
#else
#define MAYBE_NotAvailableWithoutUV NotAvailableWithoutUV
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       MAYBE_NotAvailableWithoutUV) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_FALSE(
      request_delegate()->enclave_controller_for_testing()->is_active());
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NotAvailableForEmptyAccounts DISABLED_NotAvailableForEmptyAccounts
#else
#define MAYBE_NotAvailableForEmptyAccounts NotAvailableForEmptyAccounts
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       MAYBE_NotAvailableForEmptyAccounts) {
  EnableUVKeySupport();
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_FALSE(
      request_delegate()->enclave_controller_for_testing()->is_active());
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NoGpmCredentialsIfDeviceCannotBeEnrolled \
  DISABLED_NoGpmCredentialsIfDeviceCannotBeEnrolled
#else
#define MAYBE_NoGpmCredentialsIfDeviceCannotBeEnrolled \
  NoGpmCredentialsIfDeviceCannotBeEnrolled
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       MAYBE_NoGpmCredentialsIfDeviceCannotBeEnrolled) {
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Credential>(m.type);
      }));
  EXPECT_FALSE(
      request_delegate()->enclave_controller_for_testing()->is_active());
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NotAvailableIfLskfsAreTooOld DISABLED_NotAvailableIfLskfsAreTooOld
#else
#define MAYBE_NotAvailableIfLskfsAreTooOld NotAvailableIfLskfsAreTooOld
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       MAYBE_NotAvailableIfLskfsAreTooOld) {
  EnableUVKeySupport();
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.lskf_expiries = {base::Time::Now() + base::Days(1)};
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_FALSE(
      request_delegate()->enclave_controller_for_testing()->is_active());
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NoGpmForCrossPlatformAttachment \
  DISABLED_NoGpmForCrossPlatformAttachment
#else
#define MAYBE_NoGpmForCrossPlatformAttachment NoGpmForCrossPlatformAttachment
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       MAYBE_NoGpmForCrossPlatformAttachment) {
  EnableUVKeySupport();
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialCrossPlatform);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_TRUE(
      request_delegate()->enclave_controller_for_testing()->is_active());
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NoGpmCreationIfPasswordManagerDisabled \
  DISABLED_NoGpmCreationIfPasswordManagerDisabled
#else
#define MAYBE_NoGpmCreationIfPasswordManagerDisabled \
  NoGpmCreationIfPasswordManagerDisabled
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       MAYBE_NoGpmCreationIfPasswordManagerDisabled) {
  EnableUVKeySupport();
  CheckRegistrationStateNotRequested();

  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_FALSE(request_delegate()->enclave_controller_for_testing());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       EnrollAndCreate) {
  EnableUVKeySupport();
  security_domain_service_->pretend_there_are_members();

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  EXPECT_TRUE(
      request_delegate()->enclave_controller_for_testing()->is_active());
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       GetAssertionWithPlatformUV) {
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();
  EnableUVKeySupport();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       NotForSameGoogleAccount) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("accounts.google.com", "/title1.html")));
  EnableUVKeySupport();

  // The enclave should not appear when attempting to create a Google passkey
  // for the same account.
  {
    CheckRegistrationStateNotRequested();
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(
        web_contents,
        base::ReplaceStringPlaceholders(kMakeCredentialGoogle, {kEmail},
                                        /*offsets=*/nullptr));
    delegate_observer()->WaitForUI();
    EXPECT_TRUE(
        base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
          return absl::holds_alternative<
              AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
        }));
    dialog_model()->CancelAuthenticatorRequest();
    std::string script_result;
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
    delegate_observer()->WaitForDelegateDestruction();
  }

  // For Google-internal users, the username in the create request is just the
  // local part of the email address. Enclave should not appear for those cases
  // either.
  {
    CheckRegistrationStateNotRequested();
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(
        web_contents, base::ReplaceStringPlaceholders(kMakeCredentialGoogle,
                                                      {kEmailLocalPartOnly},
                                                      /*offsets=*/nullptr));
    delegate_observer()->WaitForUI();
    EXPECT_TRUE(
        base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
          return absl::holds_alternative<
              AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
        }));
    dialog_model()->CancelAuthenticatorRequest();
    std::string script_result;
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
    delegate_observer()->WaitForDelegateDestruction();
  }

  // But trying to create a passkey for a different account is fine.
  {
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        registration_state_result;
    registration_state_result.state =
        trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
            State::kRecoverable;
    SetMockVaultConnectionOnRequestDelegate(
        std::move(registration_state_result));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(
        web_contents,
        base::ReplaceStringPlaceholders(kMakeCredentialGoogle,
                                        {std::string(kEmail) + "_different"},
                                        /*offsets=*/nullptr));
    delegate_observer()->WaitForUI();
    EXPECT_FALSE(
        base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
          return absl::holds_alternative<
              AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
        }));
    dialog_model()->CancelAuthenticatorRequest();
    std::string script_result;
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
    delegate_observer()->WaitForDelegateDestruction();
  }
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       IncognitoModeMakeCredential) {
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      https_server_.GetURL("www.example.com", "/title1.html"));
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  security_domain_service_->pretend_there_are_members();

  // Initially bootstrap from LSKF, ensuring the incognito warning is shown.
  content::WebContents* web_contents =
      otr_browser->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(
      dialog_model()->step(),
      AuthenticatorRequestDialogModel::Step::kGPMConfirmOffTheRecordCreate);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  dialog_model()->OnGPMConfirmOffTheRecordCreate();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
  delegate_observer()->WaitForDelegateDestruction();

  // Ensure the incognito warning is also shown in the non-bootstrapping flow.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(
      dialog_model()->step(),
      AuthenticatorRequestDialogModel::Step::kGPMConfirmOffTheRecordCreate);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  dialog_model()->OnGPMConfirmOffTheRecordCreate();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnGPMCreatePasskey();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       IncognitoModeGetAssertion) {
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      https_server_.GetURL("www.example.com", "/title1.html"));
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      otr_browser->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

#if BUILDFLAG(IS_MAC)

bool MacBiometricApisAvailable() {
  if (__builtin_available(macOS 12, *)) {
    return true;
  }
  return false;
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       BiometricsDisabledDuringRequest) {
  if (!MacBiometricApisAvailable()) {
    GTEST_SKIP() << "Need macOS >= 12";
  }

  // If Touch ID is disabled during the course of a request, the UV disposition
  // shouldn't also change. I.e. if we started with the expectation of doing
  // UV=true, the UI expects that to continue, even if we need macOS to prompt
  // for the system password.
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();
  EnableUVKeySupport();

  SetBiometricsEnabled(true);

  // The first get() request is satisfied implicitly because recovery was done.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  model_observer()->WaitForStep();

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  // During this second get() request, Touch ID will be disabled.
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMTouchID);
  model_observer()->WaitForStep();
  SetBiometricsEnabled(false);
  // Disable Touch ID. The request should still resolve with uv=true.
  request_delegate()->dialog_model()->OnTouchIDComplete(false);

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

constexpr char kICloudKeychainRecoveryKeyAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING ".com.google.common.folsom";

class EnclaveICloudRecoveryKeyTest
    : public EnclaveAuthenticatorWithPinBrowserTest {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnICloudRecoveryKey};
  crypto::ScopedFakeAppleKeychainV2 scoped_fake_apple_keychain_{
      kICloudKeychainRecoveryKeyAccessGroup};
};

// Tests enrolling an iCloud recovery key when there are no keys already
// enrolled with the recovery service or present in iCloud keychain.
IN_PROC_BROWSER_TEST_F(EnclaveICloudRecoveryKeyTest, Enroll) {
  // Do a make credential request and enroll a PIN.
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  delegate_observer()->WaitForDelegateDestruction();

  // Find the iCloud recovery key member.
  const auto icloud_member = std::ranges::find_if(
      security_domain_service_->members(),
      [](const trusted_vault_pb::SecurityDomainMember& member) {
        return member.member_type() == trusted_vault_pb::SecurityDomainMember::
                                           MEMBER_TYPE_ICLOUD_KEYCHAIN;
      });
  ASSERT_NE(icloud_member, security_domain_service_->members().end());

  // Find the recovery key on iCloud keychain.
  base::test::TestFuture<
      std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>>
      future;
  device::enclave::ICloudRecoveryKey::Retrieve(
      future.GetCallback(), kICloudKeychainRecoveryKeyAccessGroup);
  EXPECT_TRUE(future.Wait());
  std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>
      recovery_keys = future.Take();
  ASSERT_EQ(recovery_keys.size(), 1u);
  std::unique_ptr<device::enclave::ICloudRecoveryKey> icloud_key =
      std::move(recovery_keys.at(0));

  // Make sure they match.
  EXPECT_EQ(trusted_vault::ProtoStringToBytes(icloud_member->public_key()),
            icloud_key->key()->public_key().ExportToBytes());
}

// Tests enrolling an iCloud recovery key when there is already a recovery key
// stored in iCloud keychain. A new key should be created.
// Regression test for https://crbug.com/360321350.
IN_PROC_BROWSER_TEST_F(EnclaveICloudRecoveryKeyTest,
                       EnrollWithExistingKeyInICloud) {
  // Create an iCloud recovery key.
  base::test::TestFuture<std::unique_ptr<device::enclave::ICloudRecoveryKey>>
      future;
  device::enclave::ICloudRecoveryKey::Create(
      future.GetCallback(), kICloudKeychainRecoveryKeyAccessGroup);
  EXPECT_TRUE(future.Wait());
  std::unique_ptr<device::enclave::ICloudRecoveryKey> existing_icloud_key =
      future.Take();
  ASSERT_TRUE(existing_icloud_key);

  // Do a make credential request and enroll a PIN.
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  delegate_observer()->WaitForDelegateDestruction();

  // Find the iCloud recovery key member.
  const auto icloud_member = std::ranges::find_if(
      security_domain_service_->members(),
      [](const trusted_vault_pb::SecurityDomainMember& member) {
        return member.member_type() == trusted_vault_pb::SecurityDomainMember::
                                           MEMBER_TYPE_ICLOUD_KEYCHAIN;
      });
  ASSERT_NE(icloud_member, security_domain_service_->members().end());

  // Make sure it does not match the existing key.
  EXPECT_NE(trusted_vault::ProtoStringToBytes(icloud_member->public_key()),
            existing_icloud_key->key()->public_key().ExportToBytes());

  // Instead, a new key should have been created.
  base::test::TestFuture<
      std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>>
      list_future;
  device::enclave::ICloudRecoveryKey::Retrieve(
      list_future.GetCallback(), kICloudKeychainRecoveryKeyAccessGroup);
  EXPECT_TRUE(list_future.Wait());
  std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>
      recovery_keys = list_future.Take();
  EXPECT_EQ(recovery_keys.size(), 2u);
}

// TODO(crbug.com/368799197): The test is flaky.
// Tests enrolling an iCloud recovery key, then recovering from it.
IN_PROC_BROWSER_TEST_F(EnclaveICloudRecoveryKeyTest, DISABLED_Recovery) {
  {
    // Do a make credential request and enroll a PIN.
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        registration_state_result;
    registration_state_result.state = trusted_vault::
        DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
    SetMockVaultConnectionOnRequestDelegate(
        std::move(registration_state_result));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
    delegate_observer()->WaitForUI();

    EXPECT_EQ(dialog_model()->step(),
              AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
    EXPECT_EQ(request_delegate()
                  ->enclave_controller_for_testing()
                  ->account_state_for_testing(),
              GPMEnclaveController::AccountState::kEmpty);
    dialog_model()->OnGPMCreatePasskey();
    EXPECT_EQ(dialog_model()->step(),
              AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
    dialog_model()->OnGPMPinEntered(u"123456");

    std::string script_result;
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
    EXPECT_EQ(script_result, "\"webauthn: OK\"");

    delegate_observer()->WaitForDelegateDestruction();

    // Make sure a new recovery key was enrolled.
    base::test::TestFuture<
        std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>>
        future;
    device::enclave::ICloudRecoveryKey::Retrieve(
        future.GetCallback(), kICloudKeychainRecoveryKeyAccessGroup);
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(future.Get().size(), 1u);
  }

  // Unenroll the current device from the enclave.
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->ClearRegistrationForTesting();
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->ResetForTesting();
  // Expire any cache.
  clock_.Advance(base::Hours(10));

  // Do a make credential request and recover with the iCloud key.
  {
    // Set up the mock trusted vault connection to download the iCloud recovery
    // factor that should have been added.
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        registration_state_result;
    const auto pin_member = std::ranges::find_if(
        security_domain_service_->members(),
        [](const trusted_vault_pb::SecurityDomainMember& member) {
          return member.member_type() ==
                 trusted_vault_pb::SecurityDomainMember::
                     MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN;
        });
    const auto& pin_metadata =
        pin_member->member_metadata().google_password_manager_pin_metadata();
    registration_state_result.gpm_pin_metadata = trusted_vault::GpmPinMetadata(
        pin_member->public_key(), pin_metadata.encrypted_pin_hash(),
        base::Time::FromSecondsSinceUnixEpoch(
            pin_metadata.expiration_time().seconds()));
    registration_state_result.state =
        trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
            State::kRecoverable;
    registration_state_result.key_version = kSecretVersion;
    const auto icloud_member = std::ranges::find_if(
        security_domain_service_->members(),
        [](const trusted_vault_pb::SecurityDomainMember& member) {
          return member.member_type() ==
                 trusted_vault_pb::SecurityDomainMember::
                     MEMBER_TYPE_ICLOUD_KEYCHAIN;
        });
    ASSERT_NE(icloud_member, security_domain_service_->members().end());
    std::vector<trusted_vault::MemberKeys> member_keys;
    auto member_key = icloud_member->memberships().at(0).keys().at(0);
    member_keys.emplace_back(
        member_key.epoch(),
        std::vector<uint8_t>(member_key.wrapped_key().begin(),
                             member_key.wrapped_key().end()),
        std::vector<uint8_t>(member_key.member_proof().begin(),
                             member_key.member_proof().end()));
    registration_state_result.icloud_keys.emplace_back(
        trusted_vault::SecureBoxPublicKey::CreateByImport(
            std::vector<uint8_t>(icloud_member->public_key().begin(),
                                 icloud_member->public_key().end())),
        std::move(member_keys));
    SetMockVaultConnectionOnRequestDelegate(
        std::move(registration_state_result));

    // Running the request should result in recovering automatically after the
    // "Trust this computer" screen.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
    delegate_observer()->WaitForUI();

    EXPECT_EQ(
        dialog_model()->step(),
        AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
    EXPECT_EQ(request_delegate()
                  ->enclave_controller_for_testing()
                  ->account_state_for_testing(),
              GPMEnclaveController::AccountState::kRecoverable);

    // User verification must not be skipped when recovering from an iCloud key.
    model_observer()->SetStepToObserve(
        AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
    dialog_model()->OnTrustThisComputer();
    model_observer()->WaitForStep();
    dialog_model()->OnGPMPinEntered(u"123456");

    std::string script_result;
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
    EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

    delegate_observer()->WaitForDelegateDestruction();

    // Make sure no new recovery key was enrolled.
    base::test::TestFuture<
        std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>>
        future;
    device::enclave::ICloudRecoveryKey::Retrieve(
        future.GetCallback(), kICloudKeychainRecoveryKeyAccessGroup);
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(future.Get().size(), 1u);
  }
}

#endif  // BUILDFLAG(IS_MAC)

class EnclaveAuthenticatorCachingTest
    : public EnclaveAuthenticatorWithoutPinBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kWebAuthnCacheSecurityDomain};
};

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorCachingTest, Caching) {
  EnableUVKeySupport();
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;

  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  // The enclave is not active because the account is empty.
  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));

  dialog_model()->CancelAuthenticatorRequest();
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  delegate_observer()->WaitForDelegateDestruction();

  // The enclave will _still_ not be active for a second request because the
  // previous result is cached, thus no call to
  // `SetMockVaultconnectionOnRequestDelegate` is needed.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));

  dialog_model()->CancelAuthenticatorRequest();
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  delegate_observer()->WaitForDelegateDestruction();

  clock_.Advance(base::Hours(10));
  request_delegate_ = nullptr;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  // Now that the clock has advanced, the cache is stale and the updated account
  // state will be noticed.
  EXPECT_FALSE(
      base::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));

  dialog_model()->CancelAuthenticatorRequest();
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  delegate_observer()->WaitForDelegateDestruction();
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_MakeCredentialDeclineGPM DISABLED_MakeCredentialDeclineGPM
#else
#define MAYBE_MakeCredentialDeclineGPM MakeCredentialDeclineGPM
#endif
// TODO(crbug.com/345308672): Failing on various Mac bots.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       MAYBE_MakeCredentialDeclineGPM) {
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  delegate_observer()->AddAdditionalTransport(
      device::FidoTransportProtocol::kInternal);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
  delegate_observer()->WaitForDelegateDestruction();

  // With the enclave configured, the next request should offer GPM as a
  // priority mechanism for an attachment=platform request.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialAttachmentPlatform);
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  dialog_model()->StartOver();
  model_observer()->WaitForStep();
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();

  content::ExecuteScriptAsync(web_contents, kMakeCredentialAttachmentPlatform);
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  dialog_model()->StartOver();
  model_observer()->WaitForStep();
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();

  // After backing out of GPM twice, the next attempt should default to
  // either mechanism selection or, on Mac, the custom platform authenticator
  // passkey creation dialog.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialAttachmentPlatform);
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();
}

// Attempt a GetAssertion multiple times with GPM passkey bootstrapping
// offered, and decline each time. The default should change to hybrid after
// two times declined.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest,
                       MultipleDeclinedBootstrappings) {
  EnableUVKeySupport();
  delegate_observer()->SetUseSyncedDeviceCablePairing(/*use_pairing=*/true);

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  dialog_model()->StartOver();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  EXPECT_TRUE(base::ranges::any_of(
      dialog_model()->mechanisms,
      [](const auto& m) { return IsMechanismEnclaveCredential(m); }));
  for (auto& mechanism : request_delegate_->dialog_model()->mechanisms) {
    if (IsMechanismEnclaveCredential(mechanism)) {
      mechanism.callback.Run();
      break;
    }
  }
  model_observer()->WaitForStep();

  // The second time simulate pressing the "Use [phone]" button.
  model_observer()->ObserveNextStep();
  dialog_model()->ContactPriorityPhone();
  model_observer()->WaitForStep();

  // Cancel and send a new request so newly-enumerated credentials will be used.
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();

  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  // Synced GPM passkeys should be hybrid credentials now.
  EXPECT_FALSE(base::ranges::any_of(
      dialog_model()->mechanisms,
      [](const auto& m) { return IsMechanismEnclaveCredential(m); }));
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       ChangedPINDetectedWhenDoingUV) {
  // Set up an account with a GPM PIN and create a credential. Then create a
  // second `EnclaveManager` to change the PIN. Lastly, assert that credential
  // with the updated GPM PIN for UV. This tests that the updated PIN is used
  // for the UV.
  const std::string pin = "123456";
  const std::string newpin = "111111";

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(base::UTF8ToUTF16(pin));

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  const std::optional<std::vector<uint8_t>> security_domain_secret =
      FakeMagicArch::RecoverWithPIN(pin, *security_domain_service_,
                                    *recovery_key_store_);

  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnGPMCreatePasskey();
  model_observer()->WaitForStep();
  dialog_model()->OnGPMPinEntered(base::UTF8ToUTF16(pin));

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  {
    Profile* const profile = browser()->profile();
    EnclaveManager second_manager(
        temp_dir_.GetPath(),
        IdentityManagerFactory::GetForProfile(browser()->profile()),
        base::BindRepeating(
            [](base::WeakPtr<Profile> profile)
                -> network::mojom::NetworkContext* {
              if (!profile) {
                return nullptr;
              }
              return profile->GetDefaultStoragePartition()->GetNetworkContext();
            },
            profile->GetWeakPtr()),
        url_loader_factory_.GetSafeWeakWrapper());

    second_manager.StoreKeys(kGaiaId, {*security_domain_secret},
                             /*last_key_version=*/kSecretVersion);

    base::test::TestFuture<bool> add_future;
    second_manager.AddDeviceToAccount(std::nullopt, add_future.GetCallback());
    EXPECT_TRUE(add_future.Wait());
    EXPECT_TRUE(add_future.Get());

    base::test::TestFuture<bool> change_future;
    second_manager.ChangePIN(newpin, "rapt", change_future.GetCallback());
    EXPECT_TRUE(change_future.Wait());
    ASSERT_TRUE(change_future.Get());
  }

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  dialog_model()->OnGPMPinEntered(base::UTF8ToUTF16(newpin));

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

#if BUILDFLAG(IS_LINUX)
// These tests are run on Linux because Linux has no platform authenticator
// that can effect whether IsUVPAA returns true or not.

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithoutPinBrowserTest, IsUVPAA) {
  // We don't know, at IsUVPAA time, whether there's an Android LSKF on the
  // account and, without GPM PIN support, that means that we have to assume
  // that the enclave authenticator isn't available.
  EXPECT_FALSE(IsUVPAA());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest, IsUVPAA) {
  // With the enclave authenticator in place, IsUVPAA should return true.
  EXPECT_TRUE(IsUVPAA());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       IsUVPAA_GoogleSite) {
  // With the enclave authenticator in place, IsUVPAA should return false for
  // google.com sites because we won't create a credential for an account in
  // that same account. But since we don't know the user.id value at IsUVPAA
  // time, the result has to be conservative.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("accounts.google.com", "/title1.html")));
  EXPECT_FALSE(IsUVPAA());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       IsUVPAA_NoUnexportableKeys) {
  // Without support for unexportable keys, IsUVPAA should return false because
  // the enclave cannot be used.
  mock_hw_provider_.reset();
  crypto::ScopedNullUnexportableKeyProvider no_hw_key_support;
  EXPECT_FALSE(IsUVPAA());
}

#endif  // IS_LINUX

// Verify that GPM will do UV on a uv=preferred request if and only if
// biometrics are available.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       UserVerificationPolicy) {
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();
  EnableUVKeySupport();

  // The first get() request is satisfied implicitly because recovery was done.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  SetBiometricsEnabled(false);

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  dialog_model()->OnUserConfirmedPriorityMechanism();

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=false\"");

  // On Linux biometrics is not available so the test is done.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_MAC)
  if (!MacBiometricApisAvailable()) {
    return;
  }
#endif
  SetBiometricsEnabled(true);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMTouchID);
  dialog_model()->OnTouchIDComplete(true);
#else
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  dialog_model()->OnUserConfirmedPriorityMechanism();
#endif

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
#endif
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest, Bug_354083161) {
  // Reproduces the crash from b/354083161

  // Do an assertion to set up the enclave.
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  // Do an assertion to trigger the UI pattern that caused the crash.
  content::ExecuteScriptAsync(
      web_contents,
      base::ReplaceStringPlaceholders(
          kGetAssertionUvDiscouragedWithCredId,
          {base::Base64Encode(TestProtobufCredId())}, /*offsets=*/nullptr));
  delegate_observer()->WaitForUI();

  ASSERT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kPreSelectSingleAccount);
  dialog_model()->OnAccountPreselectedIndex(0);

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       NoSilentOperations) {
  // Check that the enclave doesn't allow silent operations.

  // Do an assertion to set up the enclave.
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser()->profile())
      ->StoreKeys(kGaiaId,
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  // Do an assertion with an allowlist and check that the assertion isn't
  // immediately run.
  content::ExecuteScriptAsync(
      web_contents,
      base::ReplaceStringPlaceholders(
          // Setting the transport hint to just `internal` means that there are
          // no other mechanisms and so the credential will be the priority
          // mechanism.
          kGetAssertionUvDiscouragedWithCredIdAndInternalTransport,
          {base::Base64Encode(TestProtobufCredId())}, /*offsets=*/nullptr));
  delegate_observer()->WaitForUI();

  // The UI must not be, e.g., kGPMConnecting as that indicates that the
  // operation is happening without any UI.
  ASSERT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
}

// Allows a `BlockingUnexportableKeyProvider` to block inside a thread-pool
// thread so that the main test can synchronize with it on the UI thread.
class BlockingUnexportableKeyProviderRendezvous {
 public:
  void Block() {
    base::ScopedAllowBaseSyncPrimitivesForTesting locks_allowed;
    base::AutoLock locked(lock_);

    blocked_ = true;
    while (!ready_to_continue_) {
      condition_.Wait();
    }
  }

  bool IsBlocked() {
    base::AutoLock locked(lock_);
    return blocked_;
  }

  void Continue() {
    base::AutoLock locked(lock_);
    CHECK(!ready_to_continue_);

    ready_to_continue_ = true;
    condition_.Broadcast();
  }

 private:
  base::Lock lock_;
  base::ConditionVariable condition_{&lock_};
  bool blocked_ GUARDED_BY(lock_) = false;
  bool ready_to_continue_ GUARDED_BY(lock_) = false;
};

BlockingUnexportableKeyProviderRendezvous&
GetBlockingUnexportableKeyProviderRendezvous() {
  static base::NoDestructor<BlockingUnexportableKeyProviderRendezvous> instance;
  return *instance;
}

// An `UnexportableKeyProvider` that blocks inside `SelectAlgorithm` and waits
// for the UI thread to synchronize with it. It doesn't implement any other
// functions.
class BlockingUnexportableKeyProvider : public crypto::UnexportableKeyProvider {
 public:
  std::optional<crypto::SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    CHECK(!acceptable_algorithms.empty());

    // This function runs in a thread-pool thread.
    GetBlockingUnexportableKeyProviderRendezvous().Block();
    return acceptable_algorithms[0];
  }

  std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    NOTREACHED();
  }

  std::unique_ptr<crypto::UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    NOTREACHED();
  }

  bool DeleteSigningKeySlowly(base::span<const uint8_t> wrapped_key) override {
    NOTREACHED();
  }
};

std::unique_ptr<crypto::UnexportableKeyProvider>
BlockingUnexportableKeyProviderFactory() {
  return std::make_unique<BlockingUnexportableKeyProvider>();
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorWithPinBrowserTest,
                       CancelRacesTPMCheck) {
  // https://crbug.com/352532554

  // Set the UnexportableKeyProvider to one that will block inside
  // `SelectAlgorithm` so that we can simulate a slow TPM check.
  mock_hw_provider_.reset();
  crypto::internal::SetUnexportableKeyProviderForTesting(
      BlockingUnexportableKeyProviderFactory);

  // Start a WebAuthn request. It'll block when checking the TPM.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents, kAbortableGetAssertion));

  // Wait until the request is blocked on checking the TPM.
  auto run_loop = std::make_unique<base::RunLoop>();
  while (!GetBlockingUnexportableKeyProviderRendezvous().IsBlocked()) {
    run_loop->RunUntilIdle();
  }

  // Cancel the outstanding request.
  EXPECT_TRUE(content::ExecJs(web_contents, kAbort));

  // Let the TPM check complete.
  GetBlockingUnexportableKeyProviderRendezvous().Continue();
  run_loop->RunUntilIdle();

  // This test is successful if it doesn't crash. It reliably crashed prior to
  // the fix for https://crbug.com/352532554.
}

}  // namespace

#endif  // !defined(MEMORY_SANITIZER)
