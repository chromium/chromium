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
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webauthn/passkey_upgrade_request_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/enclave_authenticator_browsertest_base.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/fake_magic_arch.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/browser/webauthn/unexportable_key_utils.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_scoped_fake_unexportable_key_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/device_event_log/device_event_log.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/test/test_future.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"
#include "chrome/common/chrome_version.h"
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "device/fido/mac/fake_icloud_keychain.h"
#endif  // BUILDFLAG(IS_MAC)

// These tests are disabled under MSAN. The enclave subprocess is written in
// Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace {

using trusted_vault::MockTrustedVaultThrottlingConnection;

static constexpr char kMakeCredentialLargeBlob[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{ type: "public-key", alg: -7 }],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: "discouraged",
      requireResidentKey: true
    },
    // Ask for large-blob support at registration time.
    extensions: { largeBlob: { support: "preferred" } },
  }}).then(c => {
    const lb = c.getClientExtensionResults().largeBlob;
    // Pass back the value we care about.
    window.domAutomationController.send(
        "largeblob " + (lb ? lb.supported : lb));
  }, e => window.domAutomationController.send("error " + e));
})())";

static constexpr char kGetAssertionWriteLargeBlob[] = R"((() => {
  const credIdB64 = "$1";
  const blob      = new TextEncoder().encode("hello world");

  // helper
  const b64ToBuf = b64 => {
    const bin = atob(b64);
    const u8  = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; ++i) u8[i] = bin.charCodeAt(i);
    return u8.buffer;
  };

  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: "discouraged",
    allowCredentials: [{ type: "public-key", id: b64ToBuf(credIdB64) }],
    extensions: { largeBlob: { write: blob } },
  }}).then(_ => window.domAutomationController.send("write ok"),
           e  => window.domAutomationController.send("error " + e));
})())";

static constexpr char kGetAssertionReadLargeBlob[] = R"((() => {
  const credIdB64 = "$1";

  const b64ToBuf = b64 => {
    const bin = atob(b64);
    const u8  = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; ++i) u8[i] = bin.charCodeAt(i);
    return u8.buffer;
  };

  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: "discouraged",
    allowCredentials: [{ type: "public-key", id: b64ToBuf(credIdB64) }],
    extensions: { largeBlob: { read: true } },
  }}).then(c => {
      const lb = c.getClientExtensionResults().largeBlob;
      const txt = lb && lb.blob
                  ? new TextDecoder().decode(lb.blob)
                  : "";
      window.domAutomationController.send("read " + txt);
    },
    e => window.domAutomationController.send("error " + e));
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

static constexpr char kGetAssertionSecurityKey[] = R"((() => {
  const credId = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [
      {type: 'public-key', id: new Uint8Array(credId), transports: ["usb"]}
    ],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionCredId1[] = R"((() => {
  const credId = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    allowCredentials: [{type: 'public-key', id: new Uint8Array(credId)}],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionUvDiscouragedWithGoogleRp[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    rpId: "google.com",
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

static constexpr char kMakeCredentialConditionalCreate[] = R"((() => {
  return navigator.credentials.create({
    mediation: "conditional",
    publicKey: {
      rp: { name: "www.example.com" },
      user: {
        id: new Uint8Array([1]),
        name: "user1@gmail.com",
        displayName: "Foo Bar"
      },
      pubKeyCredParams: [{type: "public-key", alg: -7}],
      challenge: new Uint8Array([0]),
    }
  }).then(c => window.domAutomationController.send('webauthn: ' + c.id),
          e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialConditionalCreateWithExcludeList[] =
    R"((() => {
  const base64ToArrayBuffer = (base64) => {
    const bytes = window.atob(base64);
    const len = bytes.length;
    const ret = new Uint8Array(len);
    for (var i = 0; i < len; i++) {
        ret[i] = bytes.charCodeAt(i);
    }
    return ret.buffer;
  }
  return navigator.credentials.create({
    mediation: "conditional",
    publicKey: {
      rp: { name: "www.example.com" },
      user: {
        id: new Uint8Array([1]),
        name: "user1@gmail.com",
        displayName: "Foo Bar"
      },
      pubKeyCredParams: [{type: "public-key", alg: -7}],
      challenge: new Uint8Array([0]),
      excludeCredentials: [{type: "public-key",
                            transports: [],
                            id: base64ToArrayBuffer("$1")}],
    }
  }).then(c => window.domAutomationController.send('webauthn: ' + c.id),
          e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kSignalHideTestPasskey[] = R"((() => {
  PublicKeyCredential.signalAllAcceptedCredentials({
    rpId: "www.example.com",
    allAcceptedCredentialIds: [],
    userId: "AA",
  }).then(c => window.domAutomationController.send('webauthn: OK'),
          e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kSignalRestoreTestPasskey[] = R"((() => {
  PublicKeyCredential.signalAllAcceptedCredentials({
    rpId: "www.example.com",
    allAcceptedCredentialIds: ["SHQCLMWFONoi2Iyv1AUphA"],
    userId: "AA",
  }).then(c => window.domAutomationController.send('webauthn: OK'),
          e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionViaButtonClickImmediateUvPreferred[] = R"(
  document.body.innerHTML = '<button id="testButton"">Get Assertion</button>';
  function triggerGetAssertion() {
    navigator.credentials.get({
      mediation: "immediate",
      publicKey: {
        challenge: new Uint8Array([0]),
        timeout: 10000,
        userVerification: 'preferred',
      }
    }).then(c => window.domAutomationController.send('webauthn: OK'),
             e => window.domAutomationController.send('error ' + e));
  }
  const button = document.getElementById('testButton');
  button.addEventListener('click', triggerGetAssertion);
)";

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

std::string GetDeviceLog() {
  return device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"fido",
      /*max_level=*/device_event_log::LOG_LEVEL_EVENT, /*max_events=*/0);
}

bool IsMechanismEnclaveCredential(
    const AuthenticatorRequestDialogModel::Mechanism& mechanism) {
  if (std::holds_alternative<
          AuthenticatorRequestDialogModel::Mechanism::Credential>(
          mechanism.type)) {
    return std::get<AuthenticatorRequestDialogModel::Mechanism::Credential>(
               mechanism.type)
               ->source == device::AuthenticatorType::kEnclave;
  }
  return false;
}

class EnclaveAuthenticatorBrowserTest : public EnclaveAuthenticatorTestBase {
 public:
  class DelegateObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    explicit DelegateObserver(EnclaveAuthenticatorBrowserTest* test_instance)
        : test_instance_(test_instance) {}
    virtual ~DelegateObserver() = default;

    void WaitForUI() {
      ui_shown_run_loop_->Run();
      ui_shown_run_loop_ = std::make_unique<base::RunLoop>();
    }

    void WaitForPreTransportAvailabilityEnumerated() {
      pre_tai_run_loop_->Run();
      pre_tai_run_loop_ = std::make_unique<base::RunLoop>();
    }

    void RunMakeCredentialWithLargeBlobSupport(std::string* out_b64);

    void WaitForDelegateDestruction() {
      destruction_run_loop_->Run();
      destruction_run_loop_ = std::make_unique<base::RunLoop>();
    }

    void AddAdditionalTransport(device::FidoTransportProtocol transport) {
      additional_transport_ = transport;
    }

    void SetUseSyncedDeviceCablePairing(bool use_pairing) {
      use_synced_device_cable_pairing_ = use_pairing;
    }

    bool ui_shown() { return ui_shown_; }

    bool on_transport_availability_enumerated_called() {
      return on_transport_availability_enumerated_called_;
    }

    const std::optional<base::flat_set<device::FidoTransportProtocol>>&
    transports_observed() const {
      return transports_observed_;
    }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(delegate);
      GpmTickAndTaskRunnerProvider::SetOverrideForFrame(
          delegate->GetRenderFrameHost(),
          test_instance_->timer_task_runner_->GetMockTickClock(),
          test_instance_->timer_task_runner_);
      transports_observed_ = std::nullopt;
    }

    void PreStartOver() override {
      // Start over creates a new request handler and invokes
      // OnTransportAvailabilityEnumerated() again.
      transports_observed_ = std::nullopt;
    }

    void OnDestroy(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(nullptr);
      destruction_run_loop_->QuitWhenIdle();
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {
      CHECK(!transports_observed_);
      transports_observed_ = tai->available_transports;
      if (additional_transport_.has_value()) {
        tai->available_transports.insert(*additional_transport_);
      }
      on_transport_availability_enumerated_called_ = true;
    }

    void OnPreTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate) override {
      pre_tai_run_loop_->QuitWhenIdle();
    }

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      ui_shown_ = true;
      ui_shown_run_loop_->QuitWhenIdle();
    }

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) override {}

    void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>& responses)
        override {}

   private:
    raw_ptr<EnclaveAuthenticatorBrowserTest> test_instance_;
    std::optional<base::flat_set<device::FidoTransportProtocol>>
        transports_observed_;
    std::optional<device::FidoTransportProtocol> additional_transport_;
    bool use_synced_device_cable_pairing_ = false;
    bool ui_shown_ = false;
    bool on_transport_availability_enumerated_called_ = false;
    std::unique_ptr<base::RunLoop> ui_shown_run_loop_ =
        std::make_unique<base::RunLoop>();
    std::unique_ptr<base::RunLoop> pre_tai_run_loop_ =
        std::make_unique<base::RunLoop>();
    std::unique_ptr<base::RunLoop> destruction_run_loop_ =
        std::make_unique<base::RunLoop>();
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

    // This will return immediately if the step is not kNotStarted. Otherwise,
    // it will wait for the next step, whichever it may be.
    void WaitForStart() {
      if (model_->step() !=
          AuthenticatorRequestDialogModel::Step::kNotStarted) {
        return;
      }
      ObserveNextStep();
      WaitForStep();
    }

    // AuthenticatorRequestDialogModel::Observer:
    void OnStepTransition() override {
      all_steps_.push_back(model_->step());

      if (run_loop_ && (observe_next_step_ || step_ == model_->step())) {
        run_loop_->QuitWhenIdle();
      }
    }

    void OnLoadingEnclaveTimeout() override {
      loading_enclave_timed_out_ = true;
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

    bool loading_enclave_timed_out() { return loading_enclave_timed_out_; }

   private:
    raw_ptr<AuthenticatorRequestDialogModel> model_;
    AuthenticatorRequestDialogModel::Step step_ =
        AuthenticatorRequestDialogModel::Step::kNotStarted;
    std::vector<AuthenticatorRequestDialogModel::Step> all_steps_;
    bool loading_enclave_timed_out_ = false;
    bool observe_next_step_ = false;
    std::unique_ptr<base::RunLoop> run_loop_;
  };

  EnclaveAuthenticatorBrowserTest() = default;
  ~EnclaveAuthenticatorBrowserTest() override = default;

  EnclaveAuthenticatorBrowserTest(const EnclaveAuthenticatorBrowserTest&) =
      delete;
  EnclaveAuthenticatorBrowserTest& operator=(
      const EnclaveAuthenticatorBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    EnclaveAuthenticatorTestBase::SetUpOnMainThread();

    delegate_observer_ = std::make_unique<DelegateObserver>(this);
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        delegate_observer_.get());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));
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

  void SetVaultConnectionToTimeout() {
    auto connection = std::make_unique<
        testing::NiceMock<MockTrustedVaultThrottlingConnection>>();
    EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                                 testing::_, testing::_, testing::_))
        .WillOnce(
            [](const CoreAccountInfo&,
               base::OnceCallback<void(AuthenticationFactorsResult)> callback,
               base::RepeatingClosure _) mutable {
              return std::make_unique<
                  trusted_vault::TrustedVaultConnection::Request>();
            });
    GpmTrustedVaultConnectionProvider::SetOverrideForFrame(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame(),
        std::move(connection));
  }

  void CheckRegistrationStateNotRequested() {
    auto connection = std::make_unique<
        testing::NiceMock<MockTrustedVaultThrottlingConnection>>();
    EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                                 testing::_, testing::_, testing::_))
        .WillRepeatedly(
            [](const CoreAccountInfo&,
               base::OnceCallback<void(AuthenticationFactorsResult)> callback,
               base::RepeatingClosure _)
                -> std::unique_ptr<
                    trusted_vault::TrustedVaultConnection::Request> {
              NOTREACHED() << "account state unexpectedly requested";
            });
    CHECK(!request_delegate_);
    GpmTrustedVaultConnectionProvider::SetOverrideForFrame(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame(),
        std::move(connection));
  }

 protected:
  std::unique_ptr<DelegateObserver> delegate_observer_;
  std::unique_ptr<ModelObserver> model_observer_;
  raw_ptr<ChromeAuthenticatorRequestDelegate> request_delegate_;
  base::HistogramTester histogram_tester_;
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

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  // Ensure the security domain secret is redacted from logs.
  EXPECT_THAT(GetDeviceLog(), testing::HasSubstr("\"secret\": \"[redacted]\""));

  // Verify the secret was redacted when being sent for wrapping.
  EXPECT_THAT(GetDeviceLog(), testing::HasSubstr("\"key\": \"[redacted]\""));
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, NonWebauthnRequest) {
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    GTEST_SKIP() << "With kSecurePaymentConfirmationBrowserBoundKeys the "
                    "SecurePaymentConfirmationService directs the request to "
                    "the internal authenticator.";
  }
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

