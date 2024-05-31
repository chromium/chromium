// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager.h"

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/fake_magic_arch.h"
#include "chrome/browser/webauthn/fake_recovery_key_store.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "chrome/browser/webauthn/test_util.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/enclave/types.h"
#include "device/fido/test_callback_receiver.h"
#include "net/http/http_status_code.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/enclave/icloud_recovery_key_mac.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#endif  // BUILDFLAG(IS_MAC)

// These tests are also disabled under MSAN. The enclave subprocess is written
// in Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace enclave = device::enclave;
using NoArgCallback = device::test::TestCallbackReceiver<>;
using BoolCallback = device::test::TestCallbackReceiver<bool>;

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
      // `IdentityTestEnvironment` wants to run on an IO thread.
      : task_env_(base::test::TaskEnvironment::MainThreadType::IO),
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
    ui_request->signing_callback = manager_.HardwareKeySigningCallback();
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
    std::optional<device::CtapDeviceResponseCode> status;
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
                device::CtapDeviceResponseCode in_status,
                std::optional<device::AuthenticatorMakeCredentialResponse>
                    in_responses) {
              status = in_status;
              response = std::move(in_responses);
              quit_closure.Run();
            }));
    task_env_.RunUntilQuit();

    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(status, device::CtapDeviceResponseCode::kSuccess);
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
    device::CtapDeviceResponseCode result =
        device::CtapDeviceResponseCode::kSuccess;
    uint32_t size = 1;
  };

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
      ui_request->signing_callback = manager_.HardwareKeySigningCallback();
      ui_request->wrapped_secret =
          *manager_.GetWrappedSecret(/*version=*/kSecretVersion);
      ui_request->entity = std::move(entity);
      ui_request->claimed_pin = std::move(claimed_pin);
      ui_request->save_passkey_callback = base::BindOnce(
          [](sync_pb::WebauthnCredentialSpecifics) { NOTREACHED_NORETURN(); });
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
    std::optional<device::CtapDeviceResponseCode> status;
    std::vector<device::AuthenticatorGetAssertionResponse> responses;
    authenticator.GetAssertion(
        std::move(ctap_request), std::move(ctap_options),
        base::BindLambdaForTesting(
            [&quit_closure, &status, &responses](
                device::CtapDeviceResponseCode in_status,
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
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
    return std::get<0>(register_callback.result().value());
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

  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();
  ASSERT_TRUE(std::get<0>(register_callback.result().value()));
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());
  EXPECT_EQ(stored_count_, 1u);

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();
  ASSERT_TRUE(std::get<0>(add_callback.result().value()));

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
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  add_callback.WaitForCallback();

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.TakeSecret());
}

TEST_F(EnclaveManagerTest, SecretsArriveBeforeRegistrationCompleted) {
  security_domain_service_->pretend_there_are_members();
  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_registered());

  // Provide the domain secrets before the registration has completed. The
  // system should still end up in the correct state.
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  add_callback.WaitForCallback();
  register_callback.WaitForCallback();

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
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
    ASSERT_FALSE(std::get<0>(register_callback.result().value()));
  }
  ASSERT_FALSE(manager_.is_registered());
  const std::string public_key = manager_.local_state_for_testing()
                                     .users()
                                     .find(gaia)
                                     ->second.hardware_public_key();
  ASSERT_FALSE(public_key.empty());

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  register_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(std::get<0>(register_callback.result().value()));

  // The public key should not have changed because re-registration attempts
  // must try the same public key again in case they actually worked the first
  // time.
  ASSERT_TRUE(public_key == manager_.local_state_for_testing()
                                .users()
                                .find(gaia)
                                ->second.hardware_public_key());
}

TEST_F(EnclaveManagerTest, PrimaryUserChange) {
  const std::string gaia1 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  {
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
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
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
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

  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback1;
  manager_.RegisterIfNeeded(register_callback1.callback());
  BoolCallback register_callback2;
  manager_.RegisterIfNeeded(register_callback2.callback());

  identity_test_env_.MakePrimaryAccountAvailable("test2@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  // `MakePrimaryAccountAvailable` should have canceled any actions.
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_FALSE(manager_.has_pending_keys());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  register_callback1.WaitForCallback();
  ASSERT_FALSE(std::get<0>(register_callback1.result().value()));
  register_callback2.WaitForCallback();
  ASSERT_FALSE(std::get<0>(register_callback2.result().value()));
}

TEST_F(EnclaveManagerTest, AddWithExistingPIN) {
  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    GetTestWrappedPIN().SerializeAsString(),
                                    /*expiry=*/base::Time()),
      add_callback.callback()));
  add_callback.WaitForCallback();

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

  BoolCallback add_callback;
  // A wrapped PIN that isn't a valid protobuf should be rejected.
  EXPECT_FALSE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    "nonsense wrapped PIN",
                                    /*expiry=*/base::Time()),
      add_callback.callback()));

  // A valid protobuf, but which fails invariants, should be rejected.
  webauthn_pb::EnclaveLocalState::WrappedPIN wrapped_pin = GetTestWrappedPIN();
  wrapped_pin.set_wrapped_pin("too short");
  EXPECT_FALSE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    wrapped_pin.SerializeAsString(),
                                    /*expiry=*/base::Time()),
      add_callback.callback()));
}

