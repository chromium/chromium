// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/platform_auth/url_session_url_loader.h"

#include <Foundation/Foundation.h>

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "components/enterprise/platform_auth/url_session_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/test/mock_url_loader_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using MockClient = testing::NiceMock<network::MockURLLoaderClient>;
using url_session_test_util::ResponseConfig;

namespace {

constexpr char kBody[] = "payload";

constexpr char kInitiator[] = "https://foobar.example.com";
constexpr char kAccessControlAllowOrigin[] = "Access-Control-Allow-Origin";
constexpr char kSsoRequestURL[] =
    "https://foobar.example.com/idp/idx/authenticators/sso_extension/"
    "transactions/123/verify";

}  // namespace

namespace enterprise_auth {

class URLSessionURLLoaderTest : public testing::Test {
 protected:
  URLSessionURLLoaderTest()
      : kTooBigPayload(1 << 21, 'a'),
        client_receiver_(&mock_client,
                         client_remote_.InitWithNewPipeAndPassReceiver()) {
    client_receiver_.set_disconnect_handler(
        client_disconnect_future_.GetCallback());
  }

  MockClient& GetMockClient() { return mock_client; }

  // Should be called at most once per test instance.
  // The behaviour of the network is controlled by the url used, see the
  // anonymous namespace for details.
  void StartRequest(ResponseConfig&& response_config,
                    const network::ResourceRequest& request,
                    base::TimeDelta timeout = base::TimeDelta::Max()) {
    CreateURLSessionURLLoader(std::move(response_config));
    url_loader_->Start(request, loader_remote_.BindNewPipeAndPassReceiver(),
                       std::move(client_remote_), timeout);
  }

  void StartRequest(ResponseConfig&& response_config,
                    base::TimeDelta timeout = base::TimeDelta::Max()) {
    network::ResourceRequest request;
    request.url = GURL(kSsoRequestURL);
    request.method = "POST";
    request.request_initiator = url::Origin::Create(request.url);
    StartRequest(std::move(response_config), request, timeout);
  }

  void WaitForLoaderToDisconnectAndDestroy() {
    EXPECT_TRUE(client_disconnect_future_.Wait());
    auto weak_ptr_copy = url_loader_;
    base::test::RunUntil(
        [weak_ptr_copy]() { return weak_ptr_copy.get() == nullptr; });
    client_receiver_.reset();
  }

  void DisconnectAndWaitForLoadersDestruction() {
    loader_remote_.reset();
    client_receiver_.reset();
    auto weak_ptr_copy = url_loader_;
    base::test::RunUntil(
        [weak_ptr_copy]() { return weak_ptr_copy.get() == nullptr; });
  }

  mojo::Remote<network::mojom::URLLoader> loader_remote_;
  base::HistogramTester histogram_tester_;
  const std::string kTooBigPayload;

 private:
  void CreateURLSessionURLLoader(ResponseConfig&& response_config) {
    URLSessionURLLoader* instance = new URLSessionURLLoader();
    instance->SetAttachProtocolCallbackForTesting(
        base::BindOnce(url_session_test_util::AttachProtocolToSessionForTesting,
                       std::move(response_config)));
    url_loader_ = instance->weak_ptr_factory_.GetWeakPtr();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<void> client_disconnect_future_;
  base::test::TestFuture<void> request_started_future_;

  base::WeakPtr<URLSessionURLLoader> url_loader_ = nullptr;

  MockClient mock_client;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote_;
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_;
};

TEST_F(URLSessionURLLoaderTest, SuccessfulResponseWithBody) {
  ResponseConfig config;
  config.body = kBody;
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        std::string received_body;
        ASSERT_TRUE(body.is_valid());
        ASSERT_TRUE(
            mojo::BlockingCopyToString(std::move(body), &received_body));
        EXPECT_EQ(kBody, received_body);
        ASSERT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 1);
}

TEST_F(URLSessionURLLoaderTest, SuccessfulResponseWithEmptyBody) {
  ResponseConfig config;
  config.body = "";
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        ASSERT_FALSE(body.is_valid());
        ASSERT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, SuccessfulResponseWithNoBody) {
  ResponseConfig config;
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        ASSERT_FALSE(body.is_valid());
        EXPECT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, RejectsTooBigBodies) {
  ResponseConfig config;
  config.body = kTooBigPayload;
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FILE_TOO_BIG)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kResponseTooBig, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, DestroysItselfOnDisconnect) {
  ResponseConfig config;
  base::test::TestFuture<void> started_future;
  base::test::TestFuture<void> stopped_future;
  config.hang = true;
  config.on_started = started_future.GetSequenceBoundCallback();
  config.on_stopped = stopped_future.GetSequenceBoundCallback();

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(_)).Times(0);

  StartRequest(std::move(config));
  EXPECT_TRUE(started_future.Wait());

  DisconnectAndWaitForLoadersDestruction();
  EXPECT_TRUE(stopped_future.Wait());

  histogram_tester_.ExpectTotalCount(URLSessionURLLoader::kOktaResultHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, HandlesErrorGently) {
  ResponseConfig config;
  config.os_error = true;

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kOsError, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, RequestCanceledOnDestruction) {
  ResponseConfig config;
  base::test::TestFuture<void> started_future;
  base::test::TestFuture<void> cancel_future;
  config.hang = true;
  config.on_started = started_future.GetSequenceBoundCallback();
  config.on_stopped = cancel_future.GetSequenceBoundCallback();

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(_)).Times(0);

  StartRequest(std::move(config));
  EXPECT_TRUE(started_future.Wait());
  DisconnectAndWaitForLoadersDestruction();
  EXPECT_TRUE(cancel_future.Wait());

  histogram_tester_.ExpectTotalCount(URLSessionURLLoader::kOktaResultHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, WorksWithoutReceiver) {
  loader_remote_.reset();

  ResponseConfig config;
  config.body = kBody;
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        std::string received_body;
        ASSERT_TRUE(body.is_valid());
        ASSERT_TRUE(
            mojo::BlockingCopyToString(std::move(body), &received_body));
        EXPECT_EQ(kBody, received_body);
        ASSERT_TRUE(head->headers);
      });

  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, Timeout) {
  ResponseConfig config;
  config.hang = true;

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);

  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_TIMED_OUT)))
      .Times(1);

  StartRequest(std::move(config), base::Seconds(1));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kTimeout, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
}

