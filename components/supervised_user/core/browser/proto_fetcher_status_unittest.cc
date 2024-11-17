// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher_status.h"

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

// Tests the functionality of status helper methods.
class ProtoFetcherStatusTest : public testing::Test {};

TEST_F(ProtoFetcherStatusTest, CreateOKStatus) {
  ProtoFetcherStatus ok_status = ProtoFetcherStatus::Ok();
  EXPECT_TRUE(ok_status.IsOk());
  EXPECT_FALSE(ok_status.IsTransientError());
  EXPECT_FALSE(ok_status.IsPersistentError());
  EXPECT_EQ(ok_status.state(), ProtoFetcherStatus::State::OK);
  EXPECT_EQ(ok_status.http_status_or_net_error().value(), 0);
}

TEST_F(ProtoFetcherStatusTest, CreateAuthErrorStatusWithTransientError) {
  ProtoFetcherStatus error_status = ProtoFetcherStatus::GoogleServiceAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));
  EXPECT_FALSE(error_status.IsOk());
  EXPECT_TRUE(error_status.IsTransientError());
  EXPECT_FALSE(error_status.IsPersistentError());
  EXPECT_EQ(error_status.state(),
            ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR);
  EXPECT_EQ(error_status.http_status_or_net_error().value(), 0);
  EXPECT_EQ(error_status.google_service_auth_error().state(),
            GoogleServiceAuthError::State::CONNECTION_FAILED);
}

TEST_F(ProtoFetcherStatusTest, CreateAuthErrorStatusWithPersistentError) {
  ProtoFetcherStatus error_status =
      ProtoFetcherStatus::GoogleServiceAuthError(GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(error_status.IsOk());
  EXPECT_FALSE(error_status.IsTransientError());
  EXPECT_TRUE(error_status.IsPersistentError());
  EXPECT_EQ(error_status.state(),
            ProtoFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR);
  EXPECT_EQ(error_status.http_status_or_net_error().value(), 0);
  EXPECT_EQ(error_status.google_service_auth_error().state(),
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
}

TEST_F(ProtoFetcherStatusTest, CreateHttpStatusOrNetError) {
  ProtoFetcherStatus error_status =
      ProtoFetcherStatus::HttpStatusOrNetError(net::ERR_IO_PENDING);
  EXPECT_FALSE(error_status.IsOk());
  EXPECT_TRUE(error_status.IsTransientError());
  EXPECT_FALSE(error_status.IsPersistentError());
  EXPECT_EQ(error_status.state(),
            ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  EXPECT_EQ(error_status.http_status_or_net_error().value(),
            net::ERR_IO_PENDING);
}

TEST_F(ProtoFetcherStatusTest, CreateHttpAuthError) {
  ProtoFetcherStatus error_status =
      ProtoFetcherStatus::HttpStatusOrNetError(net::HTTP_UNAUTHORIZED);
  EXPECT_FALSE(error_status.IsOk());
  EXPECT_FALSE(error_status.IsTransientError());
  EXPECT_TRUE(error_status.IsPersistentError());
  EXPECT_EQ(error_status.state(),
            ProtoFetcherStatus::State::HTTP_STATUS_OR_NET_ERROR);
  EXPECT_EQ(error_status.http_status_or_net_error().value(),
            net::HTTP_UNAUTHORIZED);
}

TEST_F(ProtoFetcherStatusTest, CreateInvalidResponse) {
  ProtoFetcherStatus error_status = ProtoFetcherStatus::InvalidResponse();
  EXPECT_FALSE(error_status.IsOk());
  EXPECT_FALSE(error_status.IsTransientError());
  EXPECT_TRUE(error_status.IsPersistentError());
  EXPECT_EQ(error_status.state(), ProtoFetcherStatus::State::INVALID_RESPONSE);
  EXPECT_EQ(error_status.http_status_or_net_error().value(), 0);
}

}  // namespace supervised_user
