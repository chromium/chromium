// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_client.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_frontend.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/test/fake_security_domains_server.h"
#include "components/trusted_vault/test/fake_security_domains_url_loader_factory.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_throttling_connection_impl.h"
#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif
#include "google_apis/gaia/gaia_id.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Not;
using testing::SizeIs;

constexpr char kTestEmail[] = "test@gmail.com";
constexpr char kTestServerUrl[] = "http://trusted-vault.test";

#if BUILDFLAG(IS_MAC)
constexpr char kKeychainAccessGroupPrefix[] = "test-access-group-prefix";
constexpr char kKeychainAccessGroup[] =
    "test-access-group-prefix.com.google.common.folsom";
// On Mac, both the physical device recovery factor and the iCloud Keychain
// recovery factor are registered by default.
constexpr int kDefaultExpectedMemberCount = 2;
#else
constexpr int kDefaultExpectedMemberCount = 1;
#endif

class MockTrustedVaultClientObserver : public TrustedVaultClient::Observer {
 public:
  MockTrustedVaultClientObserver() = default;
  ~MockTrustedVaultClientObserver() override = default;

  MOCK_METHOD(void,
              OnTrustedVaultKeysChanged,
              (std::optional<TrustedVaultUserActionTriggerForUMA>),
              (override));
  MOCK_METHOD(void, OnTrustedVaultRecoverabilityChanged, (), (override));
};

