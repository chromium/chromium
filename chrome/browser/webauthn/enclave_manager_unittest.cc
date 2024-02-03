// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/enclave/types.h"
#include "net/base/port_util.h"
#include "net/http/http_status_code.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// The communication with the enclave process would need to be ported to Windows
// for these tests to run there.
//
// These tests are also disabled under MSAN. The enclave subprocess is written
// in Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if BUILDFLAG(IS_POSIX) && !defined(MEMORY_SANITIZER)

namespace enclave = device::enclave;

namespace {

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

std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> GetTestEntity() {
  auto ret = std::make_unique<sync_pb::WebauthnCredentialSpecifics>();
  CHECK(ret->ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
  return ret;
}

struct TempDir {
 public:
  TempDir() { CHECK(dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

std::pair<base::Process, uint16_t> StartEnclave(base::FilePath cwd) {
  base::FilePath data_root;
  CHECK(base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &data_root));
  const base::FilePath enclave_bin_path =
      data_root.AppendASCII("cloud_authenticator_test_service");
  base::LaunchOptions subprocess_opts;
  subprocess_opts.current_directory = cwd;

  std::optional<base::Process> enclave_process;
  uint16_t port;

  for (int i = 0; i < 10; i++) {
    int fds[2];
    CHECK(!pipe(fds));
    subprocess_opts.fds_to_remap.emplace_back(fds[1], 1);
    enclave_process = base::LaunchProcess(base::CommandLine(enclave_bin_path),
                                          subprocess_opts);
    CHECK(enclave_process->IsValid());
    close(fds[1]);

    char port_str[6];
    const ssize_t read_bytes =
        HANDLE_EINTR(read(fds[0], port_str, sizeof(port_str)));
    CHECK(read_bytes > 0);
    port_str[read_bytes - 1] = 0;
    unsigned u_port;
    CHECK(base::StringToUint(port_str, &u_port)) << port_str;
    port = base::checked_cast<uint16_t>(u_port);
    close(fds[0]);

    if (net::IsPortAllowedForScheme(port, "wss")) {
      break;
    }
    LOG(INFO) << "Port " << port << " not allowed. Trying again.";

    // The kernel randomly picked a port that Chromium will refuse to connect
    // to. Try again.
    enclave_process->Terminate(/*exit_code=*/1, /*wait=*/false);
  }

  return std::make_pair(std::move(*enclave_process), port);
}

enclave::ScopedEnclaveOverride TestEnclaveIdentity(uint16_t port) {
  constexpr std::array<uint8_t, device::kP256X962Length> kTestPublicKey = {
      0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc,
      0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
      0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
      0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
      0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
      0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
  };
  const std::string url = "ws://127.0.0.1:" + base::NumberToString(port);
  enclave::EnclaveIdentity identity;
  identity.url = GURL(url);
  identity.public_key = kTestPublicKey;

  return enclave::ScopedEnclaveOverride(std::move(identity));
}

trusted_vault_pb::JoinSecurityDomainsResponse MakeJoinSecurityDomainsResponse(
    int current_epoch) {
  trusted_vault_pb::JoinSecurityDomainsResponse response;
  trusted_vault_pb::SecurityDomain* security_domain =
      response.mutable_security_domain();
  security_domain->set_name(
      GetSecurityDomainPath(trusted_vault::SecurityDomainId::kPasskeys));
  security_domain->set_current_epoch(current_epoch);
  return response;
}

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

scoped_refptr<device::JSONRequest> JSONFromString(base::StringPiece json_str) {
  base::Value json_request = base::JSONReader::Read(json_str).value();
  return base::MakeRefCounted<device::JSONRequest>(std::move(json_request));
}

class EnclaveManagerTest : public testing::Test, EnclaveManager::Observer {
 public:
  EnclaveManagerTest()
      // `IdentityTestEnvironment` wants to run on an IO thread.
      : task_env_(base::test::TaskEnvironment::MainThreadType::IO),
        temp_dir_(),
        process_and_port_(StartEnclave(temp_dir_.GetPath())),
        enclave_override_(TestEnclaveIdentity(process_and_port_.second)),
        network_service_(CreateNetwork(&network_context_)),
        manager_(temp_dir_.GetPath(),
                 identity_test_env_.identity_manager(),
                 network_context_.get(),
                 url_loader_factory_.GetSafeWeakWrapper()) {
    OSCryptMocker::SetUp();

    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
    gaia_id_ = identity_test_env_.identity_manager()
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                   .gaia;
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    manager_.AddObserver(this);
  }

  ~EnclaveManagerTest() override {
    if (manager_.RunWhenStoppedForTesting(task_env_.QuitClosure())) {
      task_env_.RunUntilQuit();
    }
    CHECK(process_and_port_.first.Terminate(/*exit_code=*/1, /*wait=*/true));
    OSCryptMocker::TearDown();
  }

 protected:
  void RunUntilIdle() {
    quit_closure_ = task_env_.QuitClosure();
    task_env_.RunUntilQuit();
  }

  base::flat_set<std::string> GaiaAccountsInState() const {
    const webauthn_pb::EnclaveLocalState& state =
        manager_.local_state_for_testing();
    base::flat_set<std::string> ret;
    for (const auto& it : state.users()) {
      ret.insert(it.first);
    }
    return ret;
  }

  void OnEnclaveManagerIdle() override {
    if (manager_.is_idle() && quit_closure_.has_value()) {
      auto quit_closure = std::move(quit_closure_.value());
      quit_closure_.reset();
      quit_closure.Run();
    }
  }

  base::test::TaskEnvironment task_env_;
  std::optional<base::RepeatingClosure> quit_closure_;
  const TempDir temp_dir_;
  const std::pair<base::Process, uint16_t> process_and_port_;
  const enclave::ScopedEnclaveOverride enclave_override_;
  network::TestURLLoaderFactory url_loader_factory_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<network::NetworkService> network_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::string gaia_id_;
  EnclaveManager manager_;
};

TEST_F(EnclaveManagerTest, TestInfrastructure) {
  // Tests that the enclave starts up.
}

TEST_F(EnclaveManagerTest, Basic) {
  ASSERT_FALSE(manager_.is_loaded());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  manager_.Start();
  ASSERT_FALSE(manager_.is_idle());
  RunUntilIdle();
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  manager_.RegisterIfNeeded();
  ASSERT_FALSE(manager_.is_idle());
  RunUntilIdle();
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  const int32_t kSecretVersion = 417;
  url_loader_factory_.AddResponse(
      GetFullJoinSecurityDomainsURLForTesting(
          trusted_vault::ExtractTrustedVaultServiceURLFromCommandLine(),
          trusted_vault::SecurityDomainId::kPasskeys)
          .spec(),
      MakeJoinSecurityDomainsResponse(/*current_epoch=*/1).SerializeAsString());
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_FALSE(manager_.is_idle());
  RunUntilIdle();
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());

  {
    auto ui_request = std::make_unique<enclave::CredentialRequest>();
    ui_request->signing_callback = manager_.HardwareKeySigningCallback();
    ui_request->wrapped_secrets = {
        *manager_.GetWrappedSecret(/*version=*/kSecretVersion)};
    ui_request->entity = GetTestEntity();

    enclave::EnclaveAuthenticator authenticator(
        std::move(ui_request), /*save_passkey_callback=*/
        base::BindRepeating(
            [](sync_pb::WebauthnCredentialSpecifics) { NOTREACHED(); }),
        network_context_.get());

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
    ASSERT_EQ(status, device::CtapDeviceResponseCode::kSuccess);
    ASSERT_EQ(responses.size(), 1u);
  }

  {
    auto ui_request = std::make_unique<enclave::CredentialRequest>();
    ui_request->signing_callback = manager_.HardwareKeySigningCallback();
    int32_t secret_version;
    std::vector<uint8_t> wrapped_secret;
    std::tie(secret_version, wrapped_secret) =
        manager_.GetCurrentWrappedSecret();
    EXPECT_EQ(secret_version, kSecretVersion);
    ui_request->wrapped_secrets = {std::move(wrapped_secret)};
    ui_request->wrapped_secret_version = kSecretVersion;

    std::optional<sync_pb::WebauthnCredentialSpecifics> specifics;

    enclave::EnclaveAuthenticator authenticator(
        std::move(ui_request), /*save_passkey_callback=*/
        base::BindLambdaForTesting(
            [&specifics](sync_pb::WebauthnCredentialSpecifics in_specifics) {
              specifics.emplace(std::move(in_specifics));
            }),
        network_context_.get());

    std::vector<device::PublicKeyCredentialParams::CredentialInfo>
        pub_key_params;
    pub_key_params.emplace_back(
        device::PublicKeyCredentialParams::CredentialInfo());

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
    ASSERT_TRUE(specifics.has_value());
    EXPECT_EQ(specifics->rp_id(), "rpid");
    EXPECT_EQ(specifics->user_id(), "uid");
    EXPECT_EQ(specifics->user_name(), "user");
    EXPECT_EQ(specifics->user_display_name(), "display name");
    EXPECT_EQ(specifics->key_version(), kSecretVersion);
  }
}

TEST_F(EnclaveManagerTest, SecretsArriveBeforeRegistrationRequested) {
  manager_.Start();
  ASSERT_FALSE(manager_.is_registered());

  // If secrets are provided before `RegisterIfNeeded` is called, the state
  // machine should still trigger registration.
  url_loader_factory_.AddResponse(
      GetFullJoinSecurityDomainsURLForTesting(
          trusted_vault::ExtractTrustedVaultServiceURLFromCommandLine(),
          trusted_vault::SecurityDomainId::kPasskeys)
          .spec(),
      MakeJoinSecurityDomainsResponse(/*current_epoch=*/1).SerializeAsString());
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  RunUntilIdle();

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, SecretsArriveBeforeRegistrationCompleted) {
  manager_.Start();
  manager_.RegisterIfNeeded();
  ASSERT_FALSE(manager_.is_registered());

  // Provide the domain secrets before the registration has completed. The
  // system should still end up in the correct state.
  url_loader_factory_.AddResponse(
      GetFullJoinSecurityDomainsURLForTesting(
          trusted_vault::ExtractTrustedVaultServiceURLFromCommandLine(),
          trusted_vault::SecurityDomainId::kPasskeys)
          .spec(),
      MakeJoinSecurityDomainsResponse(/*current_epoch=*/1).SerializeAsString());
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  RunUntilIdle();

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, RegistrationFailureAndRetry) {
  const std::string gaia =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  // Override the enclave with port=100, which will cause connection failures.
  {
    device::enclave::ScopedEnclaveOverride override(
        TestEnclaveIdentity(/*port=*/100));
    manager_.Start();
    manager_.RegisterIfNeeded();
    RunUntilIdle();
  }
  ASSERT_FALSE(manager_.is_registered());
  const std::string public_key = manager_.local_state_for_testing()
                                     .users()
                                     .find(gaia)
                                     ->second.hardware_public_key();
  ASSERT_FALSE(public_key.empty());

  manager_.RegisterIfNeeded();
  RunUntilIdle();
  ASSERT_TRUE(manager_.is_registered());

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

  manager_.Start();
  manager_.RegisterIfNeeded();
  RunUntilIdle();
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_THAT(GaiaAccountsInState(), testing::UnorderedElementsAre(gaia1));

  identity_test_env_.MakePrimaryAccountAvailable("test2@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  const std::string gaia2 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  ASSERT_FALSE(manager_.is_registered());
  manager_.RegisterIfNeeded();
  RunUntilIdle();
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

}  // namespace

#endif  // IS_POSIX && !USING_MSAN