// Regression test for https://crbug.com/451876194.
// Tests a make credential operation when the enclave is already loaded and
// ready and checking for UV takes long enough that the enclave is selected
// before the check is complete. At the time of writing, that resulted in a GPM
// failure.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MakeCredentialEnclaveLoadedButWaitingForUv) {
  // First we need to set up and make ready the enclave with a PIN.
  SetTrustedVaultEmpty();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  // At this point, the enclave should be ready with a PIN. Make a UV = required
  // request, and stall the UV key check.
  crypto::UserVerifyingKeysSupportedCallback uv_callback;
  crypto::ScopedUserVerifyingKeysSupportedOverride uvkey_supported_override(
      base::BindLambdaForTesting(
          [&](crypto::UserVerifyingKeysSupportedCallback callback) {
            uv_callback = std::move(callback);
          }));
  SetTrustedVaultRecoverable();
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);

  // The enclave is still loading so the UI should not be shown yet.
  delegate_observer()->WaitForUI();
  ASSERT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kNotStarted);
  ASSERT_TRUE(uv_callback);

  // Run the UV callback, which should advance the UI and resolve the request.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  std::move(uv_callback).Run(false);
  model_observer()->WaitForStep();

  // Finish the request.
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnGPMPinEntered(u"123456");
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, MakeCredentialWithPrf) {
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
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  const std::string kEval =
      "eval: { first: new Uint8Array([1]), second: new Uint8Array([2]) }";
  content::ExecuteScriptAsync(
      web_contents, base::ReplaceStringPlaceholders(
                        kMakeCredentialWithPrf, {kEval}, /*offsets=*/nullptr));
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMCreatePasskey();

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));

  std::tie(enabled, first, second) = ParsePrfResult(script_result);
  EXPECT_TRUE(enabled);
  EXPECT_EQ(first, "none");
  EXPECT_EQ(second, "none");

  // Ensure the PRF is redacted from logs.
  EXPECT_THAT(GetDeviceLog(), testing::HasSubstr("\"prf\": \"[redacted]\""));
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, GetAssertionWithPrf) {
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
  SetTrustedVaultRecoverable();
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

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  SimulateTrustedVaultKeyRetrieval();
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

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
  AuthenticationFactorsResult registration_state_result;
  registration_state_result.state =
      AuthenticationFactorsResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  registration_state_result.gpm_pin_metadata = trusted_vault::GpmPinMetadata(
      "public key", trusted_vault::UsableRecoveryPinMetadata(
                        EnclaveManager::MakeWrappedPINForTesting(
                            kSecurityDomainSecret, "123456"),
                        /*expiry=*/base::Time::Now() + base::Seconds(10000)));
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  security_domain_service_->pretend_there_are_members();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  SimulateTrustedVaultKeyRetrieval();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

// Regression test for https://crbug.com/465139934 ("Chrome crashes after
// unlocking passkeys in a different browser tab").
IN_PROC_BROWSER_TEST_F(
    EnclaveAuthenticatorBrowserTest,
    MakeCredential_WhenPasskeysUnlockedViaExplicitFlowInOtherTab) {
  // Starting from the passkey locked state.
  SetTrustedVaultRecoverable();
  EnableUVKeySupport();

  // Opening a browser tab and perforing the passkey creation operation there.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();
  // Since passkeys are locked, the passkey creation operation triggers the
  // passkey unlock flow.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();

  // After the first step of the given passkey unlock flow we are simulating
  // the concurrent passkey unlocking: storing a key from an explicit flow and
  // adding device to account.
  {
    // The acquired lock indicates to Enclave Manager that the keys are being
    // retrieved via explicit key retrieval flow.
    auto store_keys_lock = enclave_manager().GetStoreKeysLock();
    SimulateTrustedVaultKeyRetrieval();
    base::test::TestFuture<bool> add_future;
    enclave_manager().AddDeviceToAccount(std::nullopt,
                                         add_future.GetCallback());
    EXPECT_TRUE(add_future.Wait());
    EXPECT_TRUE(add_future.Get());
  }

  // Resuming the passkey creation flow.
  dialog_model()->OnTrustThisComputer();
  // Since passkeys are already unlocked, GPM Enclave Controller is supposed to
  // restart the passkey creation operation (so the expected step is
  // `kGPMCreatePasskey`).
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->OnGPMCreatePasskey();
  // And it is expected that the passkey can be successfully created.
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

// Regression test for https://crbug.com/465139934 ("Chrome crashes after
// unlocking passkeys in a different browser tab").
IN_PROC_BROWSER_TEST_F(
    EnclaveAuthenticatorBrowserTest,
    MakeCredential_WhenPasskeysUnlockedViaOpportunisticFlowInOtherTab) {
  // Starting from the passkey locked state.
  SetTrustedVaultRecoverable();
  EnableUVKeySupport();

  // Opening a browser tab and perforing the passkey creation operation there.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();
  // Since passkeys are locked, the passkey creation operation triggers the
  // passkey unlock flow.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();

  // After the first step of the given passkey unlock flow we are simulating
  // the concurrent passkey unlocking: storing a key from an opportunistic flow.
  // As a part of opportunistic key retrieval flow Enclave Manager adds device
  // to account.
  SimulateOpportunisticTrustedVaultKeyRetrieval();

  // Resuming the passkey creation flow.
  dialog_model()->OnTrustThisComputer();
  // Since passkeys are already unlocked, GPM Enclave Controller is supposed to
  // restart the passkey creation operation (so the expected step is
  // `kGPMCreatePasskey`).
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->OnGPMCreatePasskey();
  // And it is expected that the passkey can be successfully created.
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

// Regression test for https://crbug.com/465139934 ("Chrome crashes after
// unlocking passkeys in a different browser tab"). This test simulates
// unlocking passkeys and creating a GPM PIN, with concurrent unlocking of
// passkeys during the PIN creation.
IN_PROC_BROWSER_TEST_F(
    EnclaveAuthenticatorBrowserTest,
    MakeCredential_AndCreateGpmPin_WhenPasskeysUnlockedViaOpportunisticFlowInOtherTab) {
  // Starting from the passkey locked state and empty security domain (for
  // ensuring that we will be prompted to create a PIN).
  EnableUVKeySupport();
  SetTrustedVaultEmpty();

  // Opening a browser tab and perforing the passkey creation operation there.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();
  // Since passkeys are locked and there are no security domain members, the
  // passkey creation operation triggers the PIN creation flow.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->OnGPMCreatePasskey();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  model_observer()->WaitForStep();

  // After the PIN creation prompt we are simulating the concurrent passkey
  // unlocking: storing a key from an opportunistic flow. As a part of
  // opportunistic key retrieval flow Enclave Manager adds device to account.
  SetTrustedVaultRecoverable();
  SimulateOpportunisticTrustedVaultKeyRetrieval();

  // Resuming the passkey and PIN creation flow.
  dialog_model()->OnGPMPinEntered(u"123456");
  // Since passkeys are already unlocked, GPM Enclave Controller is supposed to
  // restart the passkey creation operation (so the expected step is
  // `kGPMCreatePasskey`).
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->OnGPMCreatePasskey();
  // And it is expected that the passkey can be successfully created.
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

// Regression test for https://crbug.com/445161563 ("Chrome on Linux crashes
// after removing passkey access in GPM settings").
// TODO(http://crbug.com/467196933) Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MakeCredential_WhenPasskeysBecomingUnregistered \
  DISABLED_MakeCredential_WhenPasskeysBecomingUnregistered
#else
#define MAYBE_MakeCredential_WhenPasskeysBecomingUnregistered \
  MakeCredential_WhenPasskeysBecomingUnregistered
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MAYBE_MakeCredential_WhenPasskeysBecomingUnregistered) {
  // Starting from the passkeys unlocked state.
  EnableUVKeySupport();
  SetTrustedVaultRecoverable();
  SimulateOpportunisticTrustedVaultKeyRetrieval();

  // Opening a browser tab and perforing the passkey creation operation there.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();
  // Since passkeys are unlocked, the expected step is `kGPMCreatePasskey`.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();

  // After the first step of the given passkey creation flow we are simulating
  // the concurrent removal of the passkey access by clearing registration of
  // Enclave Manager.
  enclave_manager().ClearRegistrationForTesting();
  // But we are keeping the trusted vault recoverable.
  SetTrustedVaultRecoverable();

  // Resuming the passkey creation flow.
  dialog_model()->OnGPMCreatePasskey();
  // GPM Enclave Controller is supposed to recognize that passkeys are locked
  // now, and the passkey creation operation must be restarted. Since passkeys
  // are locked, the user will need to unlock passkeys (so the expected step is
  // `kTrustThisComputerCreation`).
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();
  dialog_model()->OnTrustThisComputer();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  model_observer()->WaitForStep();
  // Unlocking passkeys by performing explicit key retrieval.
  {
    auto store_keys_lock = enclave_manager().GetStoreKeysLock();
    SimulateTrustedVaultKeyRetrieval();
  }
  model_observer()->WaitForStep();
  // And it is expected that the passkey can be successfully created.
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
  SetTrustedVaultRecoverable();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();
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
  SimulateTrustedVaultKeyRetrieval();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

// Tests recovering from an LSKF when there is also a GPM PIN that cannot be
// used for recovery. Regression test for crbug.com/402427390.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MakeCredential_RecoverWithLSKFAndUnusablePIN) {
  // First, register with a PIN.
  {
    SetTrustedVaultEmpty();
  }
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  // Then, have the security domain service mark the PIN as unusable and recover
  // from an LSKF.
  security_domain_service_->MakePinMemberUnusable();
  enclave_manager().ClearRegistrationForTesting();
  AuthenticationFactorsResult registration_state_result;
  registration_state_result.state =
      AuthenticationFactorsResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  registration_state_result.gpm_pin_metadata = trusted_vault::GpmPinMetadata(
      security_domain_service_->GetPinMemberPublicKey(),
      /*pin_metadata=*/std::nullopt);
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();
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
  SimulateTrustedVaultKeyRetrieval();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       CreatingDuplicateGivesInvalidStateError) {
  SetTrustedVaultRecoverable();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialReturnId);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();
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
  SimulateTrustedVaultKeyRetrieval();
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
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
  dialog_model()->OnGPMCreatePasskey();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("InvalidStateError"));
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();

  class PasskeyModelObserver : public webauthn::PasskeyModel::Observer {
   public:
    void OnPasskeysChanged(
        const std::vector<webauthn::PasskeyModelChange>& changes) override {
      for (const auto& change : changes) {
        if (change.type() == webauthn::PasskeyModelChange::ChangeType::UPDATE) {
          CHECK(!did_update);
          did_update = true;
        }
      }
    }

    void OnPasskeyModelShuttingDown() override {}
    void OnPasskeyModelIsReady(bool is_ready) override {}

    bool did_update = false;
  };
  PasskeyModelObserver passkey_model_observer;
  passkey_model().AddObserver(&passkey_model_observer);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  SimulateTrustedVaultKeyRetrieval();
  model_observer()->WaitForStep();

  EXPECT_FALSE(passkey_model_observer.did_update);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
  passkey_model().RemoveObserver(&passkey_model_observer);
  EXPECT_TRUE(passkey_model_observer.did_update);
  auto passkeys = passkey_model().GetAllPasskeys();
  ASSERT_EQ(passkeys.size(), 1u);
  // The update time should be in the last 10 minutes.
  EXPECT_LT((base::Time::Now() -
             base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
                 passkeys[0].last_used_time_windows_epoch_micros())))
                .InMinutes(),
            10);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
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

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  enclave_manager().ResetForTesting();

  EXPECT_EQ(enclave_manager().is_loaded(), false);

  // Checks that a following request goes straight to ready state.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  EXPECT_TRUE(IsReady(request_delegate()
                          ->enclave_controller_for_testing()
                          ->account_state_for_testing()));
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, UserCancelsUV) {
  EnableUVKeySupport();
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  dialog_model()->OnUserConfirmedPriorityMechanism();
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  // Do a get() where the signing fails, simulating the user canceling the
  // request. There should not be any Chrome error UI.

  fake_uv_provider_.emplace<crypto::ScopedFailingUserVerifyingKeyProvider>();
  enclave_manager().ClearCachedKeysForTesting();

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();

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
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       ConditionalMediationLoading) {
  // Set up a trusted vault connection that lets us control the time it
  // resolves.
  SetTrustedVaultSlowAndCacheCallback();

  // Execute a conditional UI request.
  AddTestPasskeyToModel();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionConditionalUI);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kPasskeyAutofill);
  model_observer()->WaitForStep();

  dialog_model()->OnAccountPreselectedIndex(0);

  // The modal UI should not be shown yet.
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kPasskeyAutofill);

  // Resolve the connection and wait for the next step.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  AuthenticationFactorsResult registration_state_result;
  registration_state_result.state =
      AuthenticationFactorsResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  std::move(cached_connection_cb()).Run(std::move(registration_state_result));
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       ConditionalAssertionWhileFullySetUp) {
  // This test reproduces crbug.com/374366241. It performs a conditional request
  // to generate an assertion and then triggers another conditional UI request.
  // At one point this crashed.
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionConditionalUI);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kPasskeyAutofill);
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnAccountPreselectedIndex(0);
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  SimulateTrustedVaultKeyRetrieval();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  content::ExecuteScriptAsync(web_contents, kGetAssertionConditionalUI);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kPasskeyAutofill);
  model_observer()->WaitForStep();
  // Not crashing here is success.
  dialog_model()->OnAccountPreselectedIndex(0);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, GpmEnclaveNeedsReauth) {
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
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  model_observer()->WaitForStep();
  ASSERT_EQ(browser()->tab_strip_model()->GetTabCount(), 1);

  // No credentials should be displayed since tapping on them won't work.
  EXPECT_FALSE(
      std::ranges::any_of(dialog_model()->mechanisms, [](const auto& m) {
        return std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Credential>(m.type);
      }));

  // The button has text indicating the user they need to sign in.
  const auto sign_in_again_mech =
      std::ranges::find_if(dialog_model()->mechanisms, [](const auto& m) {
        return std::holds_alternative<
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

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       NoReauthButtonForSecurityKeyRequests) {
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

  // Make a get assertion request that has a USB-only list of transports.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionSecurityKey);
  delegate_observer()->WaitForUI();

  // The reauth button should not be displayed.
  EXPECT_FALSE(
      std::ranges::any_of(dialog_model()->mechanisms, [](const auto& m) {
        return std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::SignInAgain>(m.type);
      }));
}

// Tests that if the enclave is the default, but loading takes too long, the
// user is sent to the mechanism selection screen instead.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       EnclaveIsDefaultButTakesTooLong) {
  // Set up a trusted vault connection that lets us control the time it
  // resolves.
  SetTrustedVaultSlowAndCacheCallback();

  // Execute a make credential request.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);

  // The UI should be made ready, but not shown yet.
  delegate_observer()->WaitForUI();
  ASSERT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kNotStarted);

  // Wait for time it takes to decide to jump to the mechanism selection screen.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  timer_task_runner_->FastForwardBy(GPMEnclaveController::kLoadingTimeout);
  model_observer()->WaitForStep();
  EXPECT_FALSE(dialog_model()->ui_disabled_);

  // Select Google Password Manager. This should trigger the loading UI.
  dialog_model()->OnGPMSelected();
  EXPECT_TRUE(dialog_model()->ui_disabled_);
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);

  // Resolve the connection.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  AuthenticationFactorsResult registration_state_result;
  registration_state_result.state =
      AuthenticationFactorsResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  std::move(cached_connection_cb()).Run(std::move(registration_state_result));
  model_observer()->WaitForStep();
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       GpmEnclaveNeedsReauthOnGoogleCom) {
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("accounts.google.com", "/title1.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents,
                              kGetAssertionUvDiscouragedWithGoogleRp);
  delegate_observer()->WaitForUI();

  ASSERT_EQ(browser()->tab_strip_model()->GetTabCount(), 1);
  // The sign in button is not visible.
  const auto sign_in_again_mech =
      std::ranges::find_if(dialog_model()->mechanisms, [](const auto& m) {
        return std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::SignInAgain>(m.type);
      });
  ASSERT_EQ(sign_in_again_mech, dialog_model()->mechanisms.end());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       UserResetsSecurityDomain) {
  EnableUVKeySupport();
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  AuthenticationFactorsResult registration_state_result;
  registration_state_result.state = AuthenticationFactorsResult::State::kEmpty;
  registration_state_result.key_version = kSecretVersion + 1;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, BiometricsInPWA) {
  // When requesting biometrics in a PWA, Touch ID should never be used.
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

  SetTrustedVaultRecoverable(kSecretVersion,
                             web_contents->GetPrimaryMainFrame());
  AddTestPasskeyToModel();
  EnableUVKeySupport();
  SetBiometricsEnabled(true);

  // Trigger a get() call to initialize the enclave. UV will be satisfied by
  // entering the PIN.
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  SimulateTrustedVaultKeyRetrieval();
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

// Without a Windows-on-ARM device we've been unable to debug why these
// tests fail in that that context.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NoGpmForCrossPlatformAttachment \
  DISABLED_NoGpmForCrossPlatformAttachment
#else
#define MAYBE_NoGpmForCrossPlatformAttachment NoGpmForCrossPlatformAttachment
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MAYBE_NoGpmForCrossPlatformAttachment) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialCrossPlatform);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      std::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_FALSE(request_delegate()->enclave_controller_for_testing());
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NoGpmCreationIfPasswordManagerDisabled \
  DISABLED_NoGpmCreationIfPasswordManagerDisabled
