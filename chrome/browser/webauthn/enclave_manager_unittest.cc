// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/fake_magic_arch.h"
#include "chrome/browser/webauthn/fake_recovery_key_store.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/browser/webauthn/unexportable_key_utils.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/enclave/types.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"
#include "device/fido/json_request.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/enclave/icloud_recovery_key_mac.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "base/run_loop.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "crypto/signature_verifier.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// These tests are also disabled under MSAN. The enclave subprocess is written
// in Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace enclave = device::enclave;
using NoArgFuture = base::test::TestFuture<void>;
using BoolFuture = base::test::TestFuture<bool>;

namespace {

constexpr int32_t kSecretVersion = 417;

constexpr std::array<uint8_t, 32> kTestKey = {
    0xc4, 0xdf, 0xa4, 0xed, 0xfc, 0xf9, 0x7c, 0xc0, 0x3a, 0xb1, 0xcb,
    0x3c, 0x03, 0x02, 0x9b, 0x5a, 0x05, 0xec, 0x88, 0x48, 0x54, 0x42,
    0xf1, 0x20, 0xb4, 0x75, 0x01, 0xde, 0x61, 0xf1, 0x39, 0x5d,
};
constexpr uint8_t kTestProtobuf[] = {
    0x0a, 0x10, 0x71, 0xfd, 0xf9, 0x65, 0xa8, 0x7c, 0x61, 0xe2, 0xff, 0x27,
    0x0c, 0x76, 0x25, 0x23, 0xe0, 0xa4, 0x12, 0x10, 0x77, 0xf2, 0x3c, 0x31,
    0x3c, 0xe8, 0x94, 0x9a, 0x9f, 0xbc, 0xdf, 0x44, 0xfc, 0xf5, 0x41, 0x97,
    0x1a, 0x0b, 0x77, 0x65, 0x62, 0x61, 0x75, 0x74, 0x68, 0x6e, 0x2e, 0x69,
    0x6f, 0x22, 0x06, 0x56, 0x47, 0x56, 0x7a, 0x64, 0x41, 0x2a, 0x10, 0x60,
    0x07, 0x19, 0x5b, 0x4e, 0x19, 0xf9, 0x6e, 0xc1, 0xfc, 0xfd, 0x0a, 0xf6,
    0x0c, 0x00, 0x7e, 0x30, 0xf9, 0xa0, 0xea, 0xf3, 0xc8, 0x31, 0x3a, 0x04,
    0x54, 0x65, 0x73, 0x74, 0x42, 0x04, 0x54, 0x65, 0x73, 0x74, 0x4a, 0xa6,
    0x01, 0xdc, 0xc5, 0x16, 0x15, 0x91, 0x24, 0xd2, 0x31, 0xfc, 0x85, 0x8b,
    0xe2, 0xec, 0x22, 0x09, 0x8f, 0x8d, 0x0f, 0xbe, 0x9b, 0x59, 0x71, 0x04,
    0xcd, 0xaa, 0x3d, 0x32, 0x23, 0xbd, 0x25, 0x46, 0x14, 0x86, 0x9c, 0xfe,
    0x74, 0xc8, 0xd3, 0x37, 0x70, 0xed, 0xb0, 0x25, 0xd4, 0x1b, 0xdd, 0xa4,
    0x3c, 0x02, 0x13, 0x8c, 0x69, 0x03, 0xff, 0xd1, 0xb0, 0x72, 0x00, 0x29,
    0xcf, 0x5f, 0x06, 0xb3, 0x94, 0xe2, 0xea, 0xca, 0x68, 0xdd, 0x0b, 0x07,
    0x98, 0x7a, 0x2c, 0x8f, 0x08, 0xee, 0x7d, 0xad, 0x16, 0x35, 0xc7, 0x10,
    0xf3, 0xa4, 0x90, 0x84, 0xd1, 0x8e, 0x2e, 0xdb, 0xb9, 0xfa, 0x72, 0x9a,
    0xcf, 0x12, 0x1b, 0x3c, 0xca, 0xfa, 0x79, 0x4a, 0x1e, 0x1b, 0xe1, 0x15,
    0xdf, 0xab, 0xee, 0x75, 0xbb, 0x5c, 0x5a, 0x94, 0x14, 0xeb, 0x72, 0xae,
    0x37, 0x97, 0x03, 0xa8, 0xe7, 0x62, 0x9d, 0x2e, 0xfd, 0x28, 0xce, 0x03,
    0x34, 0x20, 0xa7, 0xa2, 0x7b, 0x00, 0xc8, 0x12, 0x62, 0x12, 0x7f, 0x54,
    0x73, 0x8c, 0x21, 0xc8, 0x85, 0x15, 0xce, 0x36, 0x14, 0xd9, 0x41, 0x22,
    0xe8, 0xbf, 0x88, 0xf9, 0x45, 0xe4, 0x1c, 0x89, 0x7d, 0xa4, 0x23, 0x58,
    0x00, 0x68, 0x98, 0xf5, 0x81, 0xef, 0xad, 0xf4, 0xda, 0x17, 0x70, 0xab,
    0x03,
};
constexpr std::string_view kTestPINPublicKey =
    "\x04\xe4\x72\x4c\x87\xf9\x42\xbe\x2a\xd1\xe6\xac\xa3\x52\x85\xea\x08\xf7"
    "\xe9\x6d\xea\xf2\xf0\x7f\xa9\xde\x89\xe2\x9e\x69\x36\xc4\x4c\xf9\x56\xe9"
    "\xa1\x1f\x08\xfe\x55\xca\x1b\x84\xb9\xe5\x1e\xc3\x26\x69\x16\xa0\x6b\x03"
    "\xfa\x42\x08\xa8\xaf\x7d\xd9\x14\xb4\xfc\x1a";

#if BUILDFLAG(IS_MAC)
base::span<const uint8_t> ToSpan(std::string_view s) {
  return base::as_bytes(base::make_span(s));
}
#endif  // BUILDFLAG(IS_MAC)

std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> GetTestEntity() {
  auto ret = std::make_unique<sync_pb::WebauthnCredentialSpecifics>();
  CHECK(ret->ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
  return ret;
}

std::string StringOfZeros(size_t len) {
  return std::string(len, '0');
}

enclave::SigningCallback AlwaysFailsSigningCallback() {
  return base::BindOnce(
      [](enclave::SignedMessage,
         base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
             callback) { std::move(callback).Run(std::nullopt); });
}

webauthn_pb::EnclaveLocalState::WrappedPIN GetTestWrappedPIN() {
  webauthn_pb::EnclaveLocalState::WrappedPIN wrapped_pin;
  wrapped_pin.set_wrapped_pin(StringOfZeros(30));
  wrapped_pin.set_claim_key(StringOfZeros(32));
  wrapped_pin.set_generation(0);
  wrapped_pin.set_form(wrapped_pin.FORM_SIX_DIGITS);
  wrapped_pin.set_hash(wrapped_pin.HASH_SCRYPT);
  wrapped_pin.set_hash_difficulty(1 << 12);
  wrapped_pin.set_hash_salt(StringOfZeros(16));

  return wrapped_pin;
}

struct TempDir {
 public:
  TempDir() { CHECK(dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

std::unique_ptr<network::NetworkService> CreateNetwork(
    mojo::Remote<network::mojom::NetworkContext>* network_context) {
  network::mojom::NetworkContextParamsPtr params =
      network::mojom::NetworkContextParams::New();
  params->cert_verifier_params =
      network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();

  auto service = network::NetworkService::CreateForTesting();
  service->CreateNetworkContext(network_context->BindNewPipeAndPassReceiver(),
                                std::move(params));

  return service;
}

scoped_refptr<device::JSONRequest> JSONFromString(std::string_view json_str) {
  base::Value json_request = base::JSONReader::Read(json_str).value();
  return base::MakeRefCounted<device::JSONRequest>(std::move(json_request));
}

class EnclaveManagerTest : public testing::Test, EnclaveManager::Observer {
 public:
  EnclaveManagerTest()
      : EnclaveManagerTest(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}

  explicit EnclaveManagerTest(
      base::test::TaskEnvironment::TimeSource time_source)
      // `IdentityTestEnvironment` wants to run on an IO thread.
      : task_env_(base::test::TaskEnvironment::MainThreadType::IO, time_source),
        temp_dir_(),
        process_and_port_(StartWebAuthnEnclave(temp_dir_.GetPath())),
        enclave_override_(
            TestWebAuthnEnclaveIdentity(process_and_port_.second)),
        network_service_(CreateNetwork(&network_context_)),
        security_domain_service_(
            FakeSecurityDomainService::New(kSecretVersion)),
        recovery_key_store_(FakeRecoveryKeyStore::New()),
        manager_(temp_dir_.GetPath(),
                 identity_test_env_.identity_manager(),
                 base::BindLambdaForTesting(
                     [&]() -> network::mojom::NetworkContext* {
                       return network_context_.get();
                     }),
                 url_loader_factory_.GetSafeWeakWrapper()) {
    OSCryptMocker::SetUp();

    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
    gaia_id_ = identity_test_env_.identity_manager()
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                   .gaia;
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    manager_.AddObserver(this);

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
    mock_hw_provider_ =
        std::make_unique<crypto::ScopedMockUnexportableKeyProvider>();
  }

  ~EnclaveManagerTest() override {
    if (manager_.RunWhenStoppedForTesting(task_env_.QuitClosure())) {
      task_env_.RunUntilQuit();
    }
    CHECK(process_and_port_.first.Terminate(/*exit_code=*/1, /*wait=*/true));
    OSCryptMocker::TearDown();
  }

 protected:
  base::flat_set<std::string> GaiaAccountsInState() const {
    const webauthn_pb::EnclaveLocalState& state =
        manager_.local_state_for_testing();
    base::flat_set<std::string> ret;
    for (const auto& it : state.users()) {
      ret.insert(it.first);
    }
    return ret;
  }

  void OnKeysStored() override { stored_count_++; }

  void DoCreate(
      std::unique_ptr<enclave::ClaimedPIN> claimed_pin,
      std::unique_ptr<sync_pb::WebauthnCredentialSpecifics>* out_specifics) {
    auto ui_request = std::make_unique<enclave::CredentialRequest>();
    ui_request->signing_callback = manager_.IdentityKeySigningCallback();
    int32_t secret_version;
    std::vector<uint8_t> wrapped_secret;
    std::tie(secret_version, wrapped_secret) =
        manager_.GetCurrentWrappedSecret();
    EXPECT_EQ(secret_version, kSecretVersion);
    ui_request->wrapped_secret = std::move(wrapped_secret);
    ui_request->key_version = kSecretVersion;
    ui_request->claimed_pin = std::move(claimed_pin);

    std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> specifics;
    ui_request->save_passkey_callback = base::BindLambdaForTesting(
        [&specifics](sync_pb::WebauthnCredentialSpecifics in_specifics) {
          specifics = std::make_unique<sync_pb::WebauthnCredentialSpecifics>(
              std::move(in_specifics));
        });

    enclave::EnclaveAuthenticator authenticator(
        std::move(ui_request), /*network_context_factory=*/
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          return network_context_.get();
        }));

    std::vector<device::PublicKeyCredentialParams::CredentialInfo>
        pub_key_params;
    pub_key_params.emplace_back();

    device::MakeCredentialOptions ctap_options;
    ctap_options.json = JSONFromString(R"({
        "attestation": "none",
        "authenticatorSelection": {
          "residentKey": "preferred",
          "userVerification": "preferred"
        },
        "challenge": "xHyLYEorFsaL6vb",
        "extensions": { "credProps": true },
        "pubKeyCredParams": [
          { "alg": -7, "type": "public-key" },
          { "alg": -257, "type": "public-key" }
        ],
        "rp": {
          "id": "webauthn.io",
          "name": "webauthn.io"
        },
        "user": {
          "displayName": "test",
          "id": "ZEdWemRB",
          "name": "test"
        }
      })");

    auto quit_closure = task_env_.QuitClosure();
    std::optional<device::MakeCredentialStatus> status;
    std::optional<device::AuthenticatorMakeCredentialResponse> response;
    authenticator.MakeCredential(
        /*request=*/{R"({"foo": "bar"})",
                     /*rp=*/{"rpid", "rpname"},
                     /*user=*/{{'u', 'i', 'd'}, "user", "display name"},
                     device::PublicKeyCredentialParams(
                         std::move(pub_key_params))},
        std::move(ctap_options),
        base::BindLambdaForTesting(
            [&quit_closure, &status, &response](
                device::MakeCredentialStatus in_status,
                std::optional<device::AuthenticatorMakeCredentialResponse>
                    in_responses) {
              status = in_status;
              response = std::move(in_responses);
              quit_closure.Run();
            }));
    task_env_.RunUntilQuit();

    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(status, device::MakeCredentialStatus::kSuccess);
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(specifics);
    EXPECT_EQ(specifics->rp_id(), "rpid");
    EXPECT_EQ(specifics->user_id(), "uid");
    EXPECT_EQ(specifics->user_name(), "user");
    EXPECT_EQ(specifics->user_display_name(), "display name");
    EXPECT_EQ(specifics->key_version(), kSecretVersion);

    if (out_specifics) {
      *out_specifics = std::move(specifics);
    }
  }

  struct GetAssertionResponseExpectation {
    device::GetAssertionStatus result = device::GetAssertionStatus::kSuccess;
    uint32_t size = 1;
  };

  std::optional<base::Time> LastPINRenewalTime() {
    std::optional<base::Time> ret;
    webauthn_pb::EnclaveLocalState& state = manager_.local_state_for_testing();
    if (state.users().size() == 0) {
      return ret;
    }
    CHECK_EQ(state.users().size(), 1u);
    if (!state.users().begin()->second.has_last_refreshed_pin_epoch_secs()) {
      return ret;
    }
    return base::Time::FromSecondsSinceUnixEpoch(
        state.users().begin()->second.last_refreshed_pin_epoch_secs());
  }

  void DoAssertion(
      std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity,
      std::unique_ptr<enclave::ClaimedPIN> claimed_pin,
      GetAssertionResponseExpectation expected_response,
      std::unique_ptr<enclave::CredentialRequest> custom_ui_request = nullptr) {
    std::unique_ptr<enclave::CredentialRequest> ui_request;
    if (custom_ui_request) {
      ui_request = std::move(custom_ui_request);
    } else {
      ui_request = std::make_unique<enclave::CredentialRequest>();
      ui_request->signing_callback = manager_.IdentityKeySigningCallback();
      ui_request->wrapped_secret =
          *manager_.GetWrappedSecret(/*version=*/kSecretVersion);
      ui_request->entity = std::move(entity);
      ui_request->claimed_pin = std::move(claimed_pin);
      ui_request->save_passkey_callback = base::BindOnce(
          [](sync_pb::WebauthnCredentialSpecifics) { NOTREACHED(); });
    }

    enclave::EnclaveAuthenticator authenticator(
        std::move(ui_request), /*network_context_factory=*/
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          return network_context_.get();
        }));

    device::CtapGetAssertionRequest ctap_request("test.com",
                                                 R"({"foo": "bar"})");
    ctap_request.allow_list.emplace_back(device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, /*id=*/{1, 2, 3, 4}));

