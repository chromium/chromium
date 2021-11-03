// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace safe_browsing {

namespace {

const char kQueryTailoredSecurityServiceUrl[] =
    "https://history.google.com/history/api/lookup?client=aesb";

// A testing tailored security service that does extra checks and creates a
// TestRequest instead of a normal request.
class TestingTailoredSecurityService : public TailoredSecurityService {
 public:
  explicit TestingTailoredSecurityService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      // NOTE: Simply pass null object for IdentityManager.
      // TailoredSecurityService's only usage of this object is to fetch access
      // tokens via RequestImpl, and TestingTailoredSecurityService deliberately
      // replaces this flow with TestRequest.
      : TailoredSecurityService(nullptr, url_loader_factory) {}
  ~TestingTailoredSecurityService() override = default;

  std::unique_ptr<TailoredSecurityService::Request> CreateRequest(
      const GURL& url,
      CompletionCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  // This is sorta an override but override and static don't mix.
  // This function just calls TailoredSecurityService::ReadResponse.
  static base::Value ReadResponse(Request* request);

  const std::string& GetExpectedPostData(
      TailoredSecurityService::Request* request);

  std::string GetExpectedTailoredSecurityServiceValue();

  void SetTailoredSecurityServiceCallback(bool is_enabled);

  void GetTailoredSecurityServiceCallback(bool is_enabled);

  void MultipleRequestsCallback(bool is_enabled);

  void SetExpectedURL(const GURL& expected_url) {
    expected_url_ = expected_url;
  }

  void SetExpectedTailoredSecurityServiceValue(bool expected_value) {
    expected_tailored_security_service_value_ = expected_value;
  }

  void SetExpectedPostData(const std::string& expected_data) {
    current_expected_post_data_ = expected_data;
  }

  void EnsureNoPendingRequestsRemain() {
    EXPECT_EQ(0u, GetNumberOfPendingTailoredSecurityServiceRequests());
  }

  void Shutdown() override;

 private:
  GURL expected_url_;
  bool expected_tailored_security_service_value_;
  std::string current_expected_post_data_;
  std::map<Request*, std::string> expected_post_data_;
};

// A testing request class that allows expected values to be filled in.
class TestRequest : public TailoredSecurityService::Request {
 public:
  TestRequest(const GURL& url,
              TailoredSecurityService::CompletionCallback callback,
              int response_code,
              const std::string& response_body)
      : tailored_security_service_(nullptr),
        url_(url),
        callback_(std::move(callback)),
        response_code_(response_code),
        response_body_(response_body),
        is_pending_(false) {}

  TestRequest(const GURL& url,
              TailoredSecurityService::CompletionCallback callback,
              TestingTailoredSecurityService* tailored_security_service)
      : tailored_security_service_(tailored_security_service),
        url_(url),
        callback_(std::move(callback)),
        response_code_(net::HTTP_OK),
        is_pending_(false) {
    response_body_ =
        std::string("{\"history_recording_enabled\":") +
        tailored_security_service->GetExpectedTailoredSecurityServiceValue() +
        ("}");
  }

  ~TestRequest() override = default;

  // safe_browsing::Request overrides
  bool IsPending() const override { return is_pending_; }
  int GetResponseCode() const override { return response_code_; }
  const std::string& GetResponseBody() const override { return response_body_; }
  void SetPostData(const std::string& post_data) override {
    post_data_ = post_data;
  }

  void Shutdown() override { is_shut_down_ = true; }

  void Start() override {
    is_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TestRequest::MimicReturnFromFetch,
                                  base::Unretained(this)));
  }

  void MimicReturnFromFetch() {
    // Mimic a successful fetch and return. We don't actually send out a request
    // in unittests.
    if (is_shut_down_)
      return;
    EXPECT_EQ(tailored_security_service_->GetExpectedPostData(this),
              post_data_);
    std::move(callback_).Run(this, true);
  }

 private:
  TestingTailoredSecurityService* tailored_security_service_;
  GURL url_;
  TailoredSecurityService::CompletionCallback callback_;
  int response_code_;
  std::string response_body_;
  std::string post_data_;
  bool is_pending_;
  bool is_shut_down_ = false;
};

std::unique_ptr<TailoredSecurityService::Request>
TestingTailoredSecurityService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  EXPECT_EQ(expected_url_, url);
  std::unique_ptr<TailoredSecurityService::Request> request =
      std::make_unique<TestRequest>(url, std::move(callback), this);
  expected_post_data_[request.get()] = current_expected_post_data_;
  return request;
}

base::Value TestingTailoredSecurityService::ReadResponse(Request* request) {
  return TailoredSecurityService::ReadResponse(request);
}

void TestingTailoredSecurityService::SetTailoredSecurityServiceCallback(
    bool is_enabled) {
  EXPECT_EQ(expected_tailored_security_service_value_, is_enabled);
}

void TestingTailoredSecurityService::GetTailoredSecurityServiceCallback(
    bool is_enabled) {
  EXPECT_EQ(expected_tailored_security_service_value_, is_enabled);
}