#else
#define MAYBE_NoGpmCreationIfPasswordManagerDisabled \
  NoGpmCreationIfPasswordManagerDisabled
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
      std::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_FALSE(request_delegate()->enclave_controller_for_testing());
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_NoGpmCreationIfPasswordManagerPasskeysDisabled \
  DISABLED_NoGpmCreationIfPasswordManagerPasskeysDisabled
#else
#define MAYBE_NoGpmCreationIfPasswordManagerPasskeysDisabled \
  NoGpmCreationIfPasswordManagerPasskeysDisabled
#endif
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MAYBE_NoGpmCreationIfPasswordManagerPasskeysDisabled) {
  EnableUVKeySupport();
  CheckRegistrationStateNotRequested();

  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnablePasskeys, false);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_TRUE(
      std::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
        return std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
      }));
  EXPECT_FALSE(request_delegate()->enclave_controller_for_testing());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, EnrollAndCreate) {
  EnableUVKeySupport();
  SetTrustedVaultRecoverable();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  model_observer()->WaitForStep();
  EXPECT_TRUE(
      request_delegate()->enclave_controller_for_testing()->is_active());
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  SimulateTrustedVaultKeyRetrieval();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       GetAssertionWithPlatformUV) {
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();
  EnableUVKeySupport();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  SimulateTrustedVaultKeyRetrieval();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

// Tests hiding a passkey using the Signal API, then restoring it.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       SignalApiHideAndRestorePasskey) {
  AddTestPasskeyToModel();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);

  // Make a request and expect to see the credential listed as a mechanism.
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();
  std::vector<uint8_t> user_id =
      std::get<AuthenticatorRequestDialogModel::Mechanism::Credential>(
          dialog_model()
              ->mechanisms.at(*dialog_model()->priority_mechanism_index)
              .type)
          ->user_id;
  EXPECT_EQ(base::span(user_id), TestProtobufUserId());
  dialog_model()->CancelAuthenticatorRequest();
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));

  // Hide the passkey.
  content::ExecuteScriptAsync(web_contents, kSignalHideTestPasskey);
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ("\"webauthn: OK\"", script_result);

  // The credential should not be offered in the next request.
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();
  EXPECT_TRUE(
      std::ranges::none_of(dialog_model()->mechanisms, [](const auto& mech) {
        return std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Credential>(mech.type);
      }));
  dialog_model()->CancelAuthenticatorRequest();
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));

  // Restore the passkey.
  content::ExecuteScriptAsync(web_contents, kSignalRestoreTestPasskey);
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ("\"webauthn: OK\"", script_result);

  // Make a request and expect to see the credential listed again.
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();
  user_id = std::get<AuthenticatorRequestDialogModel::Mechanism::Credential>(
                dialog_model()
                    ->mechanisms.at(*dialog_model()->priority_mechanism_index)
                    .type)
                ->user_id;
  EXPECT_EQ(base::span(user_id), TestProtobufUserId());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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
        base::ReplaceStringPlaceholders(kMakeCredentialGoogle, {kSyncEmail},
                                        /*offsets=*/nullptr));
    delegate_observer()->WaitForUI();
    model_observer_->WaitForStart();
    EXPECT_TRUE(
        std::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
          return std::holds_alternative<
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
                                                      {kSyncEmailLocalPartOnly},
                                                      /*offsets=*/nullptr));
    delegate_observer()->WaitForUI();
    model_observer_->WaitForStart();
    EXPECT_TRUE(
        std::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
          return std::holds_alternative<
              AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
        }));
    dialog_model()->CancelAuthenticatorRequest();
    std::string script_result;
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
    delegate_observer()->WaitForDelegateDestruction();
  }

  // But trying to create a passkey for a different account is fine.
  {
    SetTrustedVaultRecoverable();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(
        web_contents,
        base::ReplaceStringPlaceholders(
            kMakeCredentialGoogle, {std::string(kSyncEmail) + "_different"},
            /*offsets=*/nullptr));
    delegate_observer()->WaitForUI();
    model_observer_->WaitForStart();
    EXPECT_FALSE(
        std::ranges::none_of(dialog_model()->mechanisms, [](const auto& m) {
          return std::holds_alternative<
              AuthenticatorRequestDialogModel::Mechanism::Enclave>(m.type);
        }));
    dialog_model()->CancelAuthenticatorRequest();
    std::string script_result;
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
    delegate_observer()->WaitForDelegateDestruction();
  }
}

