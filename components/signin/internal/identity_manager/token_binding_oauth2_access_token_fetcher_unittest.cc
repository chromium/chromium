// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/token_binding_oauth2_access_token_fetcher.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"
#include "google_apis/gaia/oauth2_mint_access_token_fetcher_adapter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

constexpr char kClientId[] = "test_client_id";
constexpr char kClientSecret[] = "test_client_secret";
constexpr char kScope[] = "test_scope";
constexpr char kAssertion[] = "test_assertion";

class MockOAuth2MintAccessTokenFetcherAdapter
    : public OAuth2MintAccessTokenFetcherAdapter {
 public:
  explicit MockOAuth2MintAccessTokenFetcherAdapter()
      : OAuth2MintAccessTokenFetcherAdapter(nullptr,
                                            nullptr,
                                            "",
                                            "",
                                            "",
                                            "",
                                            "") {}

  MOCK_METHOD(void,
              Start,
              (const std::string& client_id,
               const std::string& client_secret,
               const std::vector<std::string>& scopes),
              (override));
  MOCK_METHOD(void, CancelRequest, (), (override));
  MOCK_METHOD(void,
              SetBindingKeyAssertion,
              (std::string assertion),
              (override));
  MOCK_METHOD(void, SetTokenDecryptor, (TokenDecryptor decryptor), (override));
};

class TokenBindingOAuth2AccessTokenFetcherTest : public testing::Test {
 public:
  TokenBindingOAuth2AccessTokenFetcherTest() {
    auto internal_fetcher = std::make_unique<
        testing::StrictMock<MockOAuth2MintAccessTokenFetcherAdapter>>();
    mock_internal_fetcher_ = internal_fetcher.get();
    fetcher_ = std::make_unique<TokenBindingOAuth2AccessTokenFetcher>(
        std::move(internal_fetcher));
  }

  TokenBindingOAuth2AccessTokenFetcher* fetcher() { return fetcher_.get(); }

  MockOAuth2MintAccessTokenFetcherAdapter* mock_internal_fetcher() {
    return mock_internal_fetcher_;
  }

 private:
  std::unique_ptr<TokenBindingOAuth2AccessTokenFetcher> fetcher_;
  raw_ptr<testing::StrictMock<MockOAuth2MintAccessTokenFetcherAdapter>>
      mock_internal_fetcher_;
};

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, StartThenSetAssertion) {
  EXPECT_CALL(*mock_internal_fetcher(), Start).Times(0);
  fetcher()->Start(kClientId, kClientSecret, {kScope});
  testing::Mock::VerifyAndClearExpectations(mock_internal_fetcher());

  {
    testing::InSequence s;
    EXPECT_CALL(*mock_internal_fetcher(), SetBindingKeyAssertion(kAssertion));
    EXPECT_CALL(
        *mock_internal_fetcher(),
        Start(kClientId, kClientSecret, std::vector<std::string>({kScope})));
  }
  fetcher()->SetBindingKeyAssertion(kAssertion, /*ephemeral_key=*/std::nullopt);
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, EmptyAssertion) {
  EXPECT_CALL(*mock_internal_fetcher(), Start).Times(0);
  fetcher()->Start(kClientId, kClientSecret, {kScope});
  testing::Mock::VerifyAndClearExpectations(mock_internal_fetcher());

  {
    testing::InSequence s;
    // Some non-empty assertion should provided to the internal fetcher for the
    // dark launch.
    EXPECT_CALL(*mock_internal_fetcher(),
                SetBindingKeyAssertion(testing::Not(testing::IsEmpty())));
    EXPECT_CALL(
        *mock_internal_fetcher(),
        Start(kClientId, kClientSecret, std::vector<std::string>({kScope})));
  }
  fetcher()->SetBindingKeyAssertion(std::string(),
                                    /*ephemeral_key=*/std::nullopt);
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, SetAssertionThenStart) {
  EXPECT_CALL(*mock_internal_fetcher(), SetBindingKeyAssertion(kAssertion));
  EXPECT_CALL(*mock_internal_fetcher(), Start).Times(0);
  fetcher()->SetBindingKeyAssertion(kAssertion, /*ephemeral_key=*/std::nullopt);
  testing::Mock::VerifyAndClearExpectations(mock_internal_fetcher());

  EXPECT_CALL(
      *mock_internal_fetcher(),
      Start(kClientId, kClientSecret, std::vector<std::string>({kScope})));
  fetcher()->Start(kClientId, kClientSecret, {kScope});
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, CancelRequest) {
  EXPECT_CALL(*mock_internal_fetcher(), CancelRequest());
  fetcher()->CancelRequest();
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, SetAssertionWithEphemeralKey) {
  HybridEncryptionKey ephemeral_key = CreateHybridEncryptionKeyForTesting();
  const std::vector<uint8_t> plaintext = {1, 42, 0, 255};
  const std::vector<uint8_t> encrypted_data =
      ephemeral_key.EncryptForTesting(plaintext);

  OAuth2MintAccessTokenFetcherAdapter::TokenDecryptor decryptor;
  EXPECT_CALL(*mock_internal_fetcher(), SetBindingKeyAssertion(kAssertion));
  EXPECT_CALL(*mock_internal_fetcher(), SetTokenDecryptor)
      .WillOnce(testing::SaveArg<0>(&decryptor));
  fetcher()->SetBindingKeyAssertion(kAssertion, std::move(ephemeral_key));

  ASSERT_TRUE(!decryptor.is_null());
  // `decryptor` should transform `encrypted_data` back to `plaintext`.
  EXPECT_EQ(decryptor.Run(base::as_string_view(encrypted_data)),
            base::as_string_view(plaintext));
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest,
       EmptyAssertionWithEphemeralKey) {
  EXPECT_CALL(*mock_internal_fetcher(),
              SetBindingKeyAssertion(testing::Not(testing::IsEmpty())));
  // Ephemeral key should be ignored if the assertion is empty.
  EXPECT_CALL(*mock_internal_fetcher(), SetTokenDecryptor).Times(0);
  fetcher()->SetBindingKeyAssertion(std::string(),
                                    CreateHybridEncryptionKeyForTesting());
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, TokenDecryptorFails) {
  HybridEncryptionKey ephemeral_key = CreateHybridEncryptionKeyForTesting();

  OAuth2MintAccessTokenFetcherAdapter::TokenDecryptor decryptor;
  EXPECT_CALL(*mock_internal_fetcher(), SetBindingKeyAssertion(kAssertion));
  EXPECT_CALL(*mock_internal_fetcher(), SetTokenDecryptor)
      .WillOnce(testing::SaveArg<0>(&decryptor));
  fetcher()->SetBindingKeyAssertion(kAssertion, std::move(ephemeral_key));

  ASSERT_TRUE(!decryptor.is_null());
  // `decryptor` should return an empty string if decryption fails.
  constexpr std::string_view kBogusEncryptedToken = "123";
  EXPECT_EQ(decryptor.Run(kBogusEncryptedToken), std::string());
}

}  // namespace