TEST_F(EnclaveManagerTest, SetupWithPIN) {
  const std::string pin = "123456";

  BoolCallback setup_callback;
  manager_.SetupWithPIN(pin, setup_callback.callback());
  setup_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());
  EXPECT_FALSE(manager_.wrapped_pin_is_arbitrary());

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

TEST_F(EnclaveManagerTest, SetupWithPIN_CertXMLFailure) {
  recovery_key_store_->break_cert_xml_file();

  BoolCallback setup_callback;
  manager_.SetupWithPIN("123456", setup_callback.callback());
  // This test primarily shouldn't crash or hang.
  setup_callback.WaitForCallback();
  ASSERT_FALSE(std::get<0>(setup_callback.result().value()));
  ASSERT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, SetupWithPIN_SigXMLFailure) {
  recovery_key_store_->break_sig_xml_file();

  BoolCallback setup_callback;
  manager_.SetupWithPIN("123456", setup_callback.callback());
  // This test primarily shouldn't crash or hang.
  setup_callback.WaitForCallback();
  ASSERT_FALSE(std::get<0>(setup_callback.result().value()));
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

  BoolCallback add_callback;
  manager_.AddDeviceAndPINToAccount(pin, add_callback.callback());
  add_callback.WaitForCallback();
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

  BoolCallback add_callback;
  manager_.AddDeviceAndPINToAccount(pin, add_callback.callback());
  add_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());
  EXPECT_TRUE(manager_.wrapped_pin_is_arbitrary());
  const std::vector<uint8_t> security_domain_secret =
      std::move(manager_.TakeSecret()->second);

  BoolCallback change_callback;
  manager_.ChangePIN(new_pin, "rapt", change_callback.callback());
  change_callback.WaitForCallback();
  ASSERT_TRUE(std::get<0>(change_callback.result().value()));

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

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_SetupWithPIN) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();

  BoolCallback setup_callback;
  manager_.SetupWithPIN("1234", setup_callback.callback());
  setup_callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(setup_callback.result().value()));
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_AddDeviceToAccount) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();
  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      trusted_vault::GpmPinMetadata(std::string(kTestPINPublicKey),
                                    GetTestWrappedPIN().SerializeAsString(),
                                    /*expiry=*/base::Time()),
      add_callback.callback()));
  add_callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(add_callback.result().value()));
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_AddDeviceAndPINToAccount) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();

  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  manager_.AddDeviceAndPINToAccount("1234", add_callback.callback());
  add_callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(add_callback.result().value()));
}

TEST_F(EnclaveManagerTest, RenewPIN) {
  ASSERT_TRUE(Register());

  const std::string pin = "123456";

  BoolCallback setup_callback;
  manager_.SetupWithPIN(pin, setup_callback.callback());
  setup_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  BoolCallback renew_callback;
  manager_.RenewPIN(renew_callback.callback());
  renew_callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(renew_callback.result().value()));

  // The number of PIN members must not have increased because the upload should
  // have reused the vault handle etc of the original.
  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  const std::optional<std::vector<uint8_t>> security_domain_secret =
      FakeMagicArch::RecoverWithPIN(pin, *security_domain_service_,
                                    *recovery_key_store_);
  CHECK(security_domain_secret.has_value());
  EXPECT_EQ(manager_.TakeSecret()->second, *security_domain_secret);
}

TEST_F(EnclaveManagerTest, EpochChanged) {
  ASSERT_TRUE(Register());

  BoolCallback setup_callback;
  manager_.SetupWithPIN("123456", setup_callback.callback());
  setup_callback.WaitForCallback();
  EXPECT_TRUE(manager_.is_ready());

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult state;
  state.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  state.key_version = kSecretVersion;

  EXPECT_TRUE(manager_.ConsiderSecurityDomainState(state, base::DoNothing()));
  EXPECT_TRUE(manager_.is_idle());

  BoolCallback update_callback;
  state.key_version = kSecretVersion + 1;
  EXPECT_FALSE(
      manager_.ConsiderSecurityDomainState(state, update_callback.callback()));
  update_callback.WaitForCallback();
  EXPECT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, PINChanged) {
  ASSERT_TRUE(Register());

  BoolCallback setup_callback;
  manager_.SetupWithPIN("123456", setup_callback.callback());
  setup_callback.WaitForCallback();
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

  BoolCallback update_callback;
  EXPECT_TRUE(
      manager_.ConsiderSecurityDomainState(state, update_callback.callback()));
  update_callback.WaitForCallback();
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
  std::optional<device::CtapDeviceResponseCode> status;
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
              device::CtapDeviceResponseCode in_status,
              std::optional<device::AuthenticatorMakeCredentialResponse>
                  in_responses) {
            status = in_status;
            response = std::move(in_responses);
            quit_closure.Run();
          }));
  task_env_.RunUntilQuit();

  ASSERT_TRUE(status.has_value());
  ASSERT_EQ(status, device::CtapDeviceResponseCode::kCtap2ErrOperationDenied);
  ASSERT_FALSE(response.has_value());
}

