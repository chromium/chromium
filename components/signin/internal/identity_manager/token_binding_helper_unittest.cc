// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/token_binding_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/internal/identity_manager/oauth2_upgrade_token_flow.h"
#include "components/signin/public/base/binding_key_registration_token_result.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/mock_unexportable_key_provider.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using GenerateAssertionFuture = base::test::TestFuture<std::string>;
using ::base::test::ErrorIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Return;
using ::unexportable_keys::UnexportableSigningKeyId;

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserVisible;

constexpr char kGenerateAssertionResultHistogram[] =
    "Signin.TokenBinding.GenerateAssertionResult";
}  // namespace

class TokenBindingHelperTest : public testing::Test {
 public:
  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  TokenBindingHelper& helper() { return helper_; }

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  crypto::ScopedMockUnexportableKeyProvider& SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_
        .emplace<crypto::ScopedMockUnexportableKeyProvider>();
  }

  UnexportableSigningKeyId GenerateNewSigningKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<UnexportableSigningKeyId>>
        generate_future;
    unexportable_key_service_.GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    unexportable_keys::ServiceErrorOr<UnexportableSigningKeyId> key_id =
        generate_future.Get();
    CHECK(key_id.has_value());
    return *key_id;
  }

  std::vector<uint8_t> GetWrappedKey(
      unexportable_keys::UnexportableKeyId key_id) {
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        unexportable_key_service_.GetWrappedKey(key_id);
    CHECK(wrapped_key.has_value());
    return *wrapped_key;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      // QUEUED - tasks don't run until `RunUntilIdle()` is called.
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  std::variant<crypto::ScopedFakeUnexportableKeyProvider,
               crypto::ScopedMockUnexportableKeyProvider>
      scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      unexportable_key_task_manager_,
      unexportable_keys::BackgroundTaskOrigin::kRefreshTokenBinding,
      crypto::UnexportableKeyProvider::Config(),
  };
  TokenBindingHelper helper_{unexportable_key_service_};
  base::HistogramTester histogram_tester_;
};

TEST_F(TokenBindingHelperTest, SetBindingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  EXPECT_FALSE(helper().HasBindingKey(account_id));

  helper().SetBindingKey(account_id, wrapped_key);

  EXPECT_TRUE(helper().HasBindingKey(account_id));
  EXPECT_EQ(helper().GetWrappedBindingKey(account_id), wrapped_key);
}

TEST_F(TokenBindingHelperTest, SetBindingKeyToEmpty) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  helper().SetBindingKey(account_id, wrapped_key);

  helper().SetBindingKey(account_id, {});
  EXPECT_FALSE(helper().HasBindingKey(account_id));
}

TEST_F(TokenBindingHelperTest, ClearAllKeys) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  CoreAccountId account_id2 =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  std::vector<uint8_t> wrapped_key2 = GetWrappedKey(GenerateNewSigningKey());
  helper().SetBindingKey(account_id, wrapped_key);
  helper().SetBindingKey(account_id2, wrapped_key2);

  helper().OnAllCredentialsLoaded(true);
  RunBackgroundTasks();
  EXPECT_TRUE(helper().IsRegistrationKeyReady());

  helper().ClearAllKeys();
  EXPECT_FALSE(helper().HasBindingKey(account_id));
  EXPECT_FALSE(helper().HasBindingKey(account_id2));
  EXPECT_FALSE(helper().IsRegistrationKeyReady());
}

TEST_F(TokenBindingHelperTest, GetBoundTokenCount) {
  EXPECT_EQ(helper().GetBoundTokenCount(), 0u);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         GetWrappedKey(GenerateNewSigningKey()));
  EXPECT_EQ(helper().GetBoundTokenCount(), 1u);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2")),
                         GetWrappedKey(GenerateNewSigningKey()));
  EXPECT_EQ(helper().GetBoundTokenCount(), 2u);
}

