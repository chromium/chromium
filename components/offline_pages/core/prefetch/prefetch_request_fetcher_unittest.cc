// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"

#include "base/bind.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace offline_pages {

namespace {
const GURL kTestURL("http://exmaple.org");
const char kTestMessage[] = "Testing";
}  // namespace

class PrefetchRequestFetcherTest : public PrefetchRequestTestBase {
 public:
  PrefetchRequestStatus RunFetcherWithNetError(net::Error net_error);
  PrefetchRequestStatus RunFetcherWithHttpError(net::HttpStatusCode http_error);
  PrefetchRequestStatus RunFetcherWithData(const std::string& response_data,
                                           std::string* data_received);
  PrefetchRequestStatus RunFetcherWithHttpErrorAndData(
      net::HttpStatusCode http_error,
      const std::string& response_data);
  void SetEmptyRequest(bool empty_request) { empty_request_ = empty_request; }

 private:
  PrefetchRequestStatus RunFetcher(
      base::OnceCallback<void(void)> respond_callback,
      std::string* data_received);
  bool empty_request_;
};

PrefetchRequestStatus PrefetchRequestFetcherTest::RunFetcherWithNetError(
    net::Error net_error) {
  std::string data_received;
  PrefetchRequestStatus status =
      RunFetcher(base::BindOnce(&PrefetchRequestTestBase::RespondWithNetError,
                                base::Unretained(this), net_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());
  return status;
}

PrefetchRequestStatus PrefetchRequestFetcherTest::RunFetcherWithHttpError(
    net::HttpStatusCode http_error) {
  std::string data_received;
  PrefetchRequestStatus status =
      RunFetcher(base::BindOnce(&PrefetchRequestTestBase::RespondWithHttpError,
                                base::Unretained(this), http_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());
  return status;
}

PrefetchRequestStatus PrefetchRequestFetcherTest::RunFetcherWithData(
    const std::string& response_data,
    std::string* data_received) {
  return RunFetcher(base::BindOnce(&PrefetchRequestTestBase::RespondWithData,
                                   base::Unretained(this), response_data),
                    data_received);
}

PrefetchRequestStatus
PrefetchRequestFetcherTest::RunFetcherWithHttpErrorAndData(
    net::HttpStatusCode http_error,
    const std::string& response_data) {
  std::string data_received;
  return RunFetcher(
      base::BindOnce(&PrefetchRequestTestBase::RespondWithHttpErrorAndData,
                     base::Unretained(this), http_error, response_data),
      &data_received);
}

PrefetchRequestStatus PrefetchRequestFetcherTest::RunFetcher(
    base::OnceCallback<void(void)> respond_callback,
    std::string* data_received) {
  base::MockCallback<PrefetchRequestFetcher::FinishedCallback> callback;
  std::unique_ptr<PrefetchRequestFetcher> fetcher =
      PrefetchRequestFetcher::CreateForPost(
          kTestURL, kTestMessage, /*testing_header_value=*/"", empty_request_,
          shared_url_loader_factory(), callback.Get());

  PrefetchRequestStatus status;
  std::string data;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&data)));
  std::move(respond_callback).Run();
  RunUntilIdle();

  *data_received = data;
  return status;
}

TEST_F(PrefetchRequestFetcherTest, NetErrors) {
  EXPECT_EQ(PrefetchRequestStatus::kShouldSuspendBlockedByAdministrator,
            RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));

  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithoutBackoff,
            RunFetcherWithNetError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithoutBackoff,
            RunFetcherWithNetError(net::ERR_NETWORK_CHANGED));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithoutBackoff,
            RunFetcherWithNetError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithoutBackoff,
            RunFetcherWithNetError(net::ERR_CONNECTION_CLOSED));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithoutBackoff,
            RunFetcherWithNetError(net::ERR_CONNECTION_REFUSED));
}

TEST_F(PrefetchRequestFetcherTest, HttpErrors) {
  EXPECT_EQ(PrefetchRequestStatus::kShouldSuspendNotImplemented,
            RunFetcherWithHttpError(net::HTTP_NOT_IMPLEMENTED));
  EXPECT_EQ(PrefetchRequestStatus::kShouldSuspendForbidden,
            RunFetcherWithHttpError(net::HTTP_FORBIDDEN));

  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_BAD_REQUEST));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_CONFLICT));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_BAD_GATEWAY));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithHttpError(net::HTTP_GATEWAY_TIMEOUT));

  SetEmptyRequest(false);
  EXPECT_EQ(PrefetchRequestStatus::kShouldSuspendNewlyForbiddenByOPS,
            RunFetcherWithHttpErrorAndData(net::HTTP_FORBIDDEN,
                                           "request forbidden by OPS"));
  SetEmptyRequest(true);
  EXPECT_EQ(PrefetchRequestStatus::kShouldSuspendForbiddenByOPS,
            RunFetcherWithHttpErrorAndData(net::HTTP_FORBIDDEN,
                                           "request forbidden by OPS"));
}

TEST_F(PrefetchRequestFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(PrefetchRequestFetcherTest, EmptyRequestSuccess) {
  std::string data;
  SetEmptyRequest(true);
  EXPECT_EQ(PrefetchRequestStatus::kEmptyRequestSuccess,
            RunFetcherWithData("Any data.", &data));
  EXPECT_FALSE(data.empty());
}

TEST_F(PrefetchRequestFetcherTest, NonEmptyRequestSuccess) {
  std::string data;
  SetEmptyRequest(false);
  EXPECT_EQ(PrefetchRequestStatus::kSuccess,
            RunFetcherWithData("Any data.", &data));
  EXPECT_FALSE(data.empty());
}

}  // namespace offline_pages