// TESTING BOUNDARY AND CLASSES UNDER TEST:
// This fixture implements multi-class client integration tests that evaluate
// the complete standalone trusted vault client subsystem without a browser
// process.
//
// 1. Classes Under Test:
//    - StandaloneTrustedVaultClient (public client API)
//    - StandaloneTrustedVaultBackend (core engine, running in a sequence)
//    - Persistent disk storage layer
//    - Recovery factor registration / retrieval logic
//
// 2. Testing Boundaries:
//    - All test interactions occur through the public TrustedVaultClient
//      interface.
//    - RPC calls to SecurityDomainsService are intercepted and faked at the
//      TrustedVaultConnection boundary using FakeSecurityDomainsServer and
//      FakeSecurityDomainsURLLoaderFactory (in-memory, no real TCP sockets).
//    - Persistent protobuf serialization (trusted_vault.pb) is exercised
//      against a real disk directory using base::ScopedTempDir.
//
// Note: These tests do not aim to cover all possible edge cases - there are
// more suitable places to do that, e.g. in
// standalone_trusted_vault_backend_unittest.cc. The goal here is to test the
// interaction of the various components (connection, storage, recovery
// factors, helpers, etc.) for relevant user scenarios.
class StandaloneTrustedVaultClientTest : public testing::Test {
 public:
  StandaloneTrustedVaultClientTest() = default;
  ~StandaloneTrustedVaultClientTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    const GURL test_server_url(kTestServerUrl);
    fake_security_domains_server_ =
        std::make_unique<FakeSecurityDomainsServer>(test_server_url);
    url_loader_factory_ =
        base::MakeRefCounted<FakeSecurityDomainsURLLoaderFactory>(
            fake_security_domains_server_.get(), test_server_url);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kTrustedVaultServiceURLSwitch,
        FakeSecurityDomainsServer::GetServerURL(test_server_url).spec());

    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDown() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        kTrustedVaultServiceURLSwitch);
    // Make sure to run all pending deletions.
    task_environment_.RunUntilIdle();
  }

  scoped_refptr<StandaloneTrustedVaultFrontend> CreateFrontend() {
    return CreateFrontendForBaseDir(temp_dir_.GetPath());
  }

  scoped_refptr<StandaloneTrustedVaultFrontend> CreateFrontendForBaseDir(
      const base::FilePath& base_dir) {
    return base::MakeRefCounted<StandaloneTrustedVaultFrontend>(
        os_crypt_async_.get(),
#if BUILDFLAG(IS_MAC)
        kKeychainAccessGroupPrefix,
#endif
        base_dir, identity_test_env_.identity_manager(), url_loader_factory_);
  }

  std::unique_ptr<StandaloneTrustedVaultClient> CreateClient(
      SecurityDomainId security_domain = SecurityDomainId::kChromeSync,
      scoped_refptr<StandaloneTrustedVaultFrontend> frontend = nullptr) {
    if (!frontend) {
      frontend = CreateFrontend();
    }
    return std::make_unique<StandaloneTrustedVaultClient>(security_domain,
                                                          std::move(frontend));
  }

  CoreAccountInfo MakeAccountAvailable(const std::string& email) {
    return identity_test_env_
        .MakePrimaryAccountAvailable(email, signin::ConsentLevel::kSignin)
        .GetCoreAccountInfo();
  }

  void WaitForIdle(StandaloneTrustedVaultClient* client) {
    base::test::TestFuture<void> future;
    client->WaitForIdleForTesting(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  std::vector<std::vector<uint8_t>> FetchKeys(
      StandaloneTrustedVaultClient* client,
      const CoreAccountInfo& account) {
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    client->FetchKeys(account, future.GetCallback());
    return future.Get();
  }

  void StoreKeys(StandaloneTrustedVaultClient* client,
                 const GaiaId& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) {
    client->StoreKeys(gaia_id, keys, last_key_version,
                      /*trigger=*/std::nullopt);
  }

  bool MarkLocalKeysAsStale(StandaloneTrustedVaultClient* client,
                            const CoreAccountInfo& account) {
    base::test::TestFuture<bool> future;
    client->MarkLocalKeysAsStale(account, future.GetCallback());
    return future.Get();
  }

  void AddTrustedRecoveryMethod(StandaloneTrustedVaultClient* client,
                                const GaiaId& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint) {
    base::test::TestFuture<bool> future;
    client->AddTrustedRecoveryMethod(gaia_id, public_key, method_type_hint,
                                     future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  int GetLastKeyVersion(StandaloneTrustedVaultClient* client,
                        const GaiaId& gaia_id) {
    base::test::TestFuture<int> future;
    client->GetLastKeyVersionForTesting(gaia_id, future.GetCallback());
    return future.Get();
  }

  bool CanEnroll(StandaloneTrustedVaultClient* client,
                 const CoreAccountInfo& account) {
    base::test::TestFuture<bool> future;
    client->CanEnroll(account, future.GetCallback());
    return future.Get();
  }

  std::pair<std::vector<uint8_t>, int> Enroll(
      StandaloneTrustedVaultClient* client,
      const CoreAccountInfo& account) {
    base::test::TestFuture<const std::vector<uint8_t>&, int> future;
    client->Enroll(account, future.GetCallback());
    return {future.Get<0>(), future.Get<1>()};
  }

 protected:
#if BUILDFLAG(IS_MAC)
  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{kKeychainAccessGroup};
#endif
  base::test::ScopedFeatureList scoped_feature_list_{
      kTrustedVaultSharedRecoveryFactorsAndEncryption};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting();
  std::unique_ptr<FakeSecurityDomainsServer> fake_security_domains_server_;
  scoped_refptr<FakeSecurityDomainsURLLoaderFactory> url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(StandaloneTrustedVaultClientTest, ShouldPreEnrollOnStartup) {
  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  // During pre-enrollment (when only constant keys exist), iCloud Keychain
  // recovery factor registration is not supported, so only physical device
  // recovery factor is registered.
  WaitForIdle(client.get());
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(), 1);
  EXPECT_TRUE(fake_security_domains_server_->AllMembersHaveKey(
      GetConstantTrustedVaultKey()));

  EXPECT_THAT(FetchKeys(client.get(), account_info), IsEmpty());
  EXPECT_GT(fake_security_domains_server_->GetCurrentEpoch(), 0);
  EXPECT_THAT(fake_security_domains_server_->GetAllTrustedVaultKeys(),
              ElementsAre(GetConstantTrustedVaultKey()));
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldFallbackToKeyRetrievalWhenPreEnrollmentRejected) {
  const std::vector<uint8_t> kServerKey = {1, 2, 3, 4};
  const int kServerEpoch = 10;
  fake_security_domains_server_->ResetDataToState({kServerKey}, kServerEpoch);

  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  WaitForIdle(client.get());

  // Background pre-enrollment registration was rejected with
  // kLocalDataObsolete.
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(), 0);
  EXPECT_THAT(FetchKeys(client.get(), account_info), IsEmpty());

  // Fallback key retrieval via StoreKeys resolves error and registers factors.
  MockTrustedVaultClientObserver observer;
  client->AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged);
  StoreKeys(client.get(), account_info.gaia, {kServerKey}, kServerEpoch);
  WaitForIdle(client.get());
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(),
            kDefaultExpectedMemberCount);
  EXPECT_TRUE(fake_security_domains_server_->AllMembersHaveKey(kServerKey));

  EXPECT_THAT(FetchKeys(client.get(), account_info), ElementsAre(kServerKey));
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldFollowKeyRotationOnUserEnrolment) {
  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  WaitForIdle(client.get());
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(), 1);
  EXPECT_TRUE(fake_security_domains_server_->AllMembersHaveKey(
      GetConstantTrustedVaultKey()));

  // FetchKeys() returns empty when no non-constant keys exist on the server.
  // Because the server responds with kNoNewKeys, the connection records a
  // failed request and throttles subsequent requests for kThrottlingDuration.
  // We advance time past the throttling interval so post-rotation requests
  // are not blocked.
  MockTrustedVaultClientObserver observer;
  base::ScopedObservation<TrustedVaultClient, TrustedVaultClient::Observer>
      observation(&observer);
  observation.Observe(client.get());
  EXPECT_CALL(observer, OnTrustedVaultKeysChanged).Times(0);
  ASSERT_THAT(FetchKeys(client.get(), account_info), IsEmpty());
  testing::Mock::VerifyAndClearExpectations(&observer);
  task_environment_.FastForwardBy(
      TrustedVaultThrottlingConnectionImpl::kThrottlingDuration +
      base::Seconds(1));

  // Mimic user enrolment by rotating the server side key and marking local keys
  // as stale.
  std::vector<uint8_t> new_epoch_key =
      fake_security_domains_server_->RotateTrustedVaultKey(
          GetConstantTrustedVaultKey());
  EXPECT_TRUE(MarkLocalKeysAsStale(client.get(), account_info));

  // `FetchKeys()` should follow the server side key rotation, notify observers,
  // and register local factors if applicable.
  EXPECT_CALL(observer, OnTrustedVaultKeysChanged(testing::Eq(std::nullopt)));
  EXPECT_THAT(FetchKeys(client.get(), account_info),
              ElementsAre(new_epoch_key));
  WaitForIdle(client.get());
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(),
            kDefaultExpectedMemberCount);
  EXPECT_TRUE(fake_security_domains_server_->AllMembersHaveKey(new_epoch_key));
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldResolveErrorAndRegisterFactorsAfterFollowingKeyRotation) {
  const std::vector<uint8_t> kLocalKeyV1 = {1, 1, 1, 1};
  const std::vector<uint8_t> kRemoteKeyV2 = {2, 2, 2, 2};
  const int kLocalEpochV1 = 1;
  const int kRemoteEpochV2 = 2;

  fake_security_domains_server_->ResetDataToState({kLocalKeyV1, kRemoteKeyV2},
                                                  kRemoteEpochV2);

  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  WaitForIdle(client.get());

  MockTrustedVaultClientObserver observer;
  client->AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged);
  StoreKeys(client.get(), account_info.gaia, {kLocalKeyV1}, kLocalEpochV1);
  WaitForIdle(client.get());
  ASSERT_THAT(FetchKeys(client.get(), account_info), ElementsAre(kLocalKeyV1));

  // Attempting to add a recovery method with stale local key fails with
  // kLocalDataObsolete.
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  AddTrustedRecoveryMethod(client.get(), account_info.gaia,
                           key_pair->public_key().ExportToBytes(),
                           /*method_type_hint=*/3);
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(), 0);

  // Storing retrieved remote keys resolves kLocalDataObsolete and
  // auto-registers the default recovery factors.
  EXPECT_CALL(observer, OnTrustedVaultKeysChanged);
  StoreKeys(client.get(), account_info.gaia, {kLocalKeyV1, kRemoteKeyV2},
            kRemoteEpochV2);
  WaitForIdle(client.get());
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(),
            kDefaultExpectedMemberCount);
  EXPECT_TRUE(fake_security_domains_server_->AllMembersHaveKey(kRemoteKeyV2));

  EXPECT_THAT(FetchKeys(client.get(), account_info),
              ElementsAre(kLocalKeyV1, kRemoteKeyV2));
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldReadKeysAndStateFromDiskOnClientRestart) {
  const std::vector<uint8_t> kKey = {5, 6, 7, 8};
  const int kServerEpoch = 10;
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);

  {
    std::unique_ptr<StandaloneTrustedVaultClient> client_a = CreateClient();
    StoreKeys(client_a.get(), account_info.gaia, {kKey}, kServerEpoch);
    WaitForIdle(client_a.get());
  }

  {
    std::unique_ptr<StandaloneTrustedVaultClient> client_b = CreateClient();
    EXPECT_THAT(FetchKeys(client_b.get(), account_info), ElementsAre(kKey));
    EXPECT_EQ(GetLastKeyVersion(client_b.get(), account_info.gaia),
              kServerEpoch);
  }
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldShareFrontendAndRouteDomainNotifications) {
  const std::vector<uint8_t> kSyncKey = {1, 2, 3, 4};
  constexpr int kServerEpoch = 10;
  fake_security_domains_server_->ResetDataToState({kSyncKey}, kServerEpoch);
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);

  scoped_refptr<StandaloneTrustedVaultFrontend> frontend = CreateFrontend();
  std::unique_ptr<StandaloneTrustedVaultClient> sync_client =
      CreateClient(SecurityDomainId::kChromeSync, frontend);
  std::unique_ptr<StandaloneTrustedVaultClient> passkeys_client =
      CreateClient(SecurityDomainId::kPasskeys, frontend);
  WaitForIdle(sync_client.get());

  MockTrustedVaultClientObserver sync_observer;
  MockTrustedVaultClientObserver passkeys_observer;
  sync_client->AddObserver(&sync_observer);
  passkeys_client->AddObserver(&passkeys_observer);

  // Storing keys in `kChromeSync` should notify only `sync_observer` and
  // propagate the user action trigger.
  EXPECT_CALL(sync_observer,
              OnTrustedVaultKeysChanged(
                  Eq(TrustedVaultUserActionTriggerForUMA::kSettings)));
  EXPECT_CALL(passkeys_observer, OnTrustedVaultKeysChanged).Times(0);
  sync_client->StoreKeys(account_info.gaia, {kSyncKey}, kServerEpoch,
                         TrustedVaultUserActionTriggerForUMA::kSettings);
  WaitForIdle(sync_client.get());
  testing::Mock::VerifyAndClearExpectations(&sync_observer);
  testing::Mock::VerifyAndClearExpectations(&passkeys_observer);

  EXPECT_THAT(FetchKeys(sync_client.get(), account_info),
              ElementsAre(kSyncKey));

  // Downloading rotated keys via `FetchKeys()` in `kChromeSync` should notify
  // only `sync_observer`.
  const std::vector<uint8_t> rotated_sync_key =
      fake_security_domains_server_->RotateTrustedVaultKey(kSyncKey);
  EXPECT_TRUE(MarkLocalKeysAsStale(sync_client.get(), account_info));
  EXPECT_CALL(sync_observer,
              OnTrustedVaultKeysChanged(testing::Eq(std::nullopt)));
  EXPECT_CALL(passkeys_observer, OnTrustedVaultKeysChanged).Times(0);
  EXPECT_THAT(FetchKeys(sync_client.get(), account_info),
              ElementsAre(kSyncKey, rotated_sync_key));
  testing::Mock::VerifyAndClearExpectations(&sync_observer);
  testing::Mock::VerifyAndClearExpectations(&passkeys_observer);

  // Recoverability degraded notifications from the backend should be routed to
  // the respective domain client only.
  fake_security_domains_server_->RequirePublicKeyToAvoidRecoverabilityDegraded(
      /*public_key=*/{9, 9, 9, 9});
  base::RunLoop recoverability_run_loop;
  EXPECT_CALL(sync_observer, OnTrustedVaultRecoverabilityChanged)
      .WillOnce(
          [&recoverability_run_loop]() { recoverability_run_loop.Quit(); });
  EXPECT_CALL(passkeys_observer, OnTrustedVaultRecoverabilityChanged).Times(0);
  sync_client->GetIsRecoverabilityDegraded(account_info, base::DoNothing());
  recoverability_run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&sync_observer);
  testing::Mock::VerifyAndClearExpectations(&passkeys_observer);

  // Cookie jar changes in IdentityManager should notify all registered clients.
  EXPECT_CALL(sync_observer, OnTrustedVaultKeysChanged);
  EXPECT_CALL(passkeys_observer, OnTrustedVaultKeysChanged);
  identity_test_env_.SetCookieAccounts({});
  testing::Mock::VerifyAndClearExpectations(&sync_observer);
  testing::Mock::VerifyAndClearExpectations(&passkeys_observer);

  sync_client->RemoveObserver(&sync_observer);
  passkeys_client->RemoveObserver(&passkeys_observer);
}