using TokenBindingHelperAreAllBindingKeysSameTest = TokenBindingHelperTest;

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, TrueIfEmpty) {
  EXPECT_TRUE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, TrueIfOnlyOne) {
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         GetWrappedKey(GenerateNewSigningKey()));
  EXPECT_TRUE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, TrueIfAllSame) {
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id3")),
                         wrapped_key);
  EXPECT_TRUE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, FalseIfDifferent) {
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  // Two accounts share the same key but the third one is different.
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id3")),
                         GetWrappedKey(GenerateNewSigningKey()));
  EXPECT_FALSE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperTest, CopyBindingKeyFromAnotherTokenServiceEmpty) {
  EXPECT_CHECK_DEATH(helper().CopyBindingKeyFromAnotherTokenService({}));
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertion) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  UnexportableSigningKeyId key_id = GenerateNewSigningKey();
  std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);
  helper().SetBindingKey(account_id, wrapped_key);

  GenerateAssertionFuture sign_future;
  HybridEncryptionKey ephemeral_key = CreateHybridEncryptionKeyForTesting();
  helper().GenerateBindingKeyAssertion(
      account_id, "challenge", ephemeral_key.ExportPublicKey(),
      GURL("https://oauth.example.com/IssueToken"), sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(sign_future.Get().empty());

  EXPECT_TRUE(signin::VerifyJwtSignature(
      sign_future.Get(), *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
  histogram_tester().ExpectUniqueSample(kGenerateAssertionResultHistogram,
                                        TokenBindingHelper::kNoErrorForMetrics,
                                        /*expected_bucket_count=*/1);
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertionNoBindingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));

  GenerateAssertionFuture sign_future;
  HybridEncryptionKey ephemeral_key = CreateHybridEncryptionKeyForTesting();
  helper().GenerateBindingKeyAssertion(
      account_id, "challenge", ephemeral_key.ExportPublicKey(),
      GURL("https://oauth.example.com/IssueToken"), sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.Get().empty());
  histogram_tester().ExpectUniqueSample(kGenerateAssertionResultHistogram,
                                        TokenBindingHelper::Error::kKeyNotFound,
                                        /*expected_bucket_count=*/1);
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertionInvalidBindingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  const std::vector<uint8_t> kInvalidWrappedKey = {1, 2, 3};
  helper().SetBindingKey(account_id, kInvalidWrappedKey);

  GenerateAssertionFuture sign_future;
  HybridEncryptionKey ephemeral_key = CreateHybridEncryptionKeyForTesting();
  helper().GenerateBindingKeyAssertion(
      account_id, "challenge", ephemeral_key.ExportPublicKey(),
      GURL("https://oauth.example.com/IssueToken"), sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.Get().empty());
  histogram_tester().ExpectUniqueSample(
      kGenerateAssertionResultHistogram,
      TokenBindingHelper::Error::kLoadKeyFailure,
      /*expected_bucket_count=*/1);
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertionNoEphemeralKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  UnexportableSigningKeyId key_id = GenerateNewSigningKey();
  std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);
  helper().SetBindingKey(account_id, wrapped_key);

  GenerateAssertionFuture sign_future;
  helper().GenerateBindingKeyAssertion(
      account_id, "challenge", /*ephemeral_public_key=*/"",
      GURL("https://oauth.example.com/IssueToken"), sign_future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(sign_future.Get().empty());

  EXPECT_TRUE(signin::VerifyJwtSignature(
      sign_future.Get(), *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
  histogram_tester().ExpectUniqueSample(kGenerateAssertionResultHistogram,
                                        TokenBindingHelper::kNoErrorForMetrics,
                                        /*expected_bucket_count=*/1);
}

TEST_F(TokenBindingHelperTest, StartGarbageCollectionDeletesUnusedKeys) {
  crypto::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  std::vector<uint8_t> used_wrapped_key_in_memory = {1, 2, 3};
  std::vector<uint8_t> used_wrapped_key_in_db = {10, 11, 12};
  std::vector<uint8_t> unused_wrapped_key1 = {4, 5, 6};
  std::vector<uint8_t> unused_wrapped_key2 = {7, 8, 9};
  // This key is unused but created after the current process started.
  std::vector<uint8_t> unused_wrapped_key_new = {13, 14, 15};

  auto create_mock_key = [](const std::vector<uint8_t>& wrapped_key) {
    auto mock_key = std::make_unique<crypto::MockUnexportableSigningKey>();
    ON_CALL(*mock_key, GetWrappedKey).WillByDefault(Return(wrapped_key));
    return mock_key;
  };

  auto used_unexportable_key_in_memory =
      create_mock_key(used_wrapped_key_in_memory);
  auto used_unexportable_key_in_db = create_mock_key(used_wrapped_key_in_db);
  auto unused_unexportable_key1 = create_mock_key(unused_wrapped_key1);
  auto unused_unexportable_key2 = create_mock_key(unused_wrapped_key2);
  auto unused_unexportable_key_new = create_mock_key(unused_wrapped_key_new);
  EXPECT_CALL(*unused_unexportable_key_new, GetCreationTime())
      .WillOnce(Return(base::Time::Now()));

  auto* raw_unused_unexportable_key1 = unused_unexportable_key1.get();
  auto* raw_unused_unexportable_key2 = unused_unexportable_key2.get();

  EXPECT_CALL(mock_key_provider, GetAllKeysSlowly())
      .WillOnce(Return(
          base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
              std::move(used_unexportable_key_in_memory),
              std::move(used_unexportable_key_in_db),
              std::move(unused_unexportable_key1),
              std::move(unused_unexportable_key2),
              std::move(unused_unexportable_key_new),
          })));

  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("account_id")),
                         used_wrapped_key_in_memory);
  helper().StartGarbageCollection({used_wrapped_key_in_db});

  EXPECT_CALL(mock_key_provider,
              DeleteKeysSlowly(ElementsAre(raw_unused_unexportable_key1,
                                           raw_unused_unexportable_key2)))
      .WillOnce(Return(2));
  RunBackgroundTasks();

  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.RefreshTokenBinding."
      "TotalKeyCount",
      5, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.RefreshTokenBinding."
      "UsedKeyCount",
      3, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.RefreshTokenBinding."
      "ObsoleteKeyCount",
      2, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.RefreshTokenBinding."
      "ObsoleteKeyDeletionCount",
      2, 1);
}