    device::CtapGetAssertionOptions ctap_options;
    ctap_options.json = JSONFromString(R"({
        "allowCredentials": [ ],
        "challenge": "CYO8B30gOPIOVFAaU61J7PvoETG_sCZQ38Gzpu",
        "rpId": "webauthn.io",
        "userVerification": "preferred"
    })");

    auto quit_closure = task_env_.QuitClosure();
    std::optional<device::GetAssertionStatus> status;
    std::vector<device::AuthenticatorGetAssertionResponse> responses;
    authenticator.GetAssertion(
        std::move(ctap_request), std::move(ctap_options),
        base::BindLambdaForTesting(
            [&quit_closure, &status, &responses](
                device::GetAssertionStatus in_status,
                std::vector<device::AuthenticatorGetAssertionResponse>
                    in_responses) {
              status = in_status;
              responses = std::move(in_responses);
              quit_closure.Run();
            }));
    task_env_.RunUntilQuit();

    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(true);
    ASSERT_EQ(status, expected_response.result);
    ASSERT_EQ(responses.size(), expected_response.size);
  }

  bool Register() {
    BoolFuture register_future;
    manager_.RegisterIfNeeded(register_future.GetCallback());
    EXPECT_TRUE(register_future.Wait());
    return register_future.Get();
  }

  void CorruptDeviceId() {
    webauthn_pb::EnclaveLocalState& state = manager_.local_state_for_testing();
    ASSERT_EQ(state.users().size(), 1u);
    state.mutable_users()->begin()->second.set_device_id("corrupted value");
  }

  base::test::TaskEnvironment task_env_;
  unsigned stored_count_ = 0;
  const TempDir temp_dir_;
  const std::pair<base::Process, uint16_t> process_and_port_;
  const enclave::ScopedEnclaveOverride enclave_override_;
  network::TestURLLoaderFactory url_loader_factory_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<network::NetworkService> network_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::string gaia_id_;
  std::unique_ptr<FakeSecurityDomainService> security_domain_service_;
  std::unique_ptr<FakeRecoveryKeyStore> recovery_key_store_;
  std::unique_ptr<crypto::ScopedMockUnexportableKeyProvider> mock_hw_provider_;
  EnclaveManager manager_;
};