TEST_F(URLSessionURLLoaderTest, ExpectsRequestInitiator) {
  ResponseConfig config;
  config.body = kBody;
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  network::ResourceRequest request;
  request.url = GURL(kSsoRequestURL);
  request.method = "POST";
  StartRequest(std::move(config), request);
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kOther, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
}

TEST_F(URLSessionURLLoaderTest, BlocksCrossOriginRequest) {
  ResponseConfig config;
  config.body = kBody;
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  network::ResourceRequest request;
  request.url = GURL(kSsoRequestURL);
  request.method = "POST";
  request.request_initiator = url::Origin::Create(
      GURL("https://not.initiator.com/idp/idx/authenticators/sso_extension/"
           "transactions/123/verify"));
  StartRequest(std::move(config), request);
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kCorsViolation, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
}

TEST_F(URLSessionURLLoaderTest, AllowsSameOriginRedirects) {
  ResponseConfig config;
  config.redirect_url = "https://foobar.example.com/redirect";
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _))
      .Times(1)
      .WillOnce([&](auto head, auto body, auto) {
        std::string received_body;
        ASSERT_TRUE(body.is_valid());
        ASSERT_TRUE(
            mojo::BlockingCopyToString(std::move(body), &received_body));
        EXPECT_EQ("redirect", received_body);
        ASSERT_TRUE(head->headers);
      });
  EXPECT_CALL(mock_client,
              OnComplete(testing::Field(
                  &network::URLLoaderCompletionStatus::error_code, net::OK)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 0);
}

TEST_F(URLSessionURLLoaderTest, BlocksCrossOriginRedirects) {
  ResponseConfig config;
  config.redirect_url = "https://redirect.example.com";
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kCorsViolation, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
}

TEST_F(URLSessionURLLoaderTest, FailsForNonSSORequests) {
  ResponseConfig config;
  config.headers = {{kAccessControlAllowOrigin, kInitiator}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  // Using GET method to signify this is an invalid request.
  network::ResourceRequest request;
  request.url = GURL(kSsoRequestURL);
  request.method = "GET";
  request.request_initiator = url::Origin::Create(request.url);
  StartRequest(std::move(config), request);
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kOther, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
}

TEST_F(URLSessionURLLoaderTest, FailsForMissingAccessControlAllowOriginHeader) {
  ResponseConfig config;

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kCorsViolation, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
}

TEST_F(URLSessionURLLoaderTest, AccessControlAllowOriginHeaderMismatch) {
  ResponseConfig config;
  config.headers = {{kAccessControlAllowOrigin, "https://not.initiator.com"}};

  MockClient& mock_client = GetMockClient();
  EXPECT_CALL(mock_client, OnReceiveResponse(_, _, _)).Times(0);
  EXPECT_CALL(mock_client, OnComplete(testing::Field(
                               &network::URLLoaderCompletionStatus::error_code,
                               net::ERR_FAILED)))
      .Times(1);

  StartRequest(std::move(config));
  WaitForLoaderToDisconnectAndDestroy();

  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaResultHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaSuccessDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kCorsViolation, 1);
  histogram_tester_.ExpectTotalCount(
      URLSessionURLLoader::kOktaFailureDurationHistogram, 1);
}

}  // namespace enterprise_auth
