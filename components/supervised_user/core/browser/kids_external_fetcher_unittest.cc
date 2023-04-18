// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_external_fetcher.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::BindOnce;
using ::base::Time;
using ::kids_chrome_management::FamilyRole;
using ::kids_chrome_management::ListFamilyMembersRequest;
using ::kids_chrome_management::ListFamilyMembersResponse;
using ::network::GetUploadData;
using ::network::TestURLLoaderFactory;
using ::signin::ConsentLevel;
using ::signin::IdentityTestEnvironment;
using ::testing::Test;

// Tests the Kids External API fetchers functionality.
class KidsExternalFetcherTest : public Test {
 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
  IdentityTestEnvironment identity_test_env_;
};

template <typename Request, typename Response>
class Receiver {
 public:
  const base::expected<std::unique_ptr<Response>, KidsExternalFetcherStatus>&
  GetResult() const {
    return result_;
  }

  void Receive(KidsExternalFetcherStatus fetch_status,
               std::unique_ptr<Response> response) {
    if (!fetch_status.IsOk()) {
      result_ = base::unexpected(fetch_status);
      return;
    }
    result_ = std::move(response);
  }

 private:
  base::expected<std::unique_ptr<Response>, KidsExternalFetcherStatus> result_;
};

TEST_F(KidsExternalFetcherTest, AcceptsRequests) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;
  ListFamilyMembersResponse response;

  auto fetcher = FetchListFamilyMembers(
      *identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(), "http://example.com",
      BindOnce(&Receiver<ListFamilyMembersRequest,
                         ListFamilyMembersResponse>::Receive,
               base::Unretained(&receiver)));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url,
            "http://example.com/families/mine/members?alt=proto");
  EXPECT_EQ(pending_request->request.method, "GET");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response.SerializeAsString());

  ASSERT_TRUE(receiver.GetResult().has_value());
}

TEST_F(KidsExternalFetcherTest, NoAccessToken) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;

  auto fetcher = FetchListFamilyMembers(
      *identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(), "http://example.com/",
      BindOnce(&Receiver<ListFamilyMembersRequest,
                         ListFamilyMembersResponse>::Receive,
               base::Unretained(&receiver)));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(receiver.GetResult().error().state(),
            KidsExternalFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR);
  EXPECT_EQ(receiver.GetResult().error().google_service_auth_error().state(),
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
}

TEST_F(KidsExternalFetcherTest, HandlesMalformedResponse) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;

  auto fetcher = FetchListFamilyMembers(
      *identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(), "http://example.com/",
      BindOnce(&Receiver<ListFamilyMembersRequest,
                         ListFamilyMembersResponse>::Receive,
               base::Unretained(&receiver)));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);
  EXPECT_EQ(pending_request->request.url,
            "http://example.com/families/mine/members?alt=proto");
  EXPECT_EQ(pending_request->request.method, "GET");

  std::string malformed_value("garbage");  // Not a valid marshaled proto.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), malformed_value);
  EXPECT_FALSE(receiver.GetResult().has_value());
  EXPECT_EQ(receiver.GetResult().error().state(),
            KidsExternalFetcherStatus::State::INVALID_RESPONSE);
}

TEST_F(KidsExternalFetcherTest, HandlesServerError) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "bob@gmail.com", ConsentLevel::kSignin);
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;

  auto fetcher = FetchListFamilyMembers(
      *identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(), "http://example.com/",
      BindOnce(&Receiver<ListFamilyMembersRequest,
                         ListFamilyMembersResponse>::Receive,
               base::Unretained(&receiver)));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", Time::Max());

  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);
  EXPECT_EQ(pending_request->request.url,
            "http://example.com/families/mine/members?alt=proto");
  EXPECT_EQ(pending_request->request.method, "GET");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), /*content=*/"",
      net::HTTP_BAD_REQUEST);
  EXPECT_FALSE(receiver.GetResult().has_value());
  EXPECT_EQ(receiver.GetResult().error().state(),
            KidsExternalFetcherStatus::State::NET_OR_HTTP_ERROR);
  EXPECT_EQ(receiver.GetResult().error().net_or_http_error_code(),
            KidsExternalFetcherStatus::NetOrHttpErrorType(
                net::ERR_HTTP_RESPONSE_CODE_FAILURE));
}

}  // namespace