TEST_F(EnclaveManagerTest, TestInfrastructure) {
  // Tests that the enclave starts up.
}

TEST_F(EnclaveManagerTest, Basic) {
  security_domain_service_->pretend_there_are_members();

  ASSERT_FALSE(manager_.is_loaded());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());
  ASSERT_TRUE(register_future.Get());
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());
  EXPECT_TRUE(manager_.local_state_for_testing()
                  .users()
                  .find(gaia_id_)
                  ->second.identity_key_is_software_backed());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());
  EXPECT_EQ(stored_count_, 1u);

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());
  ASSERT_TRUE(add_future.Get());

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_FALSE(manager_.has_pending_keys());
  ASSERT_TRUE(manager_.TakeSecret());
  ASSERT_FALSE(manager_.TakeSecret());
  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 0u);

  DoCreate(/*claimed_pin=*/nullptr, /*out_specifics=*/nullptr);
  DoAssertion(GetTestEntity(), /*claimed_pin=*/nullptr,
              GetAssertionResponseExpectation());
}

TEST_F(EnclaveManagerTest, SecretsArriveBeforeRegistrationRequested) {
  security_domain_service_->pretend_there_are_members();
  ASSERT_FALSE(manager_.is_registered());

  // If secrets are provided before `RegisterIfNeeded` is called, the state
  // machine should still trigger registration.
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  EXPECT_TRUE(add_future.Wait());

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.TakeSecret());
}

TEST_F(EnclaveManagerTest, SecretsArriveBeforeRegistrationCompleted) {
  security_domain_service_->pretend_there_are_members();
  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_registered());

  // Provide the domain secrets before the registration has completed. The
  // system should still end up in the correct state.
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  EXPECT_TRUE(add_future.Wait());
  EXPECT_TRUE(register_future.Wait());

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.TakeSecret());
}

TEST_F(EnclaveManagerTest, RegistrationFailureAndRetry) {
  const std::string gaia =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  // Override the enclave with port=100, which will cause connection failures.
  {
    device::enclave::ScopedEnclaveOverride override(
        TestWebAuthnEnclaveIdentity(/*port=*/100));
    BoolFuture register_future;
    manager_.RegisterIfNeeded(register_future.GetCallback());
    EXPECT_TRUE(register_future.Wait());
    ASSERT_FALSE(register_future.Get());
  }
  ASSERT_FALSE(manager_.is_registered());
  const std::string public_key = manager_.local_state_for_testing()
                                     .users()
                                     .find(gaia)
                                     ->second.identity_public_key();
  ASSERT_FALSE(public_key.empty());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  EXPECT_TRUE(register_future.Wait());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(register_future.Get());

  // The public key should not have changed because re-registration attempts
  // must try the same public key again in case they actually worked the first
  // time.
  ASSERT_TRUE(public_key == manager_.local_state_for_testing()
                                .users()
                                .find(gaia)
                                ->second.identity_public_key());
}