#if BUILDFLAG(IS_MAC)
TEST_F(EnclaveManagerTest, AddICloudRecoveryKey) {
  ASSERT_TRUE(Register());

  BoolCallback setup_callback;
  manager_.SetupWithPIN("123456", setup_callback.callback());
  setup_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_ready());

  std::unique_ptr<device::enclave::ICloudRecoveryKey> icloud_key =
      device::enclave::ICloudRecoveryKey::CreateForTest();
  std::unique_ptr<trusted_vault::SecureBoxKeyPair> key =
      trusted_vault::SecureBoxKeyPair::CreateByPrivateKeyImport(
          icloud_key->key()->private_key().ExportToBytes());
  BoolCallback icloud_callback;
  manager_.AddICloudRecoveryKey(std::move(icloud_key),
                                icloud_callback.callback());
  icloud_callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(icloud_callback.result().value()));

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
  BoolCallback unenroll_callback;
  manager_.Unenroll(unenroll_callback.callback());
  unenroll_callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(unenroll_callback.result().value()));
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
  BoolCallback unenroll_callback1;
  BoolCallback unenroll_callback2;
  BoolCallback unenroll_callback3;
  manager_.Unenroll(unenroll_callback1.callback());
  manager_.Unenroll(unenroll_callback2.callback());
  manager_.Unenroll(unenroll_callback3.callback());
  unenroll_callback1.WaitForCallback();
  unenroll_callback2.WaitForCallback();
  unenroll_callback3.WaitForCallback();
  EXPECT_TRUE(std::get<0>(unenroll_callback1.result().value()));
  EXPECT_FALSE(std::get<0>(unenroll_callback2.result().value()));
  EXPECT_FALSE(std::get<0>(unenroll_callback3.result().value()));
  ASSERT_FALSE(manager_.is_registered());
}

TEST_F(EnclaveManagerTest, UnenrollWithoutRegistering) {
  ASSERT_FALSE(manager_.is_registered());
  BoolCallback unenroll_callback;
  manager_.Unenroll(unenroll_callback.callback());
  unenroll_callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(unenroll_callback.result().value()));
  ASSERT_FALSE(manager_.is_registered());
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
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

  base::RepeatingClosure quit_closure;
#if BUILDFLAG(IS_WIN)
  // Windows does deferred UV key creation. This test has to trigger the actual
  // create before testing that it is later deleted.
  EXPECT_EQ(manager_.uv_key_state(),
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
  quit_closure = task_env_.QuitClosure();
  uv_key_provider->GetUserVerifyingSigningKey(
      uv_key_label,
      base::BindLambdaForTesting(
          [&quit_closure](
              std::unique_ptr<crypto::UserVerifyingSigningKey> key) {
            EXPECT_NE(key, nullptr);
            quit_closure.Run();
          }));
  task_env_.RunUntilQuit();

  crypto::ScopedNullUnexportableKeyProvider null_hw_provider;
  auto signing_callback = manager_.HardwareKeySigningCallback();
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
  quit_closure = task_env_.QuitClosure();
  uv_key_provider->GetUserVerifyingSigningKey(
      uv_key_label,
      base::BindLambdaForTesting(
          [&quit_closure](
              std::unique_ptr<crypto::UserVerifyingSigningKey> key) {
            EXPECT_EQ(key, nullptr);
            quit_closure.Run();
          }));
  task_env_.RunUntilQuit();
}

// UV keys are only supported on Windows and macOS at this time.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

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

  void DisableUVKeySupport() {
    fake_provider_.emplace<crypto::ScopedNullUserVerifyingKeyProvider>();
  }

  void UseFailingUVKeySupport() {
    fake_provider_.emplace<crypto::ScopedFailingUserVerifyingKeyProvider>();
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
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(manager_.uv_key_state(),
            EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation);
#else
  EXPECT_EQ(manager_.uv_key_state(), EnclaveManager::UvKeyState::kUsesSystemUI);
#endif
}

TEST_F(EnclaveUVTest, UserVerifyingKeyUnavailable) {
  DisableUVKeySupport();
  security_domain_service_->pretend_there_are_members();
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_EQ(manager_.uv_key_state(), EnclaveManager::UvKeyState::kNone);
}

