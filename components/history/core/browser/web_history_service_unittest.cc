// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/web_history_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace history {

namespace {

// A testing web history service that does extra checks and creates a
// TestRequest instead of a normal request.
class TestingWebHistoryService : public WebHistoryService {
 public:
  explicit TestingWebHistoryService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      // NOTE: Simply pass null object for IdentityManager. WebHistoryService's
      // only usage of this object is to fetch access tokens via RequestImpl,
      // and TestWebHistoryService deliberately replaces this flow with
      // TestRequest.
      : WebHistoryService(nullptr, url_loader_factory) {}

  TestingWebHistoryService(const TestingWebHistoryService&) = delete;
  TestingWebHistoryService& operator=(const TestingWebHistoryService&) = delete;

  ~TestingWebHistoryService() override = default;

  // This is sorta an override but override and static don't mix.
  // This function just calls WebHistoryService::ReadResponse.
  static std::optional<base::Value::Dict> ReadResponse(Request* request);
};

// A testing request class that allows expected values to be filled in.
class TestRequest : public WebHistoryService::Request {
 public:
  TestRequest(const GURL& url,
              WebHistoryService::CompletionCallback callback,
              int response_code,
              const std::string& response_body)
      : url_(url),
        callback_(std::move(callback)),
        response_code_(response_code),
        response_body_(response_body) {}

  TestRequest(const TestRequest&) = delete;
  TestRequest& operator=(const TestRequest&) = delete;

  ~TestRequest() override = default;

  // history::Request overrides
  bool IsPending() override { return false; }
  int GetResponseCode() override { return response_code_; }
  const std::string& GetResponseBody() override { return response_body_; }
  void SetPostData(const std::string& post_data) override {
    post_data_ = post_data;
  }
  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override {
    SetPostData(post_data);
  }
  void SetUserAgent(const std::string& post_data) override {}
  void Start() override { NOTREACHED(); }

 private:
  GURL url_;
  WebHistoryService::CompletionCallback callback_;
  int response_code_;
  std::string response_body_;
  std::string post_data_;
};

std::optional<base::Value::Dict> TestingWebHistoryService::ReadResponse(
    Request* request) {
  return WebHistoryService::ReadResponse(request);
}

}  // namespace

// A test class used for testing the WebHistoryService class.
class WebHistoryServiceTest : public testing::Test {
 public:
  WebHistoryServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        web_history_service_(test_shared_loader_factory_) {}

  WebHistoryServiceTest(const WebHistoryServiceTest&) = delete;
  WebHistoryServiceTest& operator=(const WebHistoryServiceTest&) = delete;

  ~WebHistoryServiceTest() override = default;

  void TearDown() override {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestingWebHistoryService web_history_service_;
};

TEST_F(WebHistoryServiceTest, VerifyReadResponse) {
  // Test that properly formatted response with good response code returns true
  // as expected.
  std::unique_ptr<WebHistoryService::Request> request(
      new TestRequest(GURL("http://history.google.com/"), base::DoNothing(),
                      net::HTTP_OK, /* response code */
                      "{\n"         /* response body */
                      "  \"history_recording_enabled\": true\n"
                      "}"));
  // ReadResponse deletes the request
  auto response_value = TestingWebHistoryService::ReadResponse(request.get());
  ASSERT_TRUE(response_value);
  bool enabled_value = false;
  if (std::optional<bool> enabled =
          response_value->FindBool("history_recording_enabled")) {
    enabled_value = *enabled;
  }
  EXPECT_TRUE(enabled_value);

  // Test that properly formatted response with good response code returns false
  // as expected.
  std::unique_ptr<WebHistoryService::Request> request2(new TestRequest(
      GURL("http://history.google.com/"), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording_enabled\": false\n"
      "}"));
  // ReadResponse deletes the request
  auto response_value2 = TestingWebHistoryService::ReadResponse(request2.get());
  ASSERT_TRUE(response_value2);
  enabled_value = true;
  if (std::optional<bool> enabled =
          response_value2->FindBool("history_recording_enabled")) {
    enabled_value = *enabled;
  }
  EXPECT_FALSE(enabled_value);

  // Test that a bad response code returns false.
  std::unique_ptr<WebHistoryService::Request> request3(
      new TestRequest(GURL("http://history.google.com/"), base::DoNothing(),
                      net::HTTP_UNAUTHORIZED,
                      "{\n"
                      "  \"history_recording_enabled\": true\n"
                      "}"));
  // ReadResponse deletes the request
  auto response_value3 = TestingWebHistoryService::ReadResponse(request3.get());
  EXPECT_FALSE(response_value3);

  // Test that improperly formatted response returns false.
  // Note: we expect to see a warning when running this test similar to
  //   "Non-JSON response received from history server".
  // This test tests how that situation is handled.
  std::unique_ptr<WebHistoryService::Request> request4(new TestRequest(
      GURL("http://history.google.com/"), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording_enabled\": not true\n"
      "}"));
  // ReadResponse deletes the request
  auto response_value4 = TestingWebHistoryService::ReadResponse(request4.get());
  EXPECT_FALSE(response_value4);

  // Test that improperly formatted response returns false.
  std::unique_ptr<WebHistoryService::Request> request5(new TestRequest(
      GURL("http://history.google.com/"), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording\": true\n"
      "}"));
  // ReadResponse deletes the request
  auto response_value5 = TestingWebHistoryService::ReadResponse(request5.get());
  ASSERT_TRUE(response_value5);
  EXPECT_FALSE(response_value5->FindBool("history_recording_enabled"));
}

}  // namespace history