TEST_F(EnclaveManagerTest, PrimaryUserChange) {
  const std::string gaia1 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  {
    BoolFuture register_future;
    manager_.RegisterIfNeeded(register_future.GetCallback());
    EXPECT_TRUE(register_future.Wait());
  }
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_THAT(GaiaAccountsInState(), testing::UnorderedElementsAre(gaia1));

  identity_test_env_.MakePrimaryAccountAvailable("test2@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  const std::string gaia2 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  ASSERT_FALSE(manager_.is_registered());
  {
    BoolFuture register_future;
    manager_.RegisterIfNeeded(register_future.GetCallback());
    EXPECT_TRUE(register_future.Wait());
  }
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_THAT(GaiaAccountsInState(),
              testing::UnorderedElementsAre(gaia1, gaia2));

  // Remove all accounts from the cookie jar. The primary account should be
  // retained.
  identity_test_env_.SetCookieAccounts({});
  EXPECT_THAT(GaiaAccountsInState(), testing::UnorderedElementsAre(gaia2));

  // When the primary account changes, the second account should be dropped
  // because it was removed from the cookie jar.
  identity_test_env_.MakePrimaryAccountAvailable("test3@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  const std::string gaia3 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  EXPECT_THAT(GaiaAccountsInState(), testing::UnorderedElementsAre(gaia3));
}

TEST_F(EnclaveManagerTest, PrimaryUserChangeDiscardsActions) {
  security_domain_service_->pretend_there_are_members();
  const std::string gaia1 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future1;
  manager_.RegisterIfNeeded(register_future1.GetCallback());
  BoolFuture register_future2;
  manager_.RegisterIfNeeded(register_future2.GetCallback());

  identity_test_env_.MakePrimaryAccountAvailable("test2@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  // `MakePrimaryAccountAvailable` should have canceled any actions.
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_FALSE(manager_.has_pending_keys());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  EXPECT_TRUE(register_future1.Wait());
  ASSERT_FALSE(register_future1.Get());
  EXPECT_TRUE(register_future2.Wait());
  ASSERT_FALSE(register_future2.Get());
}

TEST_F(EnclaveManagerTest, AddWithExistingPIN) {
  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    GetTestWrappedPIN().SerializeAsString(),
                                    /*expiry=*/base::Time()),
      add_future.GetCallback()));
  EXPECT_TRUE(add_future.Wait());

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.TakeSecret());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  // The PIN should not have been added to the account. Instead this test is
  // pretending that it was already there.
  EXPECT_EQ(security_domain_service_->num_pin_members(), 0u);
  EXPECT_TRUE(manager_.has_wrapped_pin());
}

TEST_F(EnclaveManagerTest, InvalidWrappedPIN) {
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);

  BoolFuture add_future;
  // A wrapped PIN that isn't a valid protobuf should be rejected.
  EXPECT_FALSE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    "nonsense wrapped PIN",
                                    /*expiry=*/base::Time()),
      add_future.GetCallback()));

  // A valid protobuf, but which fails invariants, should be rejected.
  webauthn_pb::EnclaveLocalState::WrappedPIN wrapped_pin = GetTestWrappedPIN();
  wrapped_pin.set_wrapped_pin("too short");
  EXPECT_FALSE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    wrapped_pin.SerializeAsString(),
                                    /*expiry=*/base::Time()),
      add_future.GetCallback()));
}

TEST_F(EnclaveManagerTest, SetupWithPIN) {
  const std::string pin = "123456";

  BoolFuture setup_future;
  manager_.SetupWithPIN(pin, setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());
  EXPECT_FALSE(manager_.wrapped_pin_is_arbitrary());
  EXPECT_TRUE(LastPINRenewalTime().has_value());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);
  const std::optional<std::vector<uint8_t>> security_domain_secret =
      FakeMagicArch::RecoverWithPIN(pin, *security_domain_service_,
                                    *recovery_key_store_);
  CHECK(security_domain_secret.has_value());
  EXPECT_EQ(manager_.TakeSecret()->second, *security_domain_secret);

  std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(pin, manager_.GetWrappedPIN());
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  DoCreate(/*claimed_pin=*/nullptr, &entity);
  DoAssertion(std::move(entity), std::move(claimed_pin),
              GetAssertionResponseExpectation());
}

TEST_F(EnclaveManagerTest, SetupWithPIN_SecurityDomainFailure) {
  security_domain_service_->fail_all_requests();

  BoolFuture setup_future;
  manager_.SetupWithPIN("123456", setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());
  ASSERT_FALSE(setup_future.Get());
  ASSERT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, SetupWithPIN_CertXMLFailure) {
  recovery_key_store_->break_cert_xml_file();

  BoolFuture setup_future;
  manager_.SetupWithPIN("123456", setup_future.GetCallback());
  // This test primarily shouldn't crash or hang.
  EXPECT_TRUE(setup_future.Wait());
  ASSERT_FALSE(setup_future.Get());
  ASSERT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, SetupWithPIN_SigXMLFailure) {
  recovery_key_store_->break_sig_xml_file();

  BoolFuture setup_future;
  manager_.SetupWithPIN("123456", setup_future.GetCallback());
  // This test primarily shouldn't crash or hang.
  EXPECT_TRUE(setup_future.Wait());
  ASSERT_FALSE(setup_future.Get());
  ASSERT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, AddDeviceAndPINToAccount) {
  security_domain_service_->pretend_there_are_members();
  const std::string pin = "pin";

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  manager_.AddDeviceAndPINToAccount(pin, add_future.GetCallback());
  EXPECT_TRUE(add_future.Wait());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());
  EXPECT_TRUE(manager_.wrapped_pin_is_arbitrary());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);
  const std::optional<std::vector<uint8_t>> security_domain_secret =
      FakeMagicArch::RecoverWithPIN(pin, *security_domain_service_,
                                    *recovery_key_store_);
  CHECK(security_domain_secret.has_value());
  EXPECT_EQ(manager_.TakeSecret()->second, *security_domain_secret);

  std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(pin, manager_.GetWrappedPIN());
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  DoCreate(/*claimed_pin=*/nullptr, &entity);
  DoAssertion(std::move(entity), std::move(claimed_pin),
              GetAssertionResponseExpectation());
}

TEST_F(EnclaveManagerTest, ChangePIN) {
  security_domain_service_->pretend_there_are_members();
  const std::string pin = "pin";
  const std::string new_pin = "newpin";

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  manager_.AddDeviceAndPINToAccount(pin, add_future.GetCallback());
  EXPECT_TRUE(add_future.Wait());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());
  EXPECT_TRUE(manager_.wrapped_pin_is_arbitrary());
  const std::vector<uint8_t> security_domain_secret =
      std::move(manager_.TakeSecret()->second);

  BoolFuture change_future;
  manager_.ChangePIN(new_pin, "rapt", change_future.GetCallback());
  EXPECT_TRUE(change_future.Wait());
  ASSERT_TRUE(change_future.Get());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);
  EXPECT_EQ(recovery_key_store_->vaults().size(), 2u);
  const std::optional<std::vector<uint8_t>> recovered_security_domain_secret =
      FakeMagicArch::RecoverWithPIN(new_pin, *security_domain_service_,
                                    *recovery_key_store_);
  CHECK(recovered_security_domain_secret.has_value());
  EXPECT_EQ(*recovered_security_domain_secret, security_domain_secret);

  std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(new_pin, manager_.GetWrappedPIN());
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  DoCreate(/*claimed_pin=*/nullptr, &entity);
  DoAssertion(std::move(entity), std::move(claimed_pin),
              GetAssertionResponseExpectation());
}

