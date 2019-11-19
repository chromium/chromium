// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_info_fetcher_win.h"

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAccessTokenValue[] = "test_access_token_value";
constexpr char kRefreshTokenValue[] = "test_refresh_token_value";

}  // namespace

// Provides base functionality for the AccessTokenFetcher Tests below.  The
// FakeURLFetcherFactory allows us to override the response data and payload for
// specified URLs.  We use this to stub out network calls made by the
// AccessTokenFetcher.  This fixture also creates an IO MessageLoop, if
// necessary, for use by the AccessTokenFetcher.
class CredentialProviderFetcherTest : public ::testing::Test {
 protected:
  CredentialProviderFetcherTest();
  ~CredentialProviderFetcherTest() override;

  void OnFetchComplete(base::OnceClosure done_closure,
                       base::Value fetch_result);

  void SetFakeResponses(const std::string& access_token_fetch_data,
                        net::HttpStatusCode access_token_fetch_code,
                        int access_token_net_error,
                        const std::string& user_info_data,
                        net::HttpStatusCode user_info_code,
                        int user_info_net_error,
                        const std::string& token_info_data,
                        net::HttpStatusCode token_info_code,
                        int token_info_net_error);

  scoped_refptr<network::SharedURLLoaderFactory> shared_factory() {
    return shared_factory_;
  }

  void RunFetcher(const std::string& additional_oauth_scopes);

  // Used for result verification
  base::Value fetch_result_;
  CredentialProviderSigninDialogTestDataStorage test_data_storage_;

  std::string valid_token_info_response_;
  std::string valid_user_info_response_;
  std::string valid_access_token_fetch_response_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::TaskEnvironment task_environment_;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderFetcherTest);
};

CredentialProviderFetcherTest::CredentialProviderFetcherTest()
    : shared_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {
  valid_token_info_response_ =
      test_data_storage_.GetSuccessfulTokenInfoFetchResult();
  valid_user_info_response_ =
      test_data_storage_.GetSuccessfulUserInfoFetchResult();
  valid_access_token_fetch_response_ =
      test_data_storage_.GetSuccessfulMdmTokenFetchResult();
}

CredentialProviderFetcherTest::~CredentialProviderFetcherTest() = default;

void CredentialProviderFetcherTest::OnFetchComplete(
    base::OnceClosure done_closure,
    base::Value fetch_result) {
  EXPECT_TRUE(fetch_result.is_dict());
  fetch_result_ = std::move(fetch_result);

  std::move(done_closure).Run();
}

void CredentialProviderFetcherTest::SetFakeResponses(
    const std::string& access_token_fetch_data,
    net::HttpStatusCode access_token_fetch_code,
    int access_token_net_error,
    const std::string& user_info_data,
    net::HttpStatusCode user_info_code,
    int user_info_net_error,
    const std::string& token_info_data,
    net::HttpStatusCode token_info_code,
    int token_info_net_error) {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_info_url(),
      network::CreateURLResponseHead(token_info_code), token_info_data,
      network::URLLoaderCompletionStatus(token_info_net_error));

  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url(),
      network::CreateURLResponseHead(user_info_code), user_info_data,
      network::URLLoaderCompletionStatus(user_info_net_error));

  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url(),
      network::CreateURLResponseHead(access_token_fetch_code),
      access_token_fetch_data,
      network::URLLoaderCompletionStatus(access_token_net_error));
}

void CredentialProviderFetcherTest::RunFetcher(
    const std::string& additional_oauth_scopes) {
  base::RunLoop run_loop;
  auto fetcher_callback =
      base::BindOnce(&CredentialProviderFetcherTest::OnFetchComplete,
                     base::Unretained(this), run_loop.QuitClosure());

  CredentialProviderSigninInfoFetcher fetcher(kRefreshTokenValue,
                                              shared_factory());
  fetcher.SetCompletionCallbackAndStart(
      kAccessTokenValue, additional_oauth_scopes, std::move(fetcher_callback));
  run_loop.Run();
}