// Tests that an allow list filters the available GPM credentials.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       GetAssertionWithAllowList) {
  const std::vector<uint8_t> kCredId1 = {1, 2,  3,  4,  5,  6,  7,  8,
                                         9, 10, 11, 12, 13, 14, 15, 16};
  constexpr char kUserName1[] = "ruby";
  const std::vector<uint8_t> kCredId2 = {16, 15, 14, 13, 12, 11, 10, 9,
                                         8,  7,  6,  5,  4,  3,  2,  1};
  constexpr char kUserName2[] = "yang";

  SetTrustedVaultRecoverable();

  sync_pb::WebauthnCredentialSpecifics passkey1;
  CHECK(passkey1.ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
  passkey1.set_sync_id(kCredId1.data(), kCredId1.size());
  passkey1.set_credential_id(kCredId1.data(), kCredId1.size());
  passkey1.set_user_id(kCredId1.data(), kCredId1.size());
  passkey1.set_user_name(kUserName1);
  passkey1.set_user_display_name(kUserName1);
  passkey_model().AddNewPasskeyForTesting(passkey1);

  sync_pb::WebauthnCredentialSpecifics passkey2;
  CHECK(passkey2.ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
  passkey1.set_sync_id(kCredId2.data(), kCredId2.size());
  passkey2.set_credential_id(kCredId2.data(), kCredId2.size());
  passkey2.set_user_id(kCredId2.data(), kCredId2.size());
  passkey2.set_user_name(kUserName2);
  passkey1.set_user_display_name(kUserName2);
  passkey_model().AddNewPasskeyForTesting(passkey2);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionCredId1);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();

  // Only the first passkey should be included in the request.
  std::optional<int> found;
  for (size_t i = 0; i < dialog_model()->mechanisms.size(); ++i) {
    if (IsMechanismEnclaveCredential(dialog_model()->mechanisms[i])) {
      ASSERT_EQ(
          std::get<AuthenticatorRequestDialogModel::Mechanism::Credential>(
              dialog_model()->mechanisms[i].type)
              ->user_id,
          kCredId1);
      found = i;
    }
  }
  ASSERT_TRUE(found);
  EXPECT_EQ(found, dialog_model()->priority_mechanism_index);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       IncognitoModeMakeCredential) {
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      https_server_.GetURL("www.example.com", "/title1.html"));
  SetTrustedVaultRecoverable(kSecretVersion, otr_browser->tab_strip_model()
                                                 ->GetActiveWebContents()
                                                 ->GetPrimaryMainFrame());

  // Initially bootstrap from LSKF, ensuring the incognito warning is shown.
  content::WebContents* web_contents =
      otr_browser->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMConfirmOffTheRecordCreate);
  model_observer()->WaitForStep();
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
  SimulateTrustedVaultKeyRetrieval();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
  delegate_observer()->WaitForDelegateDestruction();

  // Ensure the incognito warning is also shown in the non-bootstrapping flow.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMConfirmOffTheRecordCreate);
  model_observer()->WaitForStep();
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

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       IncognitoModeGetAssertion) {
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      https_server_.GetURL("www.example.com", "/title1.html"));
  SetTrustedVaultRecoverable(kSecretVersion, otr_browser->tab_strip_model()
                                                 ->GetActiveWebContents()
                                                 ->GetPrimaryMainFrame());
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      otr_browser->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  SimulateTrustedVaultKeyRetrieval();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