TEST_F(EnclaveManagerTest, ChangePINWithTwoDevices) {
  security_domain_service_->pretend_there_are_members();
  const std::string pin = "pin";
  const std::string intermediate_pin = "intermediate_pin";
  const std::string new_pin = "newpin";

  EnclaveManager second_manager(
      temp_dir_.GetPath(), identity_test_env_.identity_manager(),
      base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
        return network_context_.get();
      }),
      url_loader_factory_.GetSafeWeakWrapper());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {key},
                     /*last_key_version=*/kSecretVersion);
  second_manager.StoreKeys(gaia_id_, {key},
                           /*last_key_version=*/kSecretVersion);

  LOG(INFO) << "Adding first manager";
  {
    BoolFuture add_future;
    manager_.AddDeviceAndPINToAccount(pin, add_future.GetCallback());
    EXPECT_TRUE(add_future.Wait());
    ASSERT_TRUE(add_future.Get());
  }
  const std::vector<uint8_t> security_domain_secret =
      std::move(manager_.TakeSecret()->second);

  LOG(INFO) << "Adding second manager";
  {
    BoolFuture add_future;
    second_manager.AddDeviceToAccount(std::nullopt, add_future.GetCallback());
    EXPECT_TRUE(add_future.Wait());
  }

  LOG(INFO) << "First PIN change";
  {
    BoolFuture change_future;
    // `second_manager` must fetch PIN information from the security domain in
    // order to change it.
    second_manager.ChangePIN(intermediate_pin, "rapt",
                             change_future.GetCallback());
    EXPECT_TRUE(change_future.Wait());
    ASSERT_TRUE(change_future.Get());
  }

  LOG(INFO) << "Second PIN change";
  {
    BoolFuture change_future;
    manager_.ChangePIN(new_pin, "rapt", change_future.GetCallback());
    EXPECT_TRUE(change_future.Wait());
    ASSERT_TRUE(change_future.Get());
  }

  EXPECT_EQ(security_domain_service_->num_physical_members(), 2u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);
  EXPECT_EQ(recovery_key_store_->vaults().size(), 3u);
  const std::optional<std::vector<uint8_t>> recovered_security_domain_secret =
      FakeMagicArch::RecoverWithPIN(new_pin, *security_domain_service_,
                                    *recovery_key_store_);
  CHECK(recovered_security_domain_secret.has_value());
  EXPECT_EQ(*recovered_security_domain_secret, security_domain_secret);

  std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(new_pin, manager_.GetWrappedPIN());
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  DoCreate(/*claimed_pin=*/nullptr, &entity);
  DoAssertion(std::move(entity), std::move(claimed_pin),
              GetAssertionResponseExpectation());
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_SetupWithPIN) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();

  BoolFuture setup_future;
  manager_.SetupWithPIN("1234", setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());
  EXPECT_FALSE(setup_future.Get());
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_AddDeviceToAccount) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();
  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    GetTestWrappedPIN().SerializeAsString(),
                                    /*expiry=*/base::Time()),
      add_future.GetCallback()));
  EXPECT_TRUE(add_future.Wait());
  EXPECT_FALSE(add_future.Get());
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_AddDeviceAndPINToAccount) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();

  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolFuture add_future;
  manager_.AddDeviceAndPINToAccount("1234", add_future.GetCallback());
  EXPECT_TRUE(add_future.Wait());
  EXPECT_FALSE(add_future.Get());
}

TEST_F(EnclaveManagerTest, RenewPIN) {
  ASSERT_TRUE(Register());

  const std::string pin = "123456";

  BoolFuture setup_future;
  manager_.SetupWithPIN(pin, setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  const std::optional<base::Time> initial_time = LastPINRenewalTime();
  ASSERT_TRUE(initial_time.has_value());

  BoolFuture renew_future;
  manager_.RenewPIN(renew_future.GetCallback());
  EXPECT_TRUE(renew_future.Wait());
  EXPECT_TRUE(renew_future.Get());

  // The number of PIN members must not have increased because the upload should
  // have reused the vault handle etc of the original.
  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  const std::optional<std::vector<uint8_t>> security_domain_secret =
      FakeMagicArch::RecoverWithPIN(pin, *security_domain_service_,
                                    *recovery_key_store_);
  CHECK(security_domain_secret.has_value());
  EXPECT_EQ(manager_.TakeSecret()->second, *security_domain_secret);
  EXPECT_TRUE(*LastPINRenewalTime() > *initial_time);
}

TEST_F(EnclaveManagerTest, EpochChanged) {
  ASSERT_TRUE(Register());

  BoolFuture setup_future;
  manager_.SetupWithPIN("123456", setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());
  EXPECT_TRUE(manager_.is_ready());

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult state;
  state.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  state.key_version = kSecretVersion;

  EXPECT_TRUE(manager_.ConsiderSecurityDomainState(state, base::DoNothing()));
  EXPECT_TRUE(manager_.is_idle());

  BoolFuture update_future;
  state.key_version = kSecretVersion + 1;
  EXPECT_FALSE(
      manager_.ConsiderSecurityDomainState(state, update_future.GetCallback()));
  EXPECT_TRUE(update_future.Wait());
  EXPECT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, PINChanged) {
  ASSERT_TRUE(Register());

  BoolFuture setup_future;
  manager_.SetupWithPIN("123456", setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());
  EXPECT_TRUE(manager_.is_ready());

  const webauthn_pb::EnclaveLocalState::User& user =
      manager_.local_state_for_testing().users().begin()->second;
  webauthn_pb::EnclaveLocalState::WrappedPIN wrapped_pin = user.wrapped_pin();
  wrapped_pin.set_generation(wrapped_pin.generation() + 1);

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult state;
  state.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  state.key_version = kSecretVersion;
  state.gpm_pin_metadata.emplace(user.pin_public_key(),
                                 wrapped_pin.SerializeAsString(),
                                 /*expiry=*/base::Time::FromTimeT(1));

  BoolFuture update_future;
  EXPECT_TRUE(
      manager_.ConsiderSecurityDomainState(state, update_future.GetCallback()));
  EXPECT_TRUE(update_future.Wait());
  EXPECT_TRUE(manager_.is_ready());
  const webauthn_pb::EnclaveLocalState::User& updated_user =
      manager_.local_state_for_testing().users().begin()->second;
  EXPECT_EQ(updated_user.wrapped_pin().generation(), wrapped_pin.generation());
}

TEST_F(EnclaveManagerTest, SigningFails) {
  auto ui_request = std::make_unique<enclave::CredentialRequest>();
  ui_request->signing_callback = AlwaysFailsSigningCallback();
  ui_request->wrapped_secret = {1, 2, 3};
  ui_request->key_version = 1;

  enclave::EnclaveAuthenticator authenticator(
      std::move(ui_request), /*network_context_factory=*/
      base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
        return network_context_.get();
      }));

  std::vector<device::PublicKeyCredentialParams::CredentialInfo> pub_key_params;
  pub_key_params.emplace_back();

  device::MakeCredentialOptions ctap_options;
  ctap_options.json = JSONFromString(R"({
        "attestation": "none",
        "challenge": "xHyLYEorFsaL6vb",
        "extensions": { "credProps": true }
      })");

  auto quit_closure = task_env_.QuitClosure();
  std::optional<device::MakeCredentialStatus> status;
  std::optional<device::AuthenticatorMakeCredentialResponse> response;
  authenticator.MakeCredential(
      /*request=*/{R"({"foo": "bar"})",
                   /*rp=*/{"rpid", "rpname"},
                   /*user=*/{{'u', 'i', 'd'}, "user", "display name"},
                   device::PublicKeyCredentialParams(
                       std::move(pub_key_params))},
      std::move(ctap_options),
      base::BindLambdaForTesting(
          [&quit_closure, &status, &response](
              device::MakeCredentialStatus in_status,
              std::optional<device::AuthenticatorMakeCredentialResponse>
                  in_responses) {
            status = in_status;
            response = std::move(in_responses);
            quit_closure.Run();
          }));
  task_env_.RunUntilQuit();

  ASSERT_TRUE(status.has_value());
  ASSERT_EQ(status, device::MakeCredentialStatus::kEnclaveCancel);
  ASSERT_FALSE(response.has_value());
}

