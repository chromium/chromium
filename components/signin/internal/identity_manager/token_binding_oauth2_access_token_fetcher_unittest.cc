// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/token_binding_oauth2_access_token_fetcher.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
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
      : OAuth2MintAccessTokenFetcherAdapter(nullptr, nullptr, "", "", "", "") {}

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
};

class TokenBindingOAuth2AccessTokenFetcherTest : public testing::Test {
 public:
  TokenBindingOAuth2AccessTokenFetcherTest() {
    auto internal_fetcher =
        std::make_unique<MockOAuth2MintAccessTokenFetcherAdapter>();
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
  raw_ptr<MockOAuth2MintAccessTokenFetcherAdapter> mock_internal_fetcher_;
};

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, StartThenSetAssertion) {
  EXPECT_CALL(*mock_internal_fetcher(), Start(_, _, _)).Times(0);
  fetcher()->Start(kClientId, kClientSecret, {kScope});
  testing::Mock::VerifyAndClearExpectations(mock_internal_fetcher());

  {
    testing::InSequence s;
    EXPECT_CALL(*mock_internal_fetcher(), SetBindingKeyAssertion(kAssertion));
    EXPECT_CALL(
        *mock_internal_fetcher(),
        Start(kClientId, kClientSecret, std::vector<std::string>({kScope})));
  }
  fetcher()->SetBindingKeyAssertion(kAssertion);
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, EmptyAssertion) {
  EXPECT_CALL(*mock_internal_fetcher(), Start(_, _, _)).Times(0);
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
  fetcher()->SetBindingKeyAssertion(std::string());
}

TEST_F(TokenBindingOAuth2AccessTokenFetcherTest, SetAssertionThenStart) {
  EXPECT_CALL(*mock_internal_fetcher(), SetBindingKeyAssertion(kAssertion));
  EXPECT_CALL(*mock_internal_fetcher(), Start(_, _, _)).Times(0);
  fetcher()->SetBindingKeyAssertion(kAssertion);
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

}  // namespace