#if BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       BiometricsDisabledDuringRequest) {
  // If Touch ID is disabled during the course of a request, the UV disposition
  // shouldn't also change. I.e. if we started with the expectation of doing
  // UV=true, the UI expects that to continue, even if we need macOS to prompt
  // for the system password.
  SetTrustedVaultRecoverable();
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
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  SimulateTrustedVaultKeyRetrieval();

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

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

constexpr char kICloudKeychainRecoveryKeyAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING ".com.google.common.folsom";

class EnclaveICloudRecoveryKeyTest : public EnclaveAuthenticatorBrowserTest {
 protected:
  crypto::apple::ScopedFakeKeychainV2 scoped_fake_keychain_{
      kICloudKeychainRecoveryKeyAccessGroup};
};

// Tests enrolling an iCloud recovery key when there are no keys already
// enrolled with the recovery service or present in iCloud keychain.
IN_PROC_BROWSER_TEST_F(EnclaveICloudRecoveryKeyTest, Enroll) {
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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
      std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>>
      future;
  trusted_vault::ICloudRecoveryKey::Retrieve(
      future.GetCallback(), trusted_vault::SecurityDomainId::kPasskeys,
      kICloudKeychainRecoveryKeyAccessGroup);
  EXPECT_TRUE(future.Wait());
  std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>> recovery_keys =
      future.Take();
  ASSERT_EQ(recovery_keys.size(), 1u);
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> icloud_key =
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
  base::test::TestFuture<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
      future;
  trusted_vault::ICloudRecoveryKey::Create(
      future.GetCallback(), trusted_vault::SecurityDomainId::kPasskeys,
      kICloudKeychainRecoveryKeyAccessGroup);
  EXPECT_TRUE(future.Wait());
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> existing_icloud_key =
      future.Take();
  ASSERT_TRUE(existing_icloud_key);

  // Do a make credential request and enroll a PIN.
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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
      std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>>
      list_future;
  trusted_vault::ICloudRecoveryKey::Retrieve(
      list_future.GetCallback(), trusted_vault::SecurityDomainId::kPasskeys,
      kICloudKeychainRecoveryKeyAccessGroup);
  EXPECT_TRUE(list_future.Wait());
  std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>> recovery_keys =
      list_future.Take();
  EXPECT_EQ(recovery_keys.size(), 2u);
}

