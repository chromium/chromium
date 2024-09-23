// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/topic_subscription_request.h"
#include "chromeos/ash/components/carrier_lock/common.h"

#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::carrier_lock {

namespace {

const uint64_t kAndroidId = 12345678;
const uint64_t kSecurityToken = 12345678;
const char kAppId[] = "TestAppId";
const char kDelete[] = "true";
const char kToken[] = "TestFCMToken";
const char kTopic[] = "/topics/testtopic";
const char kLoginHeader[] = "AidLogin";
const char kTopicSubscriptionURL[] =
    "https://android.clients.google.com/c2dm/register3";

}  // namespace

class TopicSubscriptionRequestTest : public testing::Test {
 public:
  TopicSubscriptionRequestTest();
  ~TopicSubscriptionRequestTest() override = default;

  void TopicSubscriptionCallback(Result result);

  void CreateRequest(const bool unsubscribe);

  void SetResponseForURLAndComplete(const std::string& url,
                                    net::HttpStatusCode status_code,
                                    const std::string& response_body,
                                    int net_error_code);
  void VerifyFetcherUploadDataForURL(
      const std::string& url,
      std::map<std::string, std::string>* expected_pairs);
  const net::HttpRequestHeaders* GetExtraHeadersForURL(const std::string& url);
  bool GetUploadDataForURL(const std::string& url, std::string* data_out);

 protected:
  // testing::Test:
  void SetUp() override {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  void TearDown() override { shared_factory_.reset(); }

  Result callback_result_;
  bool callback_called_;
  std::unique_ptr<TopicSubscriptionRequest> request_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
};

TopicSubscriptionRequestTest::TopicSubscriptionRequestTest()
    : callback_result_(Result::kSuccess), callback_called_(false) {}

void TopicSubscriptionRequestTest::TopicSubscriptionCallback(Result result) {
  callback_result_ = result;
  callback_called_ = true;
}

void TopicSubscriptionRequestTest::CreateRequest(const bool unsubscribe) {
  TopicSubscriptionRequest::RequestInfo request_info(
      kAndroidId, kSecurityToken, kAppId, kToken, kTopic, unsubscribe);
  request_ = std::make_unique<TopicSubscriptionRequest>(
      request_info, shared_factory_,
      base::BindOnce(&TopicSubscriptionRequestTest::TopicSubscriptionCallback,
                     base::Unretained(this)));
}

void TopicSubscriptionRequestTest::SetResponseForURLAndComplete(
    const std::string& url,
    net::HttpStatusCode status_code,
    const std::string& response_body,
    int net_error_code = net::OK) {
  callback_result_ = Result::kSuccess;
  callback_called_ = false;
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(url), network::URLLoaderCompletionStatus(net_error_code),
      network::CreateURLResponseHead(status_code), response_body));
}

bool TopicSubscriptionRequestTest::GetUploadDataForURL(const std::string& url,
                                                       std::string* data_out) {
  std::vector<network::TestURLLoaderFactory::PendingRequest>* pending_requests =
      test_url_loader_factory_.pending_requests();
  for (const auto& pending : *pending_requests) {
    if (pending.request.url == GURL(url)) {
      *data_out = network::GetUploadData(pending.request);
      return true;
    }
  }
  return false;
}

const net::HttpRequestHeaders*
TopicSubscriptionRequestTest::GetExtraHeadersForURL(const std::string& url) {
  std::vector<network::TestURLLoaderFactory::PendingRequest>* pending_requests =
      test_url_loader_factory_.pending_requests();
  for (const auto& pending : *pending_requests) {
    if (pending.request.url == GURL(url)) {
      return &pending.request.headers;
    }
  }
  return nullptr;
}

void TopicSubscriptionRequestTest::VerifyFetcherUploadDataForURL(
    const std::string& url,
    std::map<std::string, std::string>* expected_pairs) {
  std::string upload_data;
  ASSERT_TRUE(GetUploadDataForURL(url, &upload_data));

  // Verify data was formatted properly.
  base::StringTokenizer data_tokenizer(upload_data, "&=");
  while (data_tokenizer.GetNext()) {
    auto iter = expected_pairs->find(data_tokenizer.token());
    ASSERT_TRUE(iter != expected_pairs->end()) << data_tokenizer.token();
    ASSERT_TRUE(data_tokenizer.GetNext()) << data_tokenizer.token();
    ASSERT_EQ(iter->second, data_tokenizer.token());
    // Ensure that none of the keys appears twice.
    expected_pairs->erase(iter);
  }
  ASSERT_EQ(0UL, expected_pairs->size());
}