TEST_F(StandaloneTrustedVaultClientTest, ShouldStoreAndFetchKeysAcrossDomains) {
  const std::vector<uint8_t> kSyncKey = {1, 2, 3, 4};
  const std::vector<uint8_t> kPasskeysKey = {5, 6, 7, 8};
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);

  {
    scoped_refptr<StandaloneTrustedVaultFrontend> frontend = CreateFrontend();
    std::unique_ptr<StandaloneTrustedVaultClient> sync_client =
        CreateClient(SecurityDomainId::kChromeSync, frontend);
    std::unique_ptr<StandaloneTrustedVaultClient> passkeys_client =
        CreateClient(SecurityDomainId::kPasskeys, frontend);
    WaitForIdle(sync_client.get());

    StoreKeys(sync_client.get(), account_info.gaia, {kSyncKey},
              /*last_key_version=*/1);
    StoreKeys(passkeys_client.get(), account_info.gaia, {kPasskeysKey},
              /*last_key_version=*/2);
    WaitForIdle(sync_client.get());

    EXPECT_THAT(FetchKeys(sync_client.get(), account_info),
                ElementsAre(kSyncKey));
    EXPECT_THAT(FetchKeys(passkeys_client.get(), account_info),
                ElementsAre(kPasskeysKey));
  }

  // Verify persistence across frontend/client restart with LocalDomainsStorage.
  {
    scoped_refptr<StandaloneTrustedVaultFrontend> frontend = CreateFrontend();
    std::unique_ptr<StandaloneTrustedVaultClient> sync_client =
        CreateClient(SecurityDomainId::kChromeSync, frontend);
    std::unique_ptr<StandaloneTrustedVaultClient> passkeys_client =
        CreateClient(SecurityDomainId::kPasskeys, frontend);

    EXPECT_THAT(FetchKeys(sync_client.get(), account_info),
                ElementsAre(kSyncKey));
    EXPECT_THAT(FetchKeys(passkeys_client.get(), account_info),
                ElementsAre(kPasskeysKey));
  }
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldMigrateFromLegacyStorageOnRestart) {
  const std::vector<uint8_t> kKey1 = {1, 2, 3, 4};
  const int kServerEpoch = 10;
  fake_security_domains_server_->ResetDataToState({kKey1}, kServerEpoch);

  const base::FilePath legacy_file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("trusted_vault.pb"));
  const base::FilePath new_file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("local_domains_data.pb"));

  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);

  // First run with legacy storage enabled
  // (`kTrustedVaultSharedRecoveryFactorsAndEncryption` disabled).
  {
    base::test::ScopedFeatureList disable_new_storage;
    disable_new_storage.InitAndDisableFeature(
        kTrustedVaultSharedRecoveryFactorsAndEncryption);

    std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
    StoreKeys(client.get(), account_info.gaia, {kKey1}, kServerEpoch);
    WaitForIdle(client.get());

    EXPECT_TRUE(base::PathExists(legacy_file_path));
    EXPECT_FALSE(base::PathExists(new_file_path));
    EXPECT_EQ(fake_security_domains_server_->GetMemberCount(),
              kDefaultExpectedMemberCount);
  }
  task_environment_.RunUntilIdle();

  // Simulate restart with the new encrypted storage format enabled.
  std::unique_ptr<StandaloneTrustedVaultClient> migrated_client =
      CreateClient();
  WaitForIdle(migrated_client.get());

  EXPECT_TRUE(base::PathExists(new_file_path));
  // Cached keys should be immediately returned from migrated storage without
  // re-registering recovery factors.
  EXPECT_THAT(FetchKeys(migrated_client.get(), account_info),
              ElementsAre(kKey1));
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(),
            kDefaultExpectedMemberCount);

  // Verify that the migrated physical device private key can decrypt newly
  // rotated server keys.
  const std::vector<uint8_t> new_epoch_key =
      fake_security_domains_server_->RotateTrustedVaultKey(kKey1);
  EXPECT_TRUE(MarkLocalKeysAsStale(migrated_client.get(), account_info));
  EXPECT_THAT(FetchKeys(migrated_client.get(), account_info),
              ElementsAre(kKey1, new_epoch_key));
}

