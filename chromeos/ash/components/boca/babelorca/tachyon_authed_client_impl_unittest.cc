// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_manager.h"
#include "chromeos/ash/components/boca/babelorca/proto/testing_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kOAuthToken1[] = "oauth-token1";
constexpr char kOAuthToken2[] = "oauth-token2";
constexpr int kMaxRetries = 2;
constexpr char kUrl[] = "https:://test.com";

class TachyonAuthedClientImplTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_client_ = std::make_unique<FakeTachyonClient>();
    fake_client_ptr_ = fake_client_.get();
    request_message_ = std::make_unique<TestingMessage>();
    request_message_->set_int_field(1234);
    request_string_ = request_message_->SerializeAsString();
  }

  void CreateAuthedClient() {
    authed_client_ = std::make_unique<TachyonAuthedClientImpl>(
        std::move(fake_client_), &fake_token_manager_);
  }

  TachyonAuthedClientImpl* authed_client() { return authed_client_.get(); }

  FakeTachyonClient* fake_client_ptr() { return fake_client_ptr_; }

  FakeTokenManager* fake_token_manager() { return &fake_token_manager_; }

  std::unique_ptr<TestingMessage> request_message() {
    return std::move(request_message_);
  }

  const std::string& request_string() { return request_string_; }

  std::unique_ptr<RequestDataWrapper> request_data_wrapper() {
    return std::make_unique<RequestDataWrapper>(TRAFFIC_ANNOTATION_FOR_TESTS,
                                                kUrl, kMaxRetries,
                                                test_future_.GetCallback());
  }

  base::test::TestFuture<TachyonResponse>* test_future() {
    return &test_future_;
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<TachyonAuthedClientImpl> authed_client_;
  std::unique_ptr<FakeTachyonClient> fake_client_;
  raw_ptr<FakeTachyonClient> fake_client_ptr_;
  FakeTokenManager fake_token_manager_;
  std::unique_ptr<TestingMessage> request_message_;
  std::string request_string_;
  base::test::TestFuture<TachyonResponse> test_future_;
};

TEST_F(TachyonAuthedClientImplTest, InitiallyAuthed) {
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken1));
  fake_token_manager()->SetFetchedVersion(1);

  CreateAuthedClient();
  authed_client()->StartAuthedRequest(request_data_wrapper(),
                                      request_message());
  fake_client_ptr()->WaitForRequest();

  EXPECT_THAT(fake_client_ptr()->GetOAuthToken(), testing::StrEq(kOAuthToken1));

  auto* request_data = fake_client_ptr()->GetRequestData();
  ASSERT_THAT(request_data, testing::NotNull());
  EXPECT_EQ(request_data->max_retries, kMaxRetries);
  EXPECT_EQ(request_data->oauth_retry_num, 0);
  EXPECT_EQ(request_data->oauth_version, 1);
  EXPECT_THAT(request_data->url, testing::StrEq(kUrl));
  EXPECT_EQ(request_data->content_data, request_string());
}

TEST_F(TachyonAuthedClientImplTest, InitiallyAuthedRequestString) {
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken1));
  fake_token_manager()->SetFetchedVersion(1);

  CreateAuthedClient();
  authed_client()->StartAuthedRequestString(
      request_data_wrapper(), request_message()->SerializeAsString());
  fake_client_ptr()->WaitForRequest();

  EXPECT_THAT(fake_client_ptr()->GetOAuthToken(), testing::StrEq(kOAuthToken1));

  auto* request_data = fake_client_ptr()->GetRequestData();
  ASSERT_THAT(request_data, testing::NotNull());
  EXPECT_EQ(request_data->max_retries, kMaxRetries);
  EXPECT_EQ(request_data->oauth_retry_num, 0);
  EXPECT_EQ(request_data->oauth_version, 1);
  EXPECT_THAT(request_data->url, testing::StrEq(kUrl));
  EXPECT_EQ(request_data->content_data, request_string());
}