#if BUILDFLAG(IS_MAC)
TEST_F(EnclaveManagerTest, AddICloudRecoveryKey) {
  ASSERT_TRUE(Register());

  BoolFuture setup_future;
  manager_.SetupWithPIN("123456", setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());
  ASSERT_TRUE(manager_.is_ready());

  std::unique_ptr<device::enclave::ICloudRecoveryKey> icloud_key =
      device::enclave::ICloudRecoveryKey::CreateForTest();
  std::unique_ptr<trusted_vault::SecureBoxKeyPair> key =
      trusted_vault::SecureBoxKeyPair::CreateByPrivateKeyImport(
          icloud_key->key()->private_key().ExportToBytes());
  BoolFuture icloud_future;
  manager_.AddICloudRecoveryKey(std::move(icloud_key),
                                icloud_future.GetCallback());
  EXPECT_TRUE(icloud_future.Wait());
  EXPECT_TRUE(icloud_future.Get());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  // Find the iCloud recovery key member.
  const auto icloud_member = std::ranges::find_if(
      security_domain_service_->members(),
      [](const trusted_vault_pb::SecurityDomainMember& member) {
        return member.member_type() == trusted_vault_pb::SecurityDomainMember::
                                           MEMBER_TYPE_ICLOUD_KEYCHAIN;
      });
  ASSERT_NE(icloud_member, security_domain_service_->members().end());
  ASSERT_EQ(trusted_vault::ProtoStringToBytes(icloud_member->public_key()),
            key->public_key().ExportToBytes());

  // Use the iCloud recovery key to recover the security domain secret.
  const trusted_vault_pb::SharedMemberKey& shared_member_key =
      icloud_member->memberships().at(0).keys().at(0);
  const std::optional<std::vector<uint8_t>> security_domain_secret =
      key->private_key().Decrypt(base::span<const uint8_t>(),
                                 ToSpan("V1 shared_key"),
                                 ToSpan(shared_member_key.wrapped_key()));
  ASSERT_TRUE(security_domain_secret);
  EXPECT_EQ(manager_.TakeSecret()->second, *security_domain_secret);

  std::array<uint8_t, SHA256_DIGEST_LENGTH> expected_proof;
  unsigned expected_proof_len;
  HMAC(EVP_sha256(), security_domain_secret->data(),
       security_domain_secret->size(),
       reinterpret_cast<const uint8_t*>(icloud_member->public_key().data()),
       icloud_member->public_key().size(), expected_proof.data(),
       &expected_proof_len);
  ASSERT_EQ(expected_proof_len, expected_proof.size());
  EXPECT_EQ(base::span<const uint8_t>(expected_proof),
            ToSpan(shared_member_key.member_proof()));
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(EnclaveManagerTest, Unenroll) {
  ASSERT_TRUE(Register());

  ASSERT_TRUE(manager_.is_registered());
  BoolFuture unenroll_future;
  manager_.Unenroll(unenroll_future.GetCallback());
  EXPECT_TRUE(unenroll_future.Wait());
  EXPECT_TRUE(unenroll_future.Get());
  ASSERT_FALSE(manager_.is_registered());

  // Things should be in a good state such that we can register again.
  ASSERT_TRUE(Register());
  ASSERT_TRUE(manager_.is_registered());
}

TEST_F(EnclaveManagerTest, UnenrollRace) {
  ASSERT_TRUE(Register());

  // Should be safe to race multiple unenroll requests. The ones after the first
  // will fail when pending requests are cancelled.
  ASSERT_TRUE(manager_.is_registered());
  BoolFuture unenroll_future1;
  BoolFuture unenroll_future2;
  BoolFuture unenroll_future3;
  manager_.Unenroll(unenroll_future1.GetCallback());
  manager_.Unenroll(unenroll_future2.GetCallback());
  manager_.Unenroll(unenroll_future3.GetCallback());
  EXPECT_TRUE(unenroll_future1.Wait());
  EXPECT_TRUE(unenroll_future2.Wait());
  EXPECT_TRUE(unenroll_future3.Wait());
  EXPECT_TRUE(unenroll_future1.Get());
  EXPECT_FALSE(unenroll_future2.Get());
  EXPECT_FALSE(unenroll_future3.Get());
  ASSERT_FALSE(manager_.is_registered());
}

TEST_F(EnclaveManagerTest, UnenrollWithoutRegistering) {
  ASSERT_FALSE(manager_.is_registered());
  BoolFuture unenroll_future;
  manager_.Unenroll(unenroll_future.GetCallback());
  EXPECT_TRUE(unenroll_future.Wait());
  EXPECT_TRUE(unenroll_future.Get());
  ASSERT_FALSE(manager_.is_registered());
}

TEST_F(EnclaveManagerTest, LockPINThenChange) {
  const std::string pin = "123456";
  const std::string wrong_pin = "654321";

  BoolFuture setup_future;
  manager_.SetupWithPIN(pin, setup_future.GetCallback());
  EXPECT_TRUE(setup_future.Wait());

  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  DoCreate(/*claimed_pin=*/nullptr, &entity);

  // Use the wrong PIN until it's locked at the enclave.
  for (int i = 0; i < 5; i++) {
    std::unique_ptr<device::enclave::ClaimedPIN> wrong_claimed_pin =
        EnclaveManager::MakeClaimedPINSlowly(wrong_pin,
                                             manager_.GetWrappedPIN());
    DoAssertion(
        std::make_unique<sync_pb::WebauthnCredentialSpecifics>(*entity.get()),
        std::move(wrong_claimed_pin),
        GetAssertionResponseExpectation{
            .result = device::GetAssertionStatus::kUserConsentDenied,
            .size = 0});
  }

  // Even the correct PIN should fail now.
  std::unique_ptr<device::enclave::ClaimedPIN> correct_claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(pin, manager_.GetWrappedPIN());
  DoAssertion(
      std::make_unique<sync_pb::WebauthnCredentialSpecifics>(*entity.get()),
      std::move(correct_claimed_pin),
      GetAssertionResponseExpectation{
          .result = device::GetAssertionStatus::kUserConsentDenied, .size = 0});

  // Change the PIN.
  const std::string new_pin = "123123";
  BoolFuture change_future;
  manager_.ChangePIN(new_pin, "rapt", change_future.GetCallback());
  ASSERT_TRUE(change_future.Get());

  // The new PIN should work.
  std::unique_ptr<device::enclave::ClaimedPIN> new_correct_claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(new_pin, manager_.GetWrappedPIN());
  DoAssertion(std::move(entity), std::move(new_correct_claimed_pin),
              GetAssertionResponseExpectation());
}

// Tests that rely on `ScopedMockUnexportableKeyProvider` only work on
// platforms where EnclaveManager uses `GetUnexportableKeyProvider`, as opposed
// to `GetSoftwareUnsecureUnexportableKeyProvider`.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_HardwareKeyLost HardwareKeyLost
#else
#define MAYBE_HardwareKeyLost DISABLED_HardwareKeyLost
#endif
TEST_F(EnclaveManagerTest, MAYBE_HardwareKeyLost) {
  crypto::ScopedFakeUserVerifyingKeyProvider scoped_uv_key_provider;
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());

  base::RepeatingClosure quit_closure;
#if BUILDFLAG(IS_WIN)
  // Windows does deferred UV key creation. This test has to trigger the actual
  // create before testing that it is later deleted.
  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation);
  auto key_creation_callback = manager_.UserVerifyingKeyCreationCallback();
  quit_closure = task_env_.QuitClosure();
  std::move(key_creation_callback)
      .Run(base::BindLambdaForTesting(
          [&quit_closure](base::span<const uint8_t> uv_public_key) {
            EXPECT_FALSE(uv_public_key.empty());
            quit_closure.Run();
          }));
  task_env_.RunUntilQuit();