void TestingTailoredSecurityService::MultipleRequestsCallback(bool is_enabled) {
  EXPECT_EQ(expected_tailored_security_service_value_, is_enabled);
}

const std::string& TestingTailoredSecurityService::GetExpectedPostData(
    Request* request) {
  return expected_post_data_[request];
}

std::string
TestingTailoredSecurityService::GetExpectedTailoredSecurityServiceValue() {
  if (expected_tailored_security_service_value_)
    return "true";
  return "false";
}

void TestingTailoredSecurityService::Shutdown() {
  for (auto request : expected_post_data_) {
    request.first->Shutdown();
  }
  expected_post_data_.clear();
}

}  // namespace

// A test class used for testing the TailoredSecurityService class.
// In order for TailoredSecurityService to be valid, we must have a valid
// ProfileSyncService. Using the ProfileSyncServiceMock class allows to
// assign specific return values as needed to make sure the tailored security
// service is available.
class TailoredSecurityServiceTest : public testing::Test {
 public:
  TailoredSecurityServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        tailored_security_service_(test_shared_loader_factory_) {}

  ~TailoredSecurityServiceTest() override = default;

  void TearDown() override {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

  TestingTailoredSecurityService* tailored_security_service() {
    return &tailored_security_service_;
  }

  void Shutdown() { tailored_security_service_.Shutdown(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestingTailoredSecurityService tailored_security_service_;
};

TEST_F(TailoredSecurityServiceTest, GetTailoredSecurityServiceEnabled) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);
  tailored_security_service()->StartRequest(
      base::BindOnce(
          &TestingTailoredSecurityService::GetTailoredSecurityServiceCallback,
          base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest,
       SetTailoredSecurityBitEnabledForTestingTrue) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);
  tailored_security_service()->SetExpectedPostData(
      "{\"history_recording_enabled\":true}");
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      true,
      base::BindOnce(
          &TestingTailoredSecurityService::SetTailoredSecurityServiceCallback,
          base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest, SetTailoredSecurityBitForTestingFalse) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(false);
  tailored_security_service()->SetExpectedPostData(
      "{\"history_recording_enabled\":false}");
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      false,
      base::BindOnce(
          &TestingTailoredSecurityService::SetTailoredSecurityServiceCallback,
          base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest, MultipleRequests) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(false);
  tailored_security_service()->SetExpectedPostData(
      "{\"history_recording_enabled\":false}");
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      false,
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedPostData("");
  tailored_security_service()->StartRequest(
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  // Check that both requests are no longer pending.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest, VerifyReadResponse) {
  // Test that properly formatted response with good response code returns true
  // as expected.
  std::unique_ptr<TailoredSecurityService::Request> request(
      new TestRequest(GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(),
                      net::HTTP_OK, /* response code */
                      "{\n"         /* response body */
                      "  \"history_recording_enabled\": true\n"
                      "}"));
  // ReadResponse deletes the request
  base::Value response_value =
      TestingTailoredSecurityService::ReadResponse(request.get());
  EXPECT_TRUE(
      response_value.FindBoolKey("history_recording_enabled").value_or(false));
  // Test that properly formatted response with good response code returns false
  // as expected.
  std::unique_ptr<TailoredSecurityService::Request> request2(new TestRequest(
      GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording_enabled\": false\n"
      "}"));
  // ReadResponse deletes the request
  base::Value response_value2 =
      TestingTailoredSecurityService::ReadResponse(request2.get());
  EXPECT_FALSE(
      response_value2.FindBoolKey("history_recording_enabled").value_or(false));

  // Test that a bad response code returns false.
  std::unique_ptr<TailoredSecurityService::Request> request3(
      new TestRequest(GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(),
                      net::HTTP_UNAUTHORIZED,
                      "{\n"
                      "  \"history_recording_enabled\": true\n"
                      "}"));
  // ReadResponse deletes the request
  base::Value response_value3 =
      TestingTailoredSecurityService::ReadResponse(request3.get());
  EXPECT_TRUE(response_value3.is_none());

  // Test that improperly formatted response returns false.
  // Note: we expect to see a warning when running this test similar to
  //   "Non-JSON response received from history server".
  // This test tests how that situation is handled.
  std::unique_ptr<TailoredSecurityService::Request> request4(new TestRequest(
      GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording_enabled\": not true\n"
      "}"));
  // ReadResponse deletes the request
  base::Value response_value4 =
      TestingTailoredSecurityService::ReadResponse(request4.get());
  EXPECT_TRUE(response_value4.is_none());

  // Test that improperly formatted response (different key) returns false.
  std::unique_ptr<TailoredSecurityService::Request> request5(new TestRequest(
      GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording\": true\n"
      "}"));
  // ReadResponse deletes the request
  base::Value response_value5 =
      TestingTailoredSecurityService::ReadResponse(request5.get());
  EXPECT_FALSE(
      response_value2.FindBoolKey("history_recording_enabled").value_or(false));
}

TEST_F(TailoredSecurityServiceTest, TestShutdown) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(false);
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      false,
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->StartRequest(
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  Shutdown();
}

}  // namespace safe_browsing