// TODO(crbug.com/368799197): The test is flaky.
// Tests enrolling an iCloud recovery key, then recovering from it.
IN_PROC_BROWSER_TEST_F(EnclaveICloudRecoveryKeyTest, DISABLED_Recovery) {
  {
    // Do a make credential request and enroll a PIN.
    SetTrustedVaultEmpty();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
    delegate_observer()->WaitForUI();

    model_observer()->SetStepToObserve(
        AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
    model_observer()->WaitForStep();
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
        std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>>
        future;
    trusted_vault::ICloudRecoveryKey::Retrieve(
        future.GetCallback(), trusted_vault::SecurityDomainId::kPasskeys,
        kICloudKeychainRecoveryKeyAccessGroup);
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(future.Get().size(), 1u);
  }

  // Unenroll the current device from the enclave.
  enclave_manager().ClearRegistrationForTesting();
  enclave_manager().ResetForTesting();

  // Do a make credential request and recover with the iCloud key.
  {
    // Set up the mock trusted vault connection to download the iCloud recovery
    // factor that should have been added.
    AuthenticationFactorsResult registration_state_result;
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
        pin_member->public_key(),
        trusted_vault::UsableRecoveryPinMetadata(
            pin_metadata.encrypted_pin_hash(),
            base::Time::FromSecondsSinceUnixEpoch(
                pin_metadata.expiration_time().seconds())));
    registration_state_result.state =
        AuthenticationFactorsResult::State::kRecoverable;
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

    model_observer()->SetStepToObserve(
        AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
    model_observer()->WaitForStep();
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
        std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>>
        future;
    trusted_vault::ICloudRecoveryKey::Retrieve(
        future.GetCallback(), trusted_vault::SecurityDomainId::kPasskeys,
        kICloudKeychainRecoveryKeyAccessGroup);
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(future.Get().size(), 1u);
  }
}

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC)
#define MAYBE_MakeCredentialDeclineGPMThenAccept \
  DISABLED_MakeCredentialDeclineGPMThenAccept
#else
#define MAYBE_MakeCredentialDeclineGPMThenAccept \
  MakeCredentialDeclineGPMThenAccept
#endif
// TODO(crbug.com/345308672): Failing on various Mac bots.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MAYBE_MakeCredentialDeclineGPMThenAccept) {
  SetTrustedVaultEmpty();
  delegate_observer()->AddAdditionalTransport(
      device::FidoTransportProtocol::kInternal);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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
  // priority mechanism for an attachment=platform request. Decline it.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialAttachmentPlatform);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  dialog_model()->StartOver();
  model_observer()->WaitForStep();
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();
  EXPECT_EQ(
      browser()->profile()->GetPrefs()->GetInteger(
          webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount),
      1);

  // Decline a second time.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialAttachmentPlatform);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  dialog_model()->StartOver();
  model_observer()->WaitForStep();
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();
  EXPECT_EQ(
      browser()->profile()->GetPrefs()->GetInteger(
          webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount),
      2);

  // After backing out of GPM twice, the next attempt should default to
  // either mechanism selection or, on Mac, the custom platform authenticator
  // passkey creation dialog.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialAttachmentPlatform);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  model_observer()->WaitForStep();

  // Now, select GPM from the list and complete the creation.
  dialog_model()->OnGPMSelected();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kReady);
  dialog_model()->OnGPMCreatePasskey();
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  delegate_observer()->WaitForDelegateDestruction();

  // The decline count should be reset.
  EXPECT_EQ(
      browser()->profile()->GetPrefs()->GetInteger(
          webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount),
      0);

  // The next request should have GPM as the priority again.
  content::ExecuteScriptAsync(web_contents, kMakeCredentialAttachmentPlatform);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();
}

class EnclaveAuthenticatorIncognitoBrowserTest
    : public EnclaveAuthenticatorBrowserTest,
      public testing::WithParamInterface<bool> {};

// Attempt a GetAssertion multiple times with GPM passkey bootstrapping
// offered, and decline each time. The default should change away from GPM after
// two times declined.
IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorIncognitoBrowserTest,
                       MultipleDeclinedBootstrappings) {
  content::WebContents* web_contents;
  if (GetParam()) {
    Browser* otr_browser = OpenURLOffTheRecord(
        browser()->profile(),
        https_server_.GetURL("www.example.com", "/title1.html"));
    web_contents = otr_browser->tab_strip_model()->GetActiveWebContents();
  } else {
    web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  }
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  EnableUVKeySupport();
  delegate_observer()->SetUseSyncedDeviceCablePairing(/*use_pairing=*/true);

  SetTrustedVaultRecoverable(kSecretVersion, rfh);
  AddTestPasskeyToModel();

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  // Enclave will be the priority mechanism. Select it.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();

  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  // Now cancel the request...
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  SetTrustedVaultRecoverable(kSecretVersion, rfh);
  dialog_model()->StartOver();
  model_observer()->WaitForStep();

  // ...and select it again from the mechanism list.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  EXPECT_TRUE(std::ranges::any_of(
      dialog_model()->mechanisms,
      [](const auto& m) { return IsMechanismEnclaveCredential(m); }));
  for (auto& mechanism : request_delegate_->dialog_model()->mechanisms) {
    if (IsMechanismEnclaveCredential(mechanism)) {
      mechanism.callback.Run();
      break;
    }
  }
  model_observer()->WaitForStep();

  // Cancel the request once more.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  dialog_model()->StartOver();
  model_observer()->WaitForStep();

  // Terminate the request and send a new one so newly-enumerated credentials
  // will be used.
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();

  SetTrustedVaultRecoverable(kSecretVersion, rfh);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  // Passkeys from GPM should still be present, but they should not be the
  // default.
  model_observer_->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  model_observer_->WaitForStep();
  EXPECT_TRUE(std::ranges::any_of(
      dialog_model()->mechanisms,
      [](const auto& m) { return IsMechanismEnclaveCredential(m); }));

  // Finally, if the user manually chooses the enclave, it should be the default
  // again. Attempting to bootstrap should be enough.
  dialog_model()->OnGPMSelected();
  if (GetParam()) {
    EXPECT_EQ(
        dialog_model()->step(),
        AuthenticatorRequestDialogModel::Step::kGPMConfirmOffTheRecordCreate);
    dialog_model()->OnGPMConfirmOffTheRecordCreate();
  }
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
  dialog_model()->OnTrustThisComputer();

  // Terminate the request and send a new one. The enclave should once again be
  // the default.
  dialog_model()->CancelAuthenticatorRequest();
  delegate_observer()->WaitForDelegateDestruction();

  SetTrustedVaultRecoverable(kSecretVersion, rfh);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
}

INSTANTIATE_TEST_SUITE_P(Incognito,
                         EnclaveAuthenticatorIncognitoBrowserTest,
                         testing::Values(false, true));

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
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

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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
        GetTempDirPath(),
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

    {
      auto store_keys_lock = second_manager.GetStoreKeysLock();
      second_manager.StoreKeys(kSyncGaiaId, {*security_domain_secret},
                               /*last_key_version=*/kSecretVersion);
    }

    base::test::TestFuture<bool> add_future;
    second_manager.AddDeviceToAccount(std::nullopt, add_future.GetCallback());
    EXPECT_TRUE(add_future.Wait());
    EXPECT_TRUE(add_future.Get());

    base::test::TestFuture<bool> change_future;
    second_manager.ChangePIN(newpin, "rapt", change_future.GetCallback());
    EXPECT_TRUE(change_future.Wait());
    ASSERT_TRUE(change_future.Get());

    // Verify the PIN claim key was redacted.
    EXPECT_THAT(GetDeviceLog(),
                testing::HasSubstr("\"pin_claim_key\": \"[redacted]\""));
  }

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
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

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, IsUVPAA) {
  // With the enclave authenticator in place, IsUVPAA should return true.
  EXPECT_TRUE(IsUVPAA());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, IsUVPAA_GoogleSite) {
  // With the enclave authenticator in place, IsUVPAA should return false for
  // google.com sites because we won't create a credential for an account in
  // that same account. But since we don't know the user.id value at IsUVPAA
  // time, the result has to be conservative.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("accounts.google.com", "/title1.html")));
  EXPECT_FALSE(IsUVPAA());
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       IsUVPAA_NoUnexportableKeys) {
  // Without support for unexportable keys, IsUVPAA should return false because
  // the enclave cannot be used.
  fake_hw_provider_.reset();
  WebAuthnScopedNullUnexportableKeyProvider no_hw_key_support;
  EXPECT_FALSE(IsUVPAA());
}

#endif  // IS_LINUX

// Verify that GPM will do UV on a uv=preferred request if and only if
// biometrics are available.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       UserVerificationPolicy) {
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();
  EnableUVKeySupport();

  // The first get() request is satisfied implicitly because recovery was done.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  SimulateTrustedVaultKeyRetrieval();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  SetBiometricsEnabled(false);

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  dialog_model()->OnUserConfirmedPriorityMechanism();

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=false\"");

  // On Linux biometrics is not available so the test is done.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  SetBiometricsEnabled(true);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();
#if BUILDFLAG(IS_MAC)
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMTouchID);
  model_observer()->WaitForStep();
  dialog_model()->OnTouchIDComplete(true);