#endif

  mock_hw_provider_.reset();
  manager_.ClearCachedKeysForTesting();

  // Verify a UV key was created as well.
  std::string uv_key_label = manager_.local_state_for_testing()
                                 .mutable_users()
                                 ->begin()
                                 ->second.wrapped_uv_private_key();
  auto uv_key_provider = crypto::GetUserVerifyingKeyProvider(
      crypto::UserVerifyingKeyProvider::Config());
  base::test::TestFuture<
      base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                     crypto::UserVerifyingKeyCreationError>>
      key_future_present;
  uv_key_provider->GetUserVerifyingSigningKey(uv_key_label,
                                              key_future_present.GetCallback());
  EXPECT_TRUE(key_future_present.Wait());
  EXPECT_TRUE(key_future_present.Get().has_value());

  crypto::ScopedNullUnexportableKeyProvider null_hw_provider;
  auto signing_callback = manager_.IdentityKeySigningCallback();
  quit_closure = task_env_.QuitClosure();
  std::move(signing_callback)
      .Run({1, 2, 3, 4},
           base::BindLambdaForTesting(
               [&quit_closure](
                   std::optional<enclave::ClientSignature> signature) {
                 EXPECT_EQ(signature, std::nullopt);
                 quit_closure.Run();
               }));
  task_env_.RunUntilQuit();
  EXPECT_FALSE(manager_.is_registered());

  // Verify that the UV key was deleted when the HW key was lost.
  base::test::TestFuture<
      base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                     crypto::UserVerifyingKeyCreationError>>
      key_future_deleted;
  uv_key_provider->GetUserVerifyingSigningKey(uv_key_label,
                                              key_future_deleted.GetCallback());
  EXPECT_TRUE(key_future_deleted.Wait());
  EXPECT_FALSE(key_future_deleted.Get().has_value());
}

class EnclaveManagerMockTimeTest : public EnclaveManagerTest {
 public:
  EnclaveManagerMockTimeTest()
      : EnclaveManagerTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }
};

TEST_F(EnclaveManagerMockTimeTest, AutomaticRenewal) {
  const std::string pin = "123456";

  BoolFuture setup_future;
  manager_.SetupWithPIN(pin, setup_future.GetCallback());

  // Because this test runs under MOCK_TIME, waiting for `setup_future` causes
  // all events to run immediately, including the timeout timer for the
  // WebSocket. Thus we have to step time forwards incrementally so that the
  // enclave process (which isn't under MOCK_TIME) gets a chance to do
  // something.
  const base::TimeDelta time_step = base::Milliseconds(1);
  while (!setup_future.IsReady()) {
    base::PlatformThread::Sleep(time_step);
    task_env_.FastForwardBy(time_step);
  }
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());

  // When using MOCK_TIME, requests to the enclave will likely timeout as noted
  // just above. But to avoid flakes, this ensures that the requests will always
  // fail.
  device::enclave::ScopedEnclaveOverride override(
      TestWebAuthnEnclaveIdentity(/*port=*/100));

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);
  EXPECT_EQ(recovery_key_store_->vaults().size(), 1u);

  // Renewal will have been checked as soon as the state was loaded.
  EXPECT_EQ(manager_.renewal_checks_for_testing(), 1u);
  EXPECT_EQ(manager_.renewal_attempts_for_testing(), 0u);

  // After a day, another check should have been done.
  task_env_.FastForwardBy(base::Hours(24));
  EXPECT_EQ(manager_.renewal_checks_for_testing(), 2u);
  EXPECT_EQ(manager_.renewal_attempts_for_testing(), 0u);

  // After 30 days, there should be a renewal attempt.
  task_env_.FastForwardBy(base::Days(30));
  EXPECT_EQ(manager_.renewal_checks_for_testing(), 32u);
  EXPECT_EQ(manager_.renewal_attempts_for_testing(), 1u);

  // The renewal attempts will fail so an attempt should be made every day.
  task_env_.FastForwardBy(base::Days(1));
  EXPECT_EQ(manager_.renewal_checks_for_testing(), 33u);
  EXPECT_EQ(manager_.renewal_attempts_for_testing(), 2u);

  // Ensure that no operation is outstanding.
  task_env_.FastForwardBy(base::Hours(1));
}

// UV keys are only supported on Windows macOS, and ChromeOS at this time.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)

std::string ToString(base::span<const uint8_t> v) {
  return std::string(v.begin(), v.end());
}

class EnclaveUVTest : public EnclaveManagerTest {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    scoped_fake_apple_keychain_.SetUVMethod(
        crypto::ScopedFakeAppleKeychainV2::UVMethod::kPasswordOnly);
#endif  // BUILDFLAG(IS_MAC)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    OverrideWebAuthnChromeosUserVerifyingKeyProviderForTesting(nullptr);
#endif
  }

  void DisableUVKeySupport() {
    fake_provider_.emplace<crypto::ScopedNullUserVerifyingKeyProvider>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The scoped fake provider doesn't cover ChromeOS.
    OverrideWebAuthnChromeosUserVerifyingKeyProviderForTesting([]() {
      return std::unique_ptr<crypto::UserVerifyingKeyProvider>(nullptr);
    });
#endif
  }

  void UseFailingUVKeySupport() {
    fake_provider_.emplace<crypto::ScopedFailingUserVerifyingKeyProvider>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The scoped fake provider doesn't cover ChromeOS.
    NOTIMPLEMENTED();
#endif
  }

  absl::variant<crypto::ScopedFakeUserVerifyingKeyProvider,
                crypto::ScopedNullUserVerifyingKeyProvider,
                crypto::ScopedFailingUserVerifyingKeyProvider>
      fake_provider_;