TEST_F(TokenBindingHelperTest,
       GenerateBindingKeyRegistrationTokenNoExistingKey) {
  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future;
  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code",
      future.GetCallback());
  RunBackgroundTasks();

  ASSERT_TRUE(future.Get().has_value());
  const signin::BindingKeyRegistrationTokenResult& result = *future.Get();
  EXPECT_FALSE(result.registration_token.empty());
  EXPECT_TRUE(signin::VerifyJwtSignature(
      result.registration_token,
      *unexportable_key_service().GetAlgorithm(result.binding_key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(
          result.binding_key_id)));
}

TEST_F(TokenBindingHelperTest,
       GenerateBindingKeyRegistrationTokenReuseExistingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  helper().SetBindingKey(account_id, wrapped_key);

  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future;
  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code",
      future.GetCallback());
  RunBackgroundTasks();

  ASSERT_TRUE(future.Get().has_value());
  const signin::BindingKeyRegistrationTokenResult& result = *future.Get();
  EXPECT_EQ(result.wrapped_binding_key, wrapped_key);
  EXPECT_FALSE(result.registration_token.empty());
  EXPECT_TRUE(signin::VerifyJwtSignature(
      result.registration_token,
      *unexportable_key_service().GetAlgorithm(result.binding_key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(
          result.binding_key_id)));
}

TEST_F(TokenBindingHelperTest,
       GenerateBindingKeyRegistrationTokenHelperAlreadyExists) {
  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future_1;
  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future_2;

  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code_1",
      future_1.GetCallback());
  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code_2",
      future_2.GetCallback());
  RunBackgroundTasks();

  ASSERT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Get().has_value());
  EXPECT_EQ(future_1.Get()->binding_key_id, future_2.Get()->binding_key_id);
}

