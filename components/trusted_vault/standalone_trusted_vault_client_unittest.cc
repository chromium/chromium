// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_client.h"

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
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/test/fake_security_domains_server.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_throttling_connection_impl.h"
#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif
#include "google_apis/gaia/gaia_id.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;

constexpr char kTestEmail[] = "test@gmail.com";

#if BUILDFLAG(IS_MAC)
constexpr char kKeychainAccessGroupPrefix[] = "test-access-group-prefix";
const std::string kKeychainAccessGroup(
    base::StrCat({kKeychainAccessGroupPrefix, ".com.google.common.folsom"}));
// On Mac, both the physical device recovery factor and the iCloud Keychain
// recovery factor are registered by default.
constexpr int kDefaultExpectedMemberCount = 2;
#else
constexpr int kDefaultExpectedMemberCount = 1;
#endif

class FakeSecurityDomainsServerMemberStatusChecker
    : public FakeSecurityDomainsServer::Observer {
 public:
  FakeSecurityDomainsServerMemberStatusChecker(
      FakeSecurityDomainsServer* server,
      int expected_member_count,
      std::optional<std::vector<uint8_t>> expected_trusted_vault_key =
          std::nullopt)
      : server_(server),
        expected_member_count_(expected_member_count),
        expected_trusted_vault_key_(expected_trusted_vault_key) {
    server_->AddObserver(this);
  }

  ~FakeSecurityDomainsServerMemberStatusChecker() override {
    server_->RemoveObserver(this);
  }

  void OnRequestHandled() override {
    if (CheckCondition()) {
      run_loop_.Quit();
    }
  }

  void Wait() {
    if (CheckCondition()) {
      return;
    }
    run_loop_.Run();
  }

 private:
  bool CheckCondition() const {
    if (server_->GetMemberCount() != expected_member_count_) {
      return false;
    }
    if (expected_trusted_vault_key_.has_value() &&
        !server_->AllMembersHaveKey(*expected_trusted_vault_key_)) {
      return false;
    }
    return true;
  }

  const raw_ptr<FakeSecurityDomainsServer> server_;
  const int expected_member_count_;
  const std::optional<std::vector<uint8_t>> expected_trusted_vault_key_;
  base::RunLoop run_loop_;
};

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
//      TrustedVaultConnection boundary using FakeSecurityDomainsServer.
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
  StandaloneTrustedVaultClientTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        test_server_(net::EmbeddedTestServer::TYPE_HTTP) {}

  ~StandaloneTrustedVaultClientTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(test_server_.InitializeAndListen());

    fake_security_domains_server_ =
        std::make_unique<FakeSecurityDomainsServer>(test_server_.base_url());
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeSecurityDomainsServer::HandleRequest,
        base::Unretained(fake_security_domains_server_.get())));
    test_server_.StartAcceptingConnections();

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kTrustedVaultServiceURLSwitch,
        FakeSecurityDomainsServer::GetServerURL(test_server_.base_url())
            .spec());

    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDown() override {
    ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        kTrustedVaultServiceURLSwitch);
    // Make sure to run all pending deletions.
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<StandaloneTrustedVaultClient> CreateClient() {
    return std::make_unique<StandaloneTrustedVaultClient>(
#if BUILDFLAG(IS_MAC)
        kKeychainAccessGroupPrefix,
#endif
        SecurityDomainId::kChromeSync,
        /*base_dir=*/temp_dir_.GetPath(), identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  }

  CoreAccountInfo MakeAccountAvailable(const std::string& email) {
    return identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  void WaitForFlush(StandaloneTrustedVaultClient* client) {
    base::test::TestFuture<void> future;
    client->WaitForFlushForTesting(future.GetCallback());
    ASSERT_TRUE(future.Wait());
    // Required on MacOS to wait for asynchronous interactions with the fake
    // Keychain.
    task_environment_.RunUntilIdle();
  }

  void WaitForServerMembers(StandaloneTrustedVaultClient* client,
                            int expected_member_count,
                            const std::optional<std::vector<uint8_t>>&
                                expected_trusted_vault_key = std::nullopt) {
    WaitForFlush(client);
    FakeSecurityDomainsServerMemberStatusChecker checker(
        fake_security_domains_server_.get(), expected_member_count,
        expected_trusted_vault_key);
    checker.Wait();
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
    base::test::TestFuture<void> future;
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

 protected:
#if BUILDFLAG(IS_MAC)
  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{kKeychainAccessGroup};
#endif
  base::test::TaskEnvironment task_environment_;
  net::EmbeddedTestServer test_server_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FakeSecurityDomainsServer> fake_security_domains_server_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(StandaloneTrustedVaultClientTest, ShouldPreEnrollOnStartup) {
  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  // During pre-enrollment (when only constant keys exist), iCloud Keychain
  // recovery factor registration is not supported, so only physical device
  // recovery factor is registered.
  WaitForServerMembers(client.get(), /*expected_member_count=*/1,
                       GetConstantTrustedVaultKey());

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
  WaitForFlush(client.get());

  // Background pre-enrollment registration was rejected with
  // kLocalDataObsolete.
  EXPECT_EQ(fake_security_domains_server_->GetMemberCount(), 0);
  EXPECT_THAT(FetchKeys(client.get(), account_info), IsEmpty());

  // Fallback key retrieval via StoreKeys resolves error and registers factors.
  MockTrustedVaultClientObserver observer;
  client->AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged);
  StoreKeys(client.get(), account_info.gaia, {kServerKey}, kServerEpoch);
  WaitForServerMembers(client.get(), kDefaultExpectedMemberCount, kServerKey);

  EXPECT_THAT(FetchKeys(client.get(), account_info), ElementsAre(kServerKey));
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldFollowKeyRotationOnUserEnrolment) {
  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  WaitForServerMembers(client.get(), /*expected_member_count=*/1,
                       GetConstantTrustedVaultKey());

  // FetchKeys() returns empty when no non-constant keys exist on the server.
  // Because the server responds with kNoNewKeys, the connection records a
  // failed request and throttles subsequent requests for kThrottlingDuration.
  // We advance time past the throttling interval so post-rotation requests
  // are not blocked.
  ASSERT_THAT(FetchKeys(client.get(), account_info), IsEmpty());
  task_environment_.FastForwardBy(
      TrustedVaultThrottlingConnectionImpl::kThrottlingDuration +
      base::Seconds(1));

  // Mimic user enrolment by rotating the server side key and marking local keys
  // as stale.
  std::vector<uint8_t> new_epoch_key =
      fake_security_domains_server_->RotateTrustedVaultKey(
          GetConstantTrustedVaultKey());
  EXPECT_TRUE(MarkLocalKeysAsStale(client.get(), account_info));

  // TODO(crbug.com/541108964): consider notifying observers when FetchKeys()
  // downloads new keys from the server so that observer expectations can be
  // tested here.
  // FetchKeys() should follow the server side key rotation and register local
  // factors if applicable.
  EXPECT_THAT(FetchKeys(client.get(), account_info),
              ElementsAre(new_epoch_key));
  WaitForServerMembers(client.get(), kDefaultExpectedMemberCount,
                       new_epoch_key);
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
  WaitForFlush(client.get());

  MockTrustedVaultClientObserver observer;
  client->AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged);
  StoreKeys(client.get(), account_info.gaia, {kLocalKeyV1}, kLocalEpochV1);
  WaitForFlush(client.get());
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
  WaitForServerMembers(client.get(), kDefaultExpectedMemberCount, kRemoteKeyV2);

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
    WaitForFlush(client_a.get());
  }

  {
    std::unique_ptr<StandaloneTrustedVaultClient> client_b = CreateClient();
    EXPECT_THAT(FetchKeys(client_b.get(), account_info), ElementsAre(kKey));
    EXPECT_EQ(GetLastKeyVersion(client_b.get(), account_info.gaia),
              kServerEpoch);
  }
}

}  // namespace
}  // namespace trusted_vault