#if BUILDFLAG(IS_MAC)
  crypto::ScopedFakeAppleKeychainV2 scoped_fake_apple_keychain_{
      "test-keychain-access-group"};
#endif  // BUILDFLAG(IS_MAC)
};

TEST_F(EnclaveUVTest, UserVerifyingKeyAvailable) {
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation);
#else
  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUI);
#endif
}

TEST_F(EnclaveUVTest, UserVerifyingKeyUnavailable) {
  DisableUVKeySupport();
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kNone);
}

TEST_F(EnclaveUVTest, UserVerifyingKeyLost) {
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());

  base::RepeatingClosure quit_closure;
#if BUILDFLAG(IS_WIN)
  // Windows does deferred UV key creation. This test has to trigger the actual
  // create before testing that it is later deleted.
  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation);
  auto key_creation_callback = manager_.UserVerifyingKeyCreationCallback();
  quit_closure = task_env_.QuitClosure();
  std::move(key_creation_callback)
      .Run(base::BindLambdaForTesting(
          [&quit_closure](base::span<const uint8_t> uv_public_key) {
            EXPECT_FALSE(uv_public_key.empty());
            quit_closure.Run();
          }));
  task_env_.RunUntilQuit();
#else
  ASSERT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUI);
#endif
  manager_.ClearCachedKeysForTesting();
  DisableUVKeySupport();
  auto signing_callback =
      manager_.UserVerifyingKeySigningCallback(/*options=*/{});
  quit_closure = task_env_.QuitClosure();
  std::move(signing_callback)
      .Run({1, 2, 3, 4},
           base::BindLambdaForTesting(
               [&quit_closure](
                   std::optional<enclave::ClientSignature> signature) {
                 EXPECT_EQ(signature, std::nullopt);
                 quit_closure.Run();
               }));
  task_env_.RunUntilQuit();
  EXPECT_FALSE(manager_.is_registered());
}

TEST_F(EnclaveUVTest, UserVerifyingKeyUseExisting) {
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  base::test::TestFuture<
      base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                     crypto::UserVerifyingKeyCreationError>>
      key_future;
  std::unique_ptr<crypto::UserVerifyingKeyProvider> key_provider =
      crypto::GetUserVerifyingKeyProvider(/*config=*/{});
  key_provider->GenerateUserVerifyingSigningKey(
      std::array{crypto::SignatureVerifier::ECDSA_SHA256},
      key_future.GetCallback());
  EXPECT_TRUE(key_future.Wait());
  manager_.local_state_for_testing()
      .mutable_users()
      ->begin()
      ->second.set_uv_public_key(
          ToString(key_future.Get().value()->GetPublicKey()));
  manager_.local_state_for_testing()
      .mutable_users()
      ->begin()
      ->second.set_wrapped_uv_private_key(
          key_future.Get().value()->GetKeyLabel());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());

  ASSERT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUI);
}

#if BUILDFLAG(IS_MAC)
// Tests that if biometrics are available on macOS, Chrome will handle prompting
// the user for biometrics.
TEST_F(EnclaveUVTest, ChromeHandlesBiometrics) {
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());

  scoped_fake_apple_keychain_.SetUVMethod(
      crypto::ScopedFakeAppleKeychainV2::UVMethod::kBiometrics);
  // The TouchID view is only available on macOS 12+.
  if (__builtin_available(macos 12, *)) {
    EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/true),
              EnclaveManager::UvKeyState::kUsesChromeUI);
  } else {
    EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
              EnclaveManager::UvKeyState::kUsesSystemUI);
  }

  scoped_fake_apple_keychain_.SetUVMethod(
      crypto::ScopedFakeAppleKeychainV2::UVMethod::kPasswordOnly);
  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUI);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
TEST_F(EnclaveUVTest, DeferredUVKeyCreation) {
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());

  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation);
  const auto& user_state =
      manager_.local_state_for_testing().users().find(gaia_id_)->second;
  EXPECT_TRUE(user_state.has_deferred_uv_key_creation() &&
              user_state.deferred_uv_key_creation());
  EXPECT_TRUE(user_state.wrapped_uv_private_key().empty());

  auto key_creation_callback = manager_.UserVerifyingKeyCreationCallback();
  auto quit_closure = task_env_.QuitClosure();
  std::move(key_creation_callback)
      .Run(base::BindLambdaForTesting(
          [&quit_closure](base::span<const uint8_t> uv_public_key) {
            EXPECT_FALSE(uv_public_key.empty());
            quit_closure.Run();
          }));
  task_env_.RunUntilQuit();

  EXPECT_FALSE(user_state.deferred_uv_key_creation());
  EXPECT_FALSE(user_state.wrapped_uv_private_key().empty());
}

TEST_F(EnclaveUVTest, UnregisterOnFailedDeferredUVKeyCreation) {
  security_domain_service_->pretend_there_are_members();
  NoArgFuture loaded_future;
  manager_.Load(loaded_future.GetCallback());
  EXPECT_TRUE(loaded_future.Wait());

  BoolFuture register_future;
  manager_.RegisterIfNeeded(register_future.GetCallback());
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(register_future.Wait());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolFuture add_future;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_future.GetCallback()));
  ASSERT_FALSE(manager_.is_idle());
  EXPECT_TRUE(add_future.Wait());

  EXPECT_EQ(manager_.uv_key_state(/*platform_has_biometrics=*/false),
            EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation);
  const auto& user_state =
      manager_.local_state_for_testing().users().find(gaia_id_)->second;
  EXPECT_TRUE(user_state.deferred_uv_key_creation());
  EXPECT_TRUE(user_state.wrapped_uv_private_key().empty());

  UseFailingUVKeySupport();
  EnclaveManager::EnableInvariantChecksForTesting(false);

  base::RunLoop run_loop;
  auto ui_request = std::make_unique<enclave::CredentialRequest>();
  ui_request->signing_callback = manager_.IdentityKeySigningCallback();
  ui_request->wrapped_secret =
      *manager_.GetWrappedSecret(/*version=*/kSecretVersion);
  ui_request->entity = GetTestEntity();
  ui_request->claimed_pin = nullptr;
  ui_request->save_passkey_callback = base::BindOnce(
      [](sync_pb::WebauthnCredentialSpecifics) { NOTREACHED(); });
  ui_request->user_verified = true;
  ui_request->uv_key_creation_callback =
      manager_.UserVerifyingKeyCreationCallback();
  ui_request->unregister_callback =
      base::BindOnce(&EnclaveManager::Unenroll, manager_.GetWeakPtr(),
                     base::BindLambdaForTesting(
                         [&run_loop](bool) { run_loop.QuitWhenIdle(); }));

  GetAssertionResponseExpectation expected_response;
  expected_response.result = device::GetAssertionStatus::kEnclaveError;
  expected_response.size = 0;
  DoAssertion(GetTestEntity(), /*claimed_pin=*/nullptr, expected_response,
              std::move(ui_request));
  run_loop.Run();

  EXPECT_FALSE(manager_.is_registered());
}

#endif  // BUILDFLAG(IS_WIN)

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

#endif  // !defined(MEMORY_SANITIZER)