#else
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  dialog_model()->OnUserConfirmedPriorityMechanism();
#endif

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
#endif
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, Bug_354083161) {
  // Reproduces the crash from b/354083161

  // Do an assertion to set up the enclave.
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  SimulateTrustedVaultKeyRetrieval();
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

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  dialog_model()->OnUserConfirmedPriorityMechanism();

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, NoSilentOperations) {
  // Check that the enclave doesn't allow silent operations.

  // Do an assertion to set up the enclave.
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  SimulateTrustedVaultKeyRetrieval();
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
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();
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

  crypto::StatefulUnexportableKeyProvider* AsStatefulUnexportableKeyProvider()
      override {
    NOTREACHED();
  }
};

std::unique_ptr<crypto::UnexportableKeyProvider>
BlockingUnexportableKeyProviderFactory() {
  return std::make_unique<BlockingUnexportableKeyProvider>();
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, CancelRacesTPMCheck) {
  // https://crbug.com/352532554

  // Set the UnexportableKeyProvider to one that will block inside
  // `SelectAlgorithm` so that we can simulate a slow TPM check.
  fake_hw_provider_.reset();
  SetWebAuthnUnexportableKeyProviderForTesting(
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

// Regression test for crbug.com/399937685.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest, SelectDeletedPasskey) {
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();

  // Set up a conditional UI request.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionConditionalUI);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kPasskeyAutofill);
  model_observer()->WaitForStep();

  // Delete the credential during the request. Websites could do this through
  // the signal API, and users through the password manager.
  passkey_model().DeleteAllPasskeys();

  // Go through all the steps to get the enclave set up.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  dialog_model()->OnAccountPreselectedIndex(0);
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  SimulateTrustedVaultKeyRetrieval();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMError);
  dialog_model()->OnGPMPinEntered(u"123456");
  model_observer()->WaitForStep();
}

#if BUILDFLAG(IS_WIN)
// UV key creation deferral only happens on Windows.
// See https://crbug.com/416664004.
IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       SimultaneousRequestsWithDeferredUVKey) {
  EnableUVKeySupport(true);
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
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

  // The EnclaveManager should be in a state where UV key creation is pending.
  ASSERT_TRUE(enclave_manager()
                  .local_state_for_testing()
                  .mutable_users()
                  ->begin()
                  ->second.deferred_uv_key_creation());

  content::ExecuteScriptAsync(web_contents, kGetAssertionUvRequired);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  model_observer()->WaitForStep();

  // Wrap the enclave request invocation callback so that it can be delayed.
  base::test::TestFuture<std::unique_ptr<device::enclave::CredentialRequest>>
      enclave_request_future;
  auto original_enclave_request_callback =
      request_delegate()
          ->enclave_controller_for_testing()
          ->enclave_request_callback_for_testing();
  request_delegate()
      ->enclave_controller_for_testing()
      ->enclave_request_callback_for_testing() = base::BindRepeating(
      [](base::RepeatingCallback<void(
             std::unique_ptr<device::enclave::CredentialRequest>)>
             future_callback,
         std::unique_ptr<device::enclave::CredentialRequest> request) {
        future_callback.Run(std::move(request));
      },
      enclave_request_future.GetRepeatingCallback());

  dialog_model()->OnUserConfirmedPriorityMechanism();

  EXPECT_TRUE(enclave_request_future.Wait());

  // A second WebContents attempts a transaction while the first is pending.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_.GetURL("www.example.com", "/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::DOMMessageQueue second_message_queue(second_web_contents);
  content::ExecuteScriptAsync(second_web_contents, kGetAssertionUvRequired);

  // NB: We no longer have access to the original request_delegate() or
  // dialog_model().
  delegate_observer()->WaitForUI();
  model_observer_->WaitForStart();
  dialog_model()->OnUserConfirmedPriorityMechanism();

  // Resume the first request.
  original_enclave_request_callback.Run(enclave_request_future.Take());

  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");

  ASSERT_TRUE(second_message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}
#endif  // BUILDFLAG(IS_WIN)

class EnclaveAuthenticatorConditionalCreateBrowserTest
    : public EnclaveAuthenticatorBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  EnclaveAuthenticatorConditionalCreateBrowserTest() {
    sync_feature_enabled_ = GetParam();

    scoped_feature_list_.InitAndEnableFeature(device::kWebAuthnPasskeyUpgrade);
    CHECK(base::FeatureList::IsEnabled(device::kWebAuthnPasskeyUpgrade));
  }

  bool use_account_password_store() { return !sync_feature_enabled_; }

  password_manager::PasswordStoreInterface* password_store() {
    return use_account_password_store()
               ? AccountPasswordStoreFactory::GetForProfile(
                     browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                     .get()
               : ProfilePasswordStoreFactory::GetForProfile(
                     browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                     .get();
  }

  // Creates a credential to ensure the enclave authenticator is in a usable
  // state prior to making a conditional create request.
  void BootstrapEnclave() {
    SetTrustedVaultEmpty();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::DOMMessageQueue message_queue(web_contents);
    content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
    delegate_observer()->WaitForUI();

    model_observer()->SetStepToObserve(
        AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
    model_observer()->WaitForStep();
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

  void InjectPassword(base::Time last_used) {
    password_manager::PasswordForm saved_form;
    saved_form.signon_realm = https_server_.GetURL("example.com", "/").spec();
    saved_form.url = https_server_.GetURL("example.com",
                                          "/password/prefilled_username.html");
    saved_form.username_value = base::UTF8ToUTF16(std::string(kSyncEmail));
    saved_form.password_value = u"hunter1";
    saved_form.date_last_used = last_used;
    password_store()->AddLogin(saved_form);
  }

  sync_pb::WebauthnCredentialSpecifics InjectPasskey() {
    sync_pb::WebauthnCredentialSpecifics passkey;
    CHECK(passkey.ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
    // Sync ID and credential ID must be 16 bytes long.
    passkey.set_sync_id(base::RandBytesAsString(16));
    passkey.set_credential_id(base::RandBytesAsString(16));
    passkey.set_user_id(base::RandBytesAsString(16));
    passkey.set_user_name(kSyncEmail);
    passkey.set_user_display_name(kSyncEmail);
    passkey_model().AddNewPasskeyForTesting(passkey);
    return passkey;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(WithSyncFeatureEnabled,
                         EnclaveAuthenticatorConditionalCreateBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorConditionalCreateBrowserTest,
                       ConditionalCreate) {
  // Set up the enclave and inject a password with a recent last use timestamp.
  // A conditional create request for the same username should succeed.
  BootstrapEnclave();
  InjectPassword(base::Time::Now());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialConditionalCreate);
  delegate_observer()->WaitForUI();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  std::optional<std::vector<uint8_t>> cred_id =
      ParseCredentialId(script_result);
  EXPECT_TRUE(cred_id);

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result",
      /*sample=*/PasskeyUpgradeResult::kSuccess,
      /*expected_bucket_count=*/1);

  // The request should not have instantiated non-enclave discoveries.
  ASSERT_TRUE(
      delegate_observer()->on_transport_availability_enumerated_called());
  EXPECT_TRUE(delegate_observer()->transports_observed()->empty());
}

IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorConditionalCreateBrowserTest,
                       ConditionalCreate_EnclaveNotLoaded) {
  // Set up the enclave and then reset it to force it to be loaded from disk at
  // the beginning of the subsequent conditional create request.
  BootstrapEnclave();
  enclave_manager().ResetForTesting();
  InjectPassword(base::Time::Now());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialConditionalCreate);
  delegate_observer()->WaitForUI();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  std::optional<std::vector<uint8_t>> cred_id =
      ParseCredentialId(script_result);
  EXPECT_TRUE(cred_id);

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result",
      /*sample=*/PasskeyUpgradeResult::kSuccess,
      /*expected_bucket_count=*/1);

  // The request should not have instantiated non-enclave discoveries.
  ASSERT_TRUE(
      delegate_observer()->on_transport_availability_enumerated_called());
  EXPECT_TRUE(delegate_observer()->transports_observed()->empty());
}

IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorConditionalCreateBrowserTest,
                       ConditionalCreate_FailsWithSettingDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kAutomaticPasskeyUpgrades, false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  BootstrapEnclave();
  InjectPassword(base::Time::Now());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialConditionalCreate);
  delegate_observer()->WaitForUI();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("NotAllowedError"));

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result",
      /*sample=*/PasskeyUpgradeResult::kOptOut,
      /*expected_bucket_count=*/1);
}