TEST_F(StandaloneTrustedVaultClientTest, ShouldCheckCanEnrollEligibility) {
  scoped_refptr<StandaloneTrustedVaultFrontend> frontend = CreateFrontend();
  std::unique_ptr<StandaloneTrustedVaultClient> sync_client =
      CreateClient(SecurityDomainId::kChromeSync, frontend);
  std::unique_ptr<StandaloneTrustedVaultClient> passkeys_client =
      CreateClient(SecurityDomainId::kPasskeys, frontend);
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  WaitForIdle(sync_client.get());
  WaitForIdle(passkeys_client.get());

  // Not eligible before a Gaia password public key is available on the server.
  EXPECT_FALSE(CanEnroll(sync_client.get(), account_info));
  EXPECT_FALSE(CanEnroll(passkeys_client.get(), account_info));

  // Eligible for `kChromeSync` once a Gaia password key is available, while
  // `kPasskeys` does not support enrollment yet.
  fake_security_domains_server_->SetGaiaPasswordKeyPair(
      SecureBoxKeyPair::GenerateRandom());
  EXPECT_TRUE(CanEnroll(sync_client.get(), account_info));
  EXPECT_FALSE(CanEnroll(passkeys_client.get(), account_info));
}

TEST_F(StandaloneTrustedVaultClientTest, ShouldEnroll) {
  fake_security_domains_server_->SetGaiaPasswordKeyPair(
      SecureBoxKeyPair::GenerateRandom());
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);

  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  WaitForIdle(client.get());
  ASSERT_TRUE(CanEnroll(client.get(), account_info));

  MockTrustedVaultClientObserver observer;
  base::ScopedObservation<TrustedVaultClient, TrustedVaultClient::Observer>
      observation(&observer);
  observation.Observe(client.get());
  EXPECT_CALL(observer,
              OnTrustedVaultKeysChanged(/*trigger=*/Eq(std::nullopt)));

  const auto [rotated_key, rotated_key_version] =
      Enroll(client.get(), account_info);
  EXPECT_THAT(rotated_key, Not(IsEmpty()));
  EXPECT_EQ(rotated_key_version,
            fake_security_domains_server_->GetCurrentEpoch());
  WaitForIdle(client.get());

  // Default local recovery factors plus the newly registered Gaia password
  // member should all hold `rotated_key`.
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(),
            kDefaultExpectedMemberCount + 1);
  EXPECT_TRUE(fake_security_domains_server_->AllMembersHaveKey(rotated_key));
  EXPECT_FALSE(fake_security_domains_server_->ReceivedInvalidRequest());

  EXPECT_FALSE(CanEnroll(client.get(), account_info));
  EXPECT_THAT(FetchKeys(client.get(), account_info), ElementsAre(rotated_key));
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldFollowKeyRotationOnSecondClientAfterEnroll) {
  const base::FilePath second_client_dir =
      temp_dir_.GetPath().AppendASCII("second_client");
  ASSERT_TRUE(base::CreateDirectory(second_client_dir));
  fake_security_domains_server_->SetGaiaPasswordKeyPair(
      SecureBoxKeyPair::GenerateRandom());
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);

  std::unique_ptr<StandaloneTrustedVaultClient> first_client = CreateClient();
  std::unique_ptr<StandaloneTrustedVaultClient> second_client =
      CreateClient(SecurityDomainId::kChromeSync,
                   CreateFrontendForBaseDir(second_client_dir));
  WaitForIdle(first_client.get());
  WaitForIdle(second_client.get());

  const auto [rotated_key, rotated_key_version] =
      Enroll(first_client.get(), account_info);
  ASSERT_THAT(rotated_key, Not(IsEmpty()));
  WaitForIdle(first_client.get());

  // Simulate a client notifying the second device that its local keys are
  // stale; `FetchKeys()` should download and unwrap `rotated_key`.
  MockTrustedVaultClientObserver second_observer;
  base::ScopedObservation<TrustedVaultClient, TrustedVaultClient::Observer>
      observation(&second_observer);
  observation.Observe(second_client.get());
  EXPECT_CALL(second_observer,
              OnTrustedVaultKeysChanged(/*trigger=*/Eq(std::nullopt)));

  EXPECT_TRUE(MarkLocalKeysAsStale(second_client.get(), account_info));
  EXPECT_THAT(FetchKeys(second_client.get(), account_info),
              ElementsAre(rotated_key));
  WaitForIdle(second_client.get());
  EXPECT_EQ(GetLastKeyVersion(second_client.get(), account_info.gaia),
            rotated_key_version);
  EXPECT_FALSE(CanEnroll(second_client.get(), account_info));
}

}  // namespace
}  // namespace trusted_vault