TEST_F(TokenBindingHelperTest,
       SetLastBindingKeyToEmptyClearsRegistrationTokenHelper) {
  CoreAccountId account_id_1 =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id_1"));
  CoreAccountId account_id_2 =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id_2"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  helper().SetBindingKey(account_id_1, wrapped_key);
  helper().SetBindingKey(account_id_2, wrapped_key);

  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future_1;
  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future_2;
  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future_3;

  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code_1",
      future_1.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future_1.Get().has_value());
  EXPECT_EQ(future_1.Get()->wrapped_binding_key, wrapped_key);
  EXPECT_TRUE(helper().IsRegistrationKeyReady());

  // Setting the second-to-last key to empty should not clear the helper or
  // change key selection.
  helper().SetBindingKey(account_id_1, {});
  EXPECT_TRUE(helper().IsRegistrationKeyReady());

  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code_2",
      future_2.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future_2.Get().has_value());
  EXPECT_EQ(future_1.Get()->binding_key_id, future_2.Get()->binding_key_id);

  // Setting the last key to empty should clear the helper.
  helper().SetBindingKey(account_id_2, {});
  EXPECT_FALSE(helper().IsRegistrationKeyReady());

  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code_3",
      future_3.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future_3.Get().has_value());
  EXPECT_NE(future_2.Get()->binding_key_id, future_3.Get()->binding_key_id);
  EXPECT_TRUE(helper().IsRegistrationKeyReady());
}

TEST_F(TokenBindingHelperTest,
       SetBindingKeyToEmptyDoesNotClearRegistrationTokenHelper) {
  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future_1;
  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future_2;

  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code_1",
      future_1.GetCallback());

  // Adding an unbound token (setting binding key to empty for an account that
  // didn't have a binding key) should not clear the in-progress registration
  // token helper.
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  helper().SetBindingKey(account_id, {});

  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code_2",
      future_2.GetCallback());
  RunBackgroundTasks();

  ASSERT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Get().has_value());
  EXPECT_EQ(future_1.Get()->binding_key_id, future_2.Get()->binding_key_id);
}

TEST_F(TokenBindingHelperTest,
       RegistrationKeyIsReadyAfterAllCredentialsLoaded) {
  EXPECT_FALSE(helper().IsRegistrationKeyReady());

  helper().OnAllCredentialsLoaded(true);
  EXPECT_FALSE(helper().IsRegistrationKeyReady());

  RunBackgroundTasks();
  EXPECT_TRUE(helper().IsRegistrationKeyReady());
}

TEST_F(TokenBindingHelperTest, OnAllCredentialsLoadedNoRefreshTokens) {
  EXPECT_FALSE(helper().IsRegistrationKeyReady());

  helper().OnAllCredentialsLoaded(/*has_refresh_tokens=*/false);
  RunBackgroundTasks();
  EXPECT_FALSE(helper().IsRegistrationKeyReady());
}

TEST_F(TokenBindingHelperTest,
       OnAllCredentialsLoadedDoesNotGenerateNewKeyIfEmpty) {
  helper().OnAllCredentialsLoaded(true);
  RunBackgroundTasks();
  EXPECT_TRUE(helper().IsRegistrationKeyReady());

  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future;
  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code",
      future.GetCallback());
  RunBackgroundTasks();

  ASSERT_TRUE(future.Get().has_value());
  const signin::BindingKeyRegistrationTokenResult& result = *future.Get();
  EXPECT_FALSE(result.wrapped_binding_key.empty());
}

TEST_F(TokenBindingHelperTest, OnAllCredentialsLoadedReusesExistingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewSigningKey());
  helper().SetBindingKey(account_id, wrapped_key);

  helper().OnAllCredentialsLoaded(true);
  RunBackgroundTasks();
  EXPECT_TRUE(helper().IsRegistrationKeyReady());

  base::test::TestFuture<
      std::optional<signin::BindingKeyRegistrationTokenResult>>
      future;
  helper().GenerateBindingKeyRegistrationToken(
      {crypto::SignatureVerifier::ECDSA_SHA256}, "auth_code",
      future.GetCallback());
  RunBackgroundTasks();

  ASSERT_TRUE(future.Get().has_value());
  const signin::BindingKeyRegistrationTokenResult& result = *future.Get();
  EXPECT_EQ(result.wrapped_binding_key, wrapped_key);
}

class TokenBindingHelperUpgradeTest : public TokenBindingHelperTest {
 public:
  TokenBindingHelperUpgradeTest() {
    ON_CALL(mock_save_callback_, Run).WillByDefault(Return(true));
    helper().SetSaveBindingKeyCallback(mock_save_callback_.Get());
  }

