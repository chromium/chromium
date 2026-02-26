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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/mock_unexportable_key_provider.h"
#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using GenerateAssertionFuture = base::test::TestFuture<std::string>;
using ::base::test::ErrorIs;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Return;

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

  unexportable_keys::ScopedMockUnexportableKeyProvider&
  SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_
        .emplace<unexportable_keys::ScopedMockUnexportableKeyProvider>();
  }

  unexportable_keys::UnexportableKeyId GenerateNewKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
        generate_future;
    unexportable_key_service_.GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        key_id = generate_future.Get();
    CHECK(key_id.has_value());
    return *key_id;
  }

  std::vector<uint8_t> GetWrappedKey(
      const unexportable_keys::UnexportableKeyId& key_id) {
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
               unexportable_keys::ScopedMockUnexportableKeyProvider>
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
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  EXPECT_FALSE(helper().HasBindingKey(account_id));

  helper().SetBindingKey(account_id, wrapped_key);

  EXPECT_TRUE(helper().HasBindingKey(account_id));
  EXPECT_EQ(helper().GetWrappedBindingKey(account_id), wrapped_key);
}

TEST_F(TokenBindingHelperTest, SetBindingKeyToEmpty) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  helper().SetBindingKey(account_id, wrapped_key);

  helper().SetBindingKey(account_id, {});
  EXPECT_FALSE(helper().HasBindingKey(account_id));
}

TEST_F(TokenBindingHelperTest, ClearAllKeys) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  CoreAccountId account_id2 =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2"));
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  std::vector<uint8_t> wrapped_key2 = GetWrappedKey(GenerateNewKey());
  helper().SetBindingKey(account_id, wrapped_key);
  helper().SetBindingKey(account_id2, wrapped_key2);

  helper().ClearAllKeys();
  EXPECT_FALSE(helper().HasBindingKey(account_id));
  EXPECT_FALSE(helper().HasBindingKey(account_id2));
}

TEST_F(TokenBindingHelperTest, GetBoundTokenCount) {
  EXPECT_EQ(helper().GetBoundTokenCount(), 0u);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         GetWrappedKey(GenerateNewKey()));
  EXPECT_EQ(helper().GetBoundTokenCount(), 1u);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2")),
                         GetWrappedKey(GenerateNewKey()));
  EXPECT_EQ(helper().GetBoundTokenCount(), 2u);
}

using TokenBindingHelperAreAllBindingKeysSameTest = TokenBindingHelperTest;

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, TrueIfEmpty) {
  EXPECT_TRUE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, TrueIfOnlyOne) {
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         GetWrappedKey(GenerateNewKey()));
  EXPECT_TRUE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, TrueIfAllSame) {
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id3")),
                         wrapped_key);
  EXPECT_TRUE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperAreAllBindingKeysSameTest, FalseIfDifferent) {
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  // Two accounts share the same key but the third one is different.
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id2")),
                         wrapped_key);
  helper().SetBindingKey(CoreAccountId::FromGaiaId(GaiaId("test_gaia_id3")),
                         GetWrappedKey(GenerateNewKey()));
  EXPECT_FALSE(helper().AreAllBindingKeysSame());
}

TEST_F(TokenBindingHelperTest, CopyBindingKeyFromAnotherTokenServiceEmpty) {
  EXPECT_CHECK_DEATH(helper().CopyBindingKeyFromAnotherTokenService({}));
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertion) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
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
  unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
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
  unexportable_keys::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  std::vector<uint8_t> used_wrapped_key_in_memory = {1, 2, 3};
  std::vector<uint8_t> used_wrapped_key_in_db = {10, 11, 12};
  std::vector<uint8_t> unused_wrapped_key1 = {4, 5, 6};
  std::vector<uint8_t> unused_wrapped_key2 = {7, 8, 9};
  // This key is unused but created after the current process started.
  std::vector<uint8_t> unused_wrapped_key_new = {13, 14, 15};

  auto create_mock_key = [](const std::vector<uint8_t>& wrapped_key) {
    auto mock_key = std::make_unique<unexportable_keys::MockUnexportableKey>();
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

  EXPECT_CALL(mock_key_provider, GetAllSigningKeysSlowly())
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
              DeleteSigningKeysSlowly(ElementsAre(
                  raw_unused_unexportable_key1, raw_unused_unexportable_key2)))
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