// Successful request.

TEST_F(TopicSubscriptionRequestTest, RequestSuccessful) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL, net::HTTP_OK, "{}");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kSuccess, callback_result_);
}

TEST_F(TopicSubscriptionRequestTest, RequestSubscriptionData) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  // Get data sent by request and verify that authorization header was put
  // together properly.
  const net::HttpRequestHeaders* headers =
      GetExtraHeadersForURL(kTopicSubscriptionURL);
  ASSERT_TRUE(headers != nullptr);
  std::string auth_header =
      headers->GetHeader(net::HttpRequestHeaders::kAuthorization)
          .value_or(std::string());
  base::StringTokenizer auth_tokenizer(auth_header, " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kSecurityToken), auth_tokenizer.token());

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["app"] = kAppId;
  expected_pairs["sender"] = kToken;
  expected_pairs["X-gcm.topic"] = kTopic;

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kTopicSubscriptionURL, &expected_pairs));
}

TEST_F(TopicSubscriptionRequestTest, RequestUnsubscriptionData) {
  CreateRequest(/*unsubscribe*/ true);
  request_->Start();

  // Get data sent by request and verify that authorization header was put
  // together properly.
  const net::HttpRequestHeaders* headers =
      GetExtraHeadersForURL(kTopicSubscriptionURL);
  ASSERT_TRUE(headers != nullptr);
  std::string auth_header =
      headers->GetHeader(net::HttpRequestHeaders::kAuthorization)
          .value_or(std::string());
  base::StringTokenizer auth_tokenizer(auth_header, " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kSecurityToken), auth_tokenizer.token());

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["app"] = kAppId;
  expected_pairs["sender"] = kToken;
  expected_pairs["X-gcm.topic"] = kTopic;
  expected_pairs["delete"] = kDelete;

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kTopicSubscriptionURL, &expected_pairs));
}

// Non-fatal errors with disabled retry.

TEST_F(TopicSubscriptionRequestTest, ResponseNetError) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL, net::HTTP_OK, "",
                               net::ERR_FAILED);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kConnectionError, callback_result_);
}

TEST_F(TopicSubscriptionRequestTest, ResponseHttpNotOK) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL, net::HTTP_GATEWAY_TIMEOUT,
                               "");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kInvalidResponse, callback_result_);
}

TEST_F(TopicSubscriptionRequestTest, ResponseAuthenticationFailed) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL, net::HTTP_UNAUTHORIZED,
                               "Error=AUTHENTICATION_FAILED");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kInvalidInput, callback_result_);
}

TEST_F(TopicSubscriptionRequestTest, ResponseInternalFailure) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL,
                               net::HTTP_INTERNAL_SERVER_ERROR,
                               "Error=InternalServerError");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kServerInternalError, callback_result_);
}

TEST_F(TopicSubscriptionRequestTest, ResponseTooManySubscribers) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL, net::HTTP_OK,
                               "Error=TOO_MANY_SUBSCRIBERS");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kServerInternalError, callback_result_);
}

// Fatal errors without retry.

TEST_F(TopicSubscriptionRequestTest, ResponseInvalidSender) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL, net::HTTP_OK,
                               "Error=INVALID_SENDER");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kInvalidInput, callback_result_);
}

TEST_F(TopicSubscriptionRequestTest, ResponseInvalidParameters) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL, net::HTTP_OK,
                               "Error=INVALID_PARAMETERS");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kInvalidInput, callback_result_);
}

TEST_F(TopicSubscriptionRequestTest, ResponseQuotaExceeded) {
  CreateRequest(/*unsubscribe*/ false);
  request_->Start();

  SetResponseForURLAndComplete(kTopicSubscriptionURL,
                               net::HTTP_SERVICE_UNAVAILABLE,
                               "Error=QUOTA_EXCEEDED");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(Result::kServerInternalError, callback_result_);
}

}  // namespace ash::carrier_lock