// Regression test for crbug.com/414750307.
IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorConditionalCreateBrowserTest,
                       ConditionalCreate_FailsWithGPMDisabledByPolicy) {
  // Disabling GPM via policy should cause upgrade requests to fail.
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  InjectPassword(base::Time::Now());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialConditionalCreate);
  delegate_observer()->WaitForUI();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("NotAllowedError"));

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result",
      /*sample=*/PasskeyUpgradeResult::kGpmDisabled,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorConditionalCreateBrowserTest,
                       ConditionalCreate_FailsWithoutBootstrappedEnclave) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  InjectPassword(base::Time::Now());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialConditionalCreate);
  delegate_observer()->WaitForUI();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("NotAllowedError"));

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result",
      /*sample=*/PasskeyUpgradeResult::kEnclaveNotInitialized,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorConditionalCreateBrowserTest,
                       ConditionalCreate_FailsWithoutMatchingPassword) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  BootstrapEnclave();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialConditionalCreate);
  delegate_observer()->WaitForUI();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("NotAllowedError"));

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result",
      /*sample=*/PasskeyUpgradeResult::kNoMatchingPassword,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(EnclaveAuthenticatorConditionalCreateBrowserTest,
                       ConditionalCreate_ExcludeListMatch) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  BootstrapEnclave();
  sync_pb::WebauthnCredentialSpecifics passkey = InjectPasskey();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  std::string script = base::ReplaceStringPlaceholders(
      kMakeCredentialConditionalCreateWithExcludeList,
      {base::Base64Encode(passkey.credential_id())},
      /*offsets=*/nullptr);
  content::ExecuteScriptAsync(web_contents, script);
  delegate_observer()->WaitForUI();

  // It shouldn't be possible to test the matches on an exclude list without
  // also having an upgrade eligible password for the request. I.e., without an
  // upgrade-eligible password, this create() request results in the generic
  // NotAllowedError, rather than the exclude-list specific InvalidStateError.
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("NotAllowedError"));

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result",
      /*sample=*/PasskeyUpgradeResult::kNoMatchingPassword,
      /*expected_bucket_count=*/1);

  // With an upgrade eligible password, we signal the exclude list match with an
  // InvalidStateError.
  InjectPassword(base::Time::Now());
  content::ExecuteScriptAsync(web_contents, script);
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("InvalidStateError"));

  // PasskeyUpgradeRequestController does not directly observe the authenticator
  // request result, such as the authenticator error that indicates the exclude
  // list match. Hence, no additional histogram value should have been emitted
  // for the second request.
  histogram_tester_.ExpectTotalCount(
      "WebAuthentication.AutomaticPasskeyUpgrade.Result", 1);
}

class EnclaveAuthenticatorImmediateMediationBrowserTest
    : public EnclaveAuthenticatorBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnImmediateGet};
};

#if BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(
    EnclaveAuthenticatorImmediateMediationBrowserTest,
    GivenOnlyOneGpmPasskeyWithBiometricsEnabled_WhenImmediateRequestWithUv_TouchIdShown) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);

  // GPM setup with one passkey
  SetTrustedVaultRecoverable();
  AddTestPasskeyToModel();
  EnableUVKeySupport();
  SetBiometricsEnabled(true);

  // The first get() request is satisfied implicitly because recovery was done.
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvPreferred);
  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
  model_observer()->WaitForStep();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();
  SimulateTrustedVaultKeyRetrieval();
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");

  // Setup page for immediate request:
  ASSERT_TRUE(content::ExecJs(web_contents,
                              kGetAssertionViaButtonClickImmediateUvPreferred));

  // Simulate button click to trigger the navigator.credentials.get() call
  content::ExecuteScriptAsync(web_contents,
                              "document.getElementById('testButton').click();");

  // Wait for TAI to be processed. This ensures SetUIPresentation has been
  // called with kModalImmediate.
  delegate_observer()->WaitForPreTransportAvailabilityEnumerated();
  // Wait for the UI to be shown. For kModalImmediate, this means a specific
  // sheet (like bootstrapping or Touch ID) is shown.
  delegate_observer()->WaitForUI();

  // Simulate successful recovery/enrollment by storing keys.
  // This should lead to OnEnclaveAccountSetUpComplete, which then picks
  // kUVKeyWithChromeUI and sets step to kGPMTouchID.
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMTouchID);
  model_observer()->WaitForStep();

  // At this point, step should be kGPMTouchID
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMTouchID);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Immediate.EnclaveReady",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorImmediateMediationBrowserTest,
                       ImmediateRequest_EnclaveNotReady_NoPasskeys) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);

  // GPM setup with one passkey
  AddTestPasskeyToModel();

  // 2. Setup page for immediate request and click button.
  ASSERT_TRUE(content::ExecJs(web_contents,
                              kGetAssertionViaButtonClickImmediateUvPreferred));
  content::ExecuteScriptAsync(web_contents,
                              "document.getElementById('testButton').click();");

  // 3. Wait for the request to complete.
  // It's expected to fail as no credentials should be found.
  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_THAT(script_result, testing::HasSubstr("NotAllowedError"));
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Immediate.EnclaveReady",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MakeCredential_LargeBlobSupported) {
  // New empty vault.
  SetTrustedVaultEmpty();

  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start JS create() call.
  content::DOMMessageQueue queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialLargeBlob);

  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  // Collect the JS result and verify the extension bit.
  std::string script_result;
  ASSERT_TRUE(queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"largeblob true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       GetAssertion_LargeBlobWriteThenRead) {
  // New empty vault.
  SetTrustedVaultEmpty();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialLargeBlob);

  delegate_observer()->WaitForUI();
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
  model_observer()->WaitForStep();
  dialog_model()->OnGPMCreatePasskey();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"largeblob true\"");

  const auto passkeys = passkey_model().GetAllPasskeys();
  ASSERT_EQ(passkeys.size(), 1u);
  const std::string cred_id_b64 =
      base::Base64Encode(passkeys[0].credential_id());

  auto run_get_and_confirm = [&](const std::string& js) {
    content::DOMMessageQueue q(web_contents);
    content::ExecuteScriptAsync(web_contents, js);

    // Wait for Chrome UI.
    delegate_observer()->WaitForUI();

    model_observer()->SetStepToObserve(
        AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
    model_observer()->WaitForStep();
    dialog_model()->OnUserConfirmedPriorityMechanism();

    // Collect the JS result.
    std::string r;
    CHECK(q.WaitForMessage(&r));
    return r;
  };

  // Write the blob.
  std::string write_js = base::ReplaceStringPlaceholders(
      kGetAssertionWriteLargeBlob, {cred_id_b64}, nullptr);
  EXPECT_EQ(run_get_and_confirm(write_js), "\"write ok\"");
  histogram_tester_.ExpectBucketCount(
      "WebAuthentication.GPM.GetAssertion.LargeBlobSucceeded.Write",
      /*sample=*/true, /*expected_count=*/1);

  // Read it back and verify contents.
  std::string read_js = base::ReplaceStringPlaceholders(
      kGetAssertionReadLargeBlob, {cred_id_b64}, nullptr);
  EXPECT_EQ(run_get_and_confirm(read_js), "\"read hello world\"");
  histogram_tester_.ExpectBucketCount(
      "WebAuthentication.GPM.GetAssertion.LargeBlobSucceeded.Read",
      /*sample=*/true, /*expected_count=*/1);

  // Ensure the large blob is redacted from logs.
  EXPECT_THAT(GetDeviceLog(),
              testing::HasSubstr("\"largeBlob\": \"[redacted]\""));
}

// Disable large blob for GPM feature flag.
class EnclaveLargeBlobFlagOffTest : public EnclaveAuthenticatorBrowserTest {
 public:
  EnclaveLargeBlobFlagOffTest() {
    scoped_feature_list_.InitAndDisableFeature(
        device::kWebAuthnLargeBlobForGPM);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(EnclaveLargeBlobFlagOffTest,
                       LargeBlobExtensionNotOfferedWhenFlagDisabled) {
  SetTrustedVaultEmpty();
  content::WebContents* wc =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue q(wc);
  content::ExecuteScriptAsync(wc, kMakeCredentialLargeBlob);

  delegate_observer()->WaitForUI();
  model_observer_->WaitForStart();
  dialog_model()->OnGPMCreatePasskey();
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string result;
  ASSERT_TRUE(q.WaitForMessage(&result));
  EXPECT_EQ(result, "\"largeblob false\"");
}

}  // namespace

#endif  // !defined(MEMORY_SANITIZER)