TEST_F(TachyonAuthedClientImplTest, NotInitiallyAuthed) {
  CreateAuthedClient();
  authed_client()->StartAuthedRequest(request_data_wrapper(),
                                      request_message());
  fake_token_manager()->WaitForForceFetchRequest();
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken1));
  fake_token_manager()->SetFetchedVersion(1);
  fake_token_manager()->ExecuteFetchCallback(/*success=*/true);
  fake_client_ptr()->WaitForRequest();

  EXPECT_THAT(fake_client_ptr()->GetOAuthToken(), testing::StrEq(kOAuthToken1));

  auto* request_data = fake_client_ptr()->GetRequestData();
  ASSERT_THAT(request_data, testing::NotNull());
  EXPECT_EQ(request_data->max_retries, kMaxRetries);
  EXPECT_EQ(request_data->oauth_retry_num, 0);
  EXPECT_EQ(request_data->oauth_version, 1);
  EXPECT_THAT(request_data->url, testing::StrEq(kUrl));
  EXPECT_EQ(request_data->content_data, request_string());
}

TEST_F(TachyonAuthedClientImplTest, AuthFailRetryNewFetch) {
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken1));
  fake_token_manager()->SetFetchedVersion(1);

  CreateAuthedClient();
  authed_client()->StartAuthedRequest(request_data_wrapper(),
                                      request_message());
  fake_client_ptr()->WaitForRequest();
  fake_client_ptr()->ExecuteAuthFailCb();
  fake_token_manager()->WaitForForceFetchRequest();
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken2));
  fake_token_manager()->SetFetchedVersion(2);
  fake_token_manager()->ExecuteFetchCallback(/*success=*/true);
  fake_client_ptr()->WaitForRequest();

  EXPECT_THAT(fake_client_ptr()->GetOAuthToken(), testing::StrEq(kOAuthToken2));

  auto* request_data = fake_client_ptr()->GetRequestData();
  ASSERT_THAT(request_data, testing::NotNull());
  EXPECT_EQ(request_data->max_retries, kMaxRetries);
  EXPECT_EQ(request_data->oauth_retry_num, 1);
  EXPECT_EQ(request_data->oauth_version, 2);
  EXPECT_THAT(request_data->url, testing::StrEq(kUrl));
  EXPECT_EQ(request_data->content_data, request_string());
}

TEST_F(TachyonAuthedClientImplTest, AuthFailRetryAlreadyFetched) {
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken1));
  fake_token_manager()->SetFetchedVersion(1);

  CreateAuthedClient();
  authed_client()->StartAuthedRequest(request_data_wrapper(),
                                      request_message());
  fake_client_ptr()->WaitForRequest();
  // Simulate new token fetched before auth failure callback.
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken2));
  fake_token_manager()->SetFetchedVersion(2);
  fake_client_ptr()->ExecuteAuthFailCb();
  fake_client_ptr()->WaitForRequest();

  EXPECT_THAT(fake_client_ptr()->GetOAuthToken(), testing::StrEq(kOAuthToken2));

  auto* request_data = fake_client_ptr()->GetRequestData();
  ASSERT_THAT(request_data, testing::NotNull());
  EXPECT_EQ(request_data->max_retries, kMaxRetries);
  EXPECT_EQ(request_data->oauth_retry_num, 1);
  EXPECT_EQ(request_data->oauth_version, 2);
  EXPECT_THAT(request_data->url, testing::StrEq(kUrl));
  EXPECT_EQ(request_data->content_data, request_string());
}

TEST_F(TachyonAuthedClientImplTest, AuthRetryFailed) {
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken1));
  fake_token_manager()->SetFetchedVersion(1);

  CreateAuthedClient();
  authed_client()->StartAuthedRequest(request_data_wrapper(),
                                      request_message());
  fake_client_ptr()->WaitForRequest();
  // Simulate new token fetched before auth failure callback.
  fake_token_manager()->SetTokenString(
      std::make_unique<std::string>(kOAuthToken2));
  fake_token_manager()->SetFetchedVersion(2);
  fake_client_ptr()->ExecuteAuthFailCb();
  fake_client_ptr()->WaitForRequest();
  fake_client_ptr()->ExecuteAuthFailCb();

  EXPECT_EQ(test_future()->Get().status(), TachyonResponse::Status::kAuthError);
}

TEST_F(TachyonAuthedClientImplTest, TokenFetchFailed) {
  CreateAuthedClient();
  authed_client()->StartAuthedRequest(request_data_wrapper(),
                                      request_message());
  fake_token_manager()->WaitForForceFetchRequest();
  fake_token_manager()->ExecuteFetchCallback(/*success=*/false);

  EXPECT_EQ(test_future()->Get().status(), TachyonResponse::Status::kAuthError);
}

}  // namespace
}  // namespace ash::babelorca