  void StartUpgrade(const CoreAccountId& account_id) {
    helper().PerformTokenBindingUpgrade(
        account_id, "test_token", shared_factory_, "test_device_id",
        "test_challenge", {crypto::SignatureVerifier::ECDSA_SHA256});
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  base::MockCallback<TokenBindingHelper::SaveBindingKeyCallback>&
  mock_save_callback() {
    return mock_save_callback_;
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableChromeRefreshTokenBindingUpgrade};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  base::MockCallback<TokenBindingHelper::SaveBindingKeyCallback>
      mock_save_callback_;
};

TEST_F(TokenBindingHelperUpgradeTest, PerformTokenBindingUpgrade) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  EXPECT_CALL(mock_save_callback(),
              Run(account_id, std::string_view("test_token"), testing::_))
      .WillOnce(Return(true));

  StartUpgrade(account_id);
  RunBackgroundTasks();

  const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
      *test_url_loader_factory()->pending_requests();
  ASSERT_EQ(pending.size(), 1u);
  EXPECT_EQ(pending[0].request.url,
            GaiaUrls::GetInstance()->oauth2_upgrade_token_url());

  test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_upgrade_token_url().spec(), "");

  histogram_tester().ExpectUniqueSample("Signin.TokenBinding.UpgradeHttpResult",
                                        net::HTTP_OK, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.TokenBinding.UpgradeResult",
      signin::OAuth2UpgradeTokenFlowResult::kSuccess, 1);
}

TEST_F(TokenBindingHelperUpgradeTest,
       PerformTokenBindingUpgradeFailedToSaveBindingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  EXPECT_CALL(mock_save_callback(),
              Run(account_id, std::string_view("test_token"), _))
      .WillOnce(Return(false));

  StartUpgrade(account_id);
  RunBackgroundTasks();

  EXPECT_EQ(test_url_loader_factory()->pending_requests()->size(), 0u);

  histogram_tester().ExpectUniqueSample(
      "Signin.TokenBinding.UpgradeResult",
      signin::OAuth2UpgradeTokenFlowResult::kFailedToSaveBindingKey, 1);
  histogram_tester().ExpectTotalCount("Signin.TokenBinding.UpgradeDuration", 1);
}

TEST_F(TokenBindingHelperUpgradeTest,
       PerformTokenBindingUpgradeGenerationFailure) {
  crypto::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();
  // Fail the binding key generation at the first algorithm selection step.
  EXPECT_CALL(mock_key_provider, SelectAlgorithm)
      .WillOnce(Return(std::nullopt));

  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  StartUpgrade(account_id);
  RunBackgroundTasks();

  EXPECT_EQ(test_url_loader_factory()->pending_requests()->size(), 0u);
  histogram_tester().ExpectUniqueSample(
      "Signin.TokenBinding.UpgradeResult",
      signin::OAuth2UpgradeTokenFlowResult::kTokenGenerationFailure, 1);
  histogram_tester().ExpectTotalCount("Signin.TokenBinding.UpgradeDuration", 1);
}

TEST_F(TokenBindingHelperUpgradeTest, DeduplicateUpgradeRequests) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  StartUpgrade(account_id);
  // Calling StartUpgrade again immediately for the same account should be
  // deduplicated.
  StartUpgrade(account_id);
  RunBackgroundTasks();

  EXPECT_EQ(test_url_loader_factory()->pending_requests()->size(), 1u);
}

TEST_F(TokenBindingHelperUpgradeTest, MultipleAccountsConcurrentUpgrades) {
  CoreAccountId account_id_1 =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id_1"));
  CoreAccountId account_id_2 =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id_2"));
  StartUpgrade(account_id_1);
  StartUpgrade(account_id_2);
  RunBackgroundTasks();

  EXPECT_EQ(test_url_loader_factory()->pending_requests()->size(), 2u);
}

TEST_F(TokenBindingHelperUpgradeTest, SubsequentUpgradeAfterCompletion) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  StartUpgrade(account_id);
  RunBackgroundTasks();

  ASSERT_EQ(test_url_loader_factory()->pending_requests()->size(), 1u);
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_upgrade_token_url().spec(), "");

  // Calling StartUpgrade again after completion should start a new request.
  StartUpgrade(account_id);
  RunBackgroundTasks();

  EXPECT_EQ(test_url_loader_factory()->pending_requests()->size(), 1u);
}