TEST_F(EnclaveUVTest, UserVerifyingKeyLost) {
  security_domain_service_->pretend_there_are_members();
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

  base::RepeatingClosure quit_closure;
#if BUILDFLAG(IS_WIN)
  // Windows does deferred UV key creation. This test has to trigger the actual
  // create before testing that it is later deleted.
  EXPECT_EQ(manager_.uv_key_state(),
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
  ASSERT_EQ(manager_.uv_key_state(), EnclaveManager::UvKeyState::kUsesSystemUI);
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
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  device::test::ValueCallbackReceiver<
      std::unique_ptr<crypto::UserVerifyingSigningKey>>
      key_callback;
  std::unique_ptr<crypto::UserVerifyingKeyProvider> key_provider =
      crypto::GetUserVerifyingKeyProvider(/*config=*/{});
  key_provider->GenerateUserVerifyingSigningKey(
      std::array{crypto::SignatureVerifier::ECDSA_SHA256},
      key_callback.callback());
  key_callback.WaitForCallback();
  manager_.local_state_for_testing()
      .mutable_users()
      ->begin()
      ->second.set_uv_public_key(
          ToString(key_callback.value()->GetPublicKey()));
  manager_.local_state_for_testing()
      .mutable_users()
      ->begin()
      ->second.set_wrapped_uv_private_key(key_callback.value()->GetKeyLabel());

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

  ASSERT_EQ(manager_.uv_key_state(), EnclaveManager::UvKeyState::kUsesSystemUI);
}

#if BUILDFLAG(IS_MAC)
// Tests that if biometrics are available on macOS, Chrome will handle prompting
// the user for biometrics.
TEST_F(EnclaveUVTest, ChromeHandlesBiometrics) {
  security_domain_service_->pretend_there_are_members();
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

  scoped_fake_apple_keychain_.SetUVMethod(
      crypto::ScopedFakeAppleKeychainV2::UVMethod::kBiometrics);
  // The TouchID view is only available on macOS 12+.
  if (__builtin_available(macos 12, *)) {
    EXPECT_EQ(manager_.uv_key_state(),
              EnclaveManager::UvKeyState::kUsesChromeUI);
  } else {
    EXPECT_EQ(manager_.uv_key_state(),
              EnclaveManager::UvKeyState::kUsesSystemUI);
  }

  scoped_fake_apple_keychain_.SetUVMethod(
      crypto::ScopedFakeAppleKeychainV2::UVMethod::kPasswordOnly);
  EXPECT_EQ(manager_.uv_key_state(), EnclaveManager::UvKeyState::kUsesSystemUI);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
TEST_F(EnclaveUVTest, DeferredUVKeyCreation) {
  security_domain_service_->pretend_there_are_members();
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

  EXPECT_EQ(manager_.uv_key_state(),
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
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

  EXPECT_EQ(manager_.uv_key_state(),
            EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation);
  const auto& user_state =
      manager_.local_state_for_testing().users().find(gaia_id_)->second;
  EXPECT_TRUE(user_state.deferred_uv_key_creation());
  EXPECT_TRUE(user_state.wrapped_uv_private_key().empty());

  UseFailingUVKeySupport();
  EnclaveManager::EnableInvariantChecksForTesting(false);

  base::RunLoop run_loop;
  auto ui_request = std::make_unique<enclave::CredentialRequest>();
  ui_request->signing_callback = manager_.HardwareKeySigningCallback();
  ui_request->wrapped_secret =
      *manager_.GetWrappedSecret(/*version=*/kSecretVersion);
  ui_request->entity = GetTestEntity();
  ui_request->claimed_pin = nullptr;
  ui_request->save_passkey_callback = base::BindOnce(
      [](sync_pb::WebauthnCredentialSpecifics) { NOTREACHED_NORETURN(); });
  ui_request->user_verified = true;
  ui_request->uv_key_creation_callback =
      manager_.UserVerifyingKeyCreationCallback();
  ui_request->unregister_callback =
      base::BindOnce(&EnclaveManager::Unenroll, manager_.GetWeakPtr(),
                     base::BindLambdaForTesting(
                         [&run_loop](bool) { run_loop.QuitWhenIdle(); }));

  GetAssertionResponseExpectation expected_response;
  expected_response.result = device::CtapDeviceResponseCode::kCtap2ErrOther;
  expected_response.size = 0;
  DoAssertion(GetTestEntity(), /*claimed_pin=*/nullptr, expected_response,
              std::move(ui_request));
  run_loop.Run();

  EXPECT_FALSE(manager_.is_registered());
}

#endif  // BUILDFLAG(IS_WIN)

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

#endif  // !defined(MEMORY_SANITIZER)
