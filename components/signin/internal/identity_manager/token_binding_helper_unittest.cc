// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/token_binding_helper.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserVisible;
}  // namespace

class TokenBindingHelperTest : public testing::Test {
 public:
  TokenBindingHelperTest()
      : unexportable_key_service_(unexportable_key_task_manager_),
        helper_(unexportable_key_service_) {}

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  TokenBindingHelper& helper() { return helper_; }

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
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
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
  TokenBindingHelper helper_;
};

TEST_F(TokenBindingHelperTest, SetBindingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId("test_gaia_id");
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  EXPECT_FALSE(helper().HasBindingKey(account_id));

  helper().SetBindingKey(account_id, wrapped_key);

  EXPECT_TRUE(helper().HasBindingKey(account_id));
  EXPECT_EQ(helper().GetWrappedBindingKey(account_id), wrapped_key);
}

TEST_F(TokenBindingHelperTest, SetBindingKeyToEmpty) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId("test_gaia_id");
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  helper().SetBindingKey(account_id, wrapped_key);

  helper().SetBindingKey(account_id, {});
  EXPECT_FALSE(helper().HasBindingKey(account_id));
}

TEST_F(TokenBindingHelperTest, ClearAllKeys) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId("test_gaia_id");
  CoreAccountId account_id2 = CoreAccountId::FromGaiaId("test_gaia_id2");
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  std::vector<uint8_t> wrapped_key2 = GetWrappedKey(GenerateNewKey());
  helper().SetBindingKey(account_id, wrapped_key);
  helper().SetBindingKey(account_id2, wrapped_key2);

  helper().ClearAllKeys();
  EXPECT_FALSE(helper().HasBindingKey(account_id));
  EXPECT_FALSE(helper().HasBindingKey(account_id2));
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertion) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId("test_gaia_id");
  unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);
  helper().SetBindingKey(account_id, wrapped_key);

  base::test::TestFuture<std::string> sign_future;
  helper().GenerateBindingKeyAssertion(
      account_id, "challenge", GURL("https://oauth.example.com/IssueToken"),
      sign_future.GetCallback());
  RunBackgroundTasks();
  std::string assertion = sign_future.Get();
  EXPECT_FALSE(assertion.empty());

  EXPECT_TRUE(signin::VerifyJwtSignature(
      assertion, *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertionNoBindingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId("test_gaia_id");

  base::test::TestFuture<std::string> sign_future;
  helper().GenerateBindingKeyAssertion(
      account_id, "challenge", GURL("https://oauth.example.com/IssueToken"),
      sign_future.GetCallback());
  RunBackgroundTasks();
  std::string assertion = sign_future.Get();
  EXPECT_TRUE(assertion.empty());
}

TEST_F(TokenBindingHelperTest, GenerateBindingKeyAssertionInvalidBindingKey) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId("test_gaia_id");
  const std::vector<uint8_t> kInvalidWrappedKey = {1, 2, 3};
  helper().SetBindingKey(account_id, kInvalidWrappedKey);

  base::test::TestFuture<std::string> sign_future;
  helper().GenerateBindingKeyAssertion(
      account_id, "challenge", GURL("https://oauth.example.com/IssueToken"),
      sign_future.GetCallback());
  RunBackgroundTasks();
  std::string assertion = sign_future.Get();
  EXPECT_TRUE(assertion.empty());
}