TEST_F(CredentialProviderFetcherTest, ValidFetchResult) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher("");
  EXPECT_FALSE(fetch_result_.DictEmpty());
  EXPECT_TRUE(test_data_storage_.EqualsSccessfulFetchResult(fetch_result_));
}

TEST_F(CredentialProviderFetcherTest,
       ValidFetchResultWithNetworkErrorOnTokenFetch) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_BAD_REQUEST,
                   net::ERR_FAILED, valid_user_info_response_, net::HTTP_OK,
                   net::OK, valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher("");
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest,
       ValidFetchResultWithNetworkErrorOnUserInfoFetch) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_BAD_REQUEST,
                   net::ERR_FAILED, valid_token_info_response_, net::HTTP_OK,
                   net::OK);

  RunFetcher("");
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidAccessTokenFetch) {
  SetFakeResponses(
      CredentialProviderSigninDialogTestDataStorage::kInvalidTokenFetchResponse,
      net::HTTP_OK, net::OK, valid_user_info_response_, net::HTTP_OK, net::OK,
      valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher("");
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidUserInfoFetch) {
  SetFakeResponses(
      valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
      CredentialProviderSigninDialogTestDataStorage::kInvalidUserInfoResponse,
      net::HTTP_OK, net::OK, valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher("");
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidTokenInfoFetch) {
  SetFakeResponses(
      valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
      valid_user_info_response_, net::HTTP_OK, net::OK,
      CredentialProviderSigninDialogTestDataStorage::kInvalidTokenInfoResponse,
      net::HTTP_OK, net::OK);

  RunFetcher("");
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidFetchResult) {
  SetFakeResponses(
      CredentialProviderSigninDialogTestDataStorage::kInvalidTokenFetchResponse,
      net::HTTP_OK, net::OK,
      CredentialProviderSigninDialogTestDataStorage::kInvalidUserInfoResponse,
      net::HTTP_OK, net::OK,
      CredentialProviderSigninDialogTestDataStorage::kInvalidTokenInfoResponse,
      net::HTTP_OK, net::OK);

  RunFetcher("");
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, ProperlyProvidedScopes) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url == GaiaUrls::GetInstance()->oauth2_token_url()) {
          std::vector<std::string> scopes = {"email", "profile", "openid", "a",
                                             "b"};
          EXPECT_THAT(GetUploadData(request),
                      ::testing::HasSubstr(base::JoinString(scopes, "+")));
        }
        task_environment_.RunUntilIdle();
      }));
  RunFetcher("a,b");
}

TEST_F(CredentialProviderFetcherTest, SpacedOutScopes) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url == GaiaUrls::GetInstance()->oauth2_token_url()) {
          std::vector<std::string> scopes = {"email", "profile", "openid", "a",
                                             "b"};
          EXPECT_THAT(GetUploadData(request),
                      ::testing::HasSubstr(base::JoinString(scopes, "+")));
        }
        task_environment_.RunUntilIdle();
      }));
  RunFetcher(" a , b ");
}

TEST_F(CredentialProviderFetcherTest, EmptyScopes) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url == GaiaUrls::GetInstance()->oauth2_token_url()) {
          std::vector<std::string> scopes = {"email", "profile", "openid", "a",
                                             "b"};
          EXPECT_THAT(GetUploadData(request),
                      ::testing::HasSubstr(base::JoinString(scopes, "+")));
        }
        task_environment_.RunUntilIdle();
      }));
  RunFetcher("a,b,,");
}

TEST_F(CredentialProviderFetcherTest, DefaultScopes) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url == GaiaUrls::GetInstance()->oauth2_token_url()) {
          std::vector<std::string> scopes = {"email", "profile", "openid"};
          EXPECT_THAT(GetUploadData(request),
                      ::testing::HasSubstr(base::JoinString(scopes, "+")));
        }
        task_environment_.RunUntilIdle();
      }));
  RunFetcher("");
}
