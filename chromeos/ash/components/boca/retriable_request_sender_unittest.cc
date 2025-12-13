// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/retriable_request_sender.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/util.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

constexpr std::string_view kValue = "value";
constexpr std::string_view kResponse = R"({"key": "value"})";
constexpr size_t kMaxRetries = 2;
constexpr std::string_view kTestUrl =
    "https://schooltools-pa.googleapis.com/test";

class TestDelegate : public BocaRequest::Delegate {
 public:
  static std::unique_ptr<BocaRequest::Delegate> CreateDelegate(
      base::RepeatingClosure signal,
      base::OnceCallback<void(std::optional<std::string>)> callback) {
    return std::make_unique<TestDelegate>(std::move(signal),
                                          std::move(callback));
  }

  TestDelegate(base::RepeatingClosure signal,
               base::OnceCallback<void(std::optional<std::string>)> callback)
      : signal_(std::move(signal)), callback_(std::move(callback)) {}
  ~TestDelegate() override = default;

  // BocaRequest::Delegate:
  std::string GetRelativeUrl() override { return "test"; }
  google_apis::HttpRequestMethod GetRequestType() const override {
    return google_apis::HttpRequestMethod::kGet;
  }
  std::optional<std::string> GetRequestBody() override { return std::nullopt; }

  void OnSuccess(std::unique_ptr<base::Value> response) override {
    std::move(callback_).Run(*response->GetDict().FindString("key"));
    signal_.Run();
  }

  void OnError(google_apis::ApiErrorCode error) override {
    std::move(callback_).Run(std::nullopt);
    signal_.Run();
  }

 private:
  base::RepeatingClosure signal_;
  base::OnceCallback<void(std::optional<std::string>)> callback_;
};

class RetriableRequestSenderTest : public testing::Test {
 protected:
  void SetUp() override {
    auto request_sender = std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        url_loader_factory_.GetSafeWeakWrapper(),
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
    retriable_sender_ = std::make_unique<RetriableRequestSender<std::string>>(
        std::move(request_sender), kMaxRetries);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<RetriableRequestSender<std::string>> retriable_sender_;
};

}  // namespace

TEST_F(RetriableRequestSenderTest, SuccessFirstTry) {
  base::test::TestFuture<std::optional<std::string>> future;
  auto create_delegate_cb =
      base::BindRepeating(&TestDelegate::CreateDelegate, base::DoNothing());

  url_loader_factory_.AddResponse(kTestUrl, kResponse);
  retriable_sender_->SendRequest(create_delegate_cb, future.GetCallback());

  EXPECT_EQ(future.Get(), kValue);
  EXPECT_EQ(url_loader_factory_.total_requests(), 1ul);
}

TEST_F(RetriableRequestSenderTest, FailsOnceThenSucceeds) {
  base::test::RepeatingTestFuture<void> signal;
  base::test::TestFuture<std::optional<std::string>> future;
  auto create_delegate_cb =
      base::BindRepeating(&TestDelegate::CreateDelegate, signal.GetCallback());

  // Simulate first request failure.
  url_loader_factory_.AddResponse(
      GURL(kTestUrl), network::mojom::URLResponseHead::New(),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::HTTP_SERVICE_UNAVAILABLE));
  retriable_sender_->SendRequest(create_delegate_cb, future.GetCallback());
  // Wait failure notification.
  ASSERT_TRUE(signal.Wait());
  // Simulate retry success.
  url_loader_factory_.AddResponse(kTestUrl, kResponse);
  task_environment_.FastForwardBy(base::Milliseconds(250));

  EXPECT_EQ(future.Get(), kValue);
  EXPECT_EQ(url_loader_factory_.total_requests(), 2ul);
}

TEST_F(RetriableRequestSenderTest, FailsUntilMaxRetries) {
  base::test::RepeatingTestFuture<void> signal;
  base::test::TestFuture<std::optional<std::string>> future;
  auto create_delegate_cb =
      base::BindRepeating(&TestDelegate::CreateDelegate, signal.GetCallback());

  url_loader_factory_.AddResponse(
      GURL(kTestUrl), network::mojom::URLResponseHead::New(),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::HTTP_SERVICE_UNAVAILABLE));
  retriable_sender_->SendRequest(create_delegate_cb, future.GetCallback());
  ASSERT_TRUE(signal.Wait());
  task_environment_.FastForwardBy(base::Milliseconds(250));
  ASSERT_TRUE(signal.Wait());
  task_environment_.FastForwardBy(base::Milliseconds(500));

  EXPECT_EQ(future.Get(), std::nullopt);
  // 1 initial attempt + 2 retries
  EXPECT_EQ(url_loader_factory_.total_requests(), kMaxRetries + 1ul);
}

}  // namespace ash::boca
