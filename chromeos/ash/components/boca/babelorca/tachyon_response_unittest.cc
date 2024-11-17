// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"

#include <memory>
#include <optional>
#include <string>

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

struct TachyonResponseTestCase {
  std::string test_name;
  int net_error;
  std::optional<int> http_status_code;
  std::string response_body;
  TachyonResponse::Status expected_status;
};

using TachyonResponseTest = testing::TestWithParam<TachyonResponseTestCase>;

TEST(TachyonResponseTest, InternalErrorStatus) {
  TachyonResponse response(TachyonResponse::Status::kInternalError);
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status(), TachyonResponse::Status::kInternalError);
  EXPECT_THAT(response.error_message(), testing::IsEmpty());
  EXPECT_THAT(response.response_body(), testing::IsEmpty());
}

TEST(TachyonResponseTest, OkRpcCode) {
  TachyonResponse response(/*rpc_code=*/0);
  EXPECT_TRUE(response.ok());
  EXPECT_EQ(response.status(), TachyonResponse::Status::kOk);
  EXPECT_THAT(response.error_message(), testing::IsEmpty());
  EXPECT_THAT(response.response_body(), testing::IsEmpty());
}

TEST(TachyonResponseTest, AuthErrorRpcCode) {
  constexpr char kAuthErrorMessage[] = "auth error";
  TachyonResponse response(/*rpc_code=*/16,
                           /*error_message=*/kAuthErrorMessage);
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status(), TachyonResponse::Status::kAuthError);
  EXPECT_THAT(response.error_message(), testing::StrEq(kAuthErrorMessage));
  EXPECT_THAT(response.response_body(), testing::IsEmpty());
}

TEST(TachyonResponseTest, OtherRpcCode) {
  constexpr char kResourcedMessage[] = "resource exhausted";
  TachyonResponse response(/*rpc_code=*/8, /*error_message=*/kResourcedMessage);
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status(), TachyonResponse::Status::kHttpError);
  EXPECT_THAT(response.error_message(), testing::StrEq(kResourcedMessage));
  EXPECT_THAT(response.response_body(), testing::IsEmpty());
}

TEST_P(TachyonResponseTest, HttpHeader) {
  TachyonResponse response(
      GetParam().net_error, GetParam().http_status_code,
      std::make_unique<std::string>(GetParam().response_body));
  EXPECT_EQ(response.status(), GetParam().expected_status);
  EXPECT_THAT(response.response_body(),
              testing::StrEq(GetParam().response_body));
  EXPECT_THAT(response.error_message(), testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    TachyonResponseTestSuiteInstantiation,
    TachyonResponseTest,
    testing::ValuesIn<TachyonResponseTestCase>({
        {"NetError", net::ERR_TOO_MANY_RETRIES, std::nullopt, "",
         TachyonResponse::Status::kNetworkError},
        {"NoHttpStatus", net::OK, std::nullopt, "",
         TachyonResponse::Status::kInternalError},
        {"AuthError", net::ERR_HTTP_RESPONSE_CODE_FAILURE,
         net::HttpStatusCode::HTTP_UNAUTHORIZED, "",
         TachyonResponse::Status::kAuthError},
        {"OtherHttpError", net::ERR_HTTP_RESPONSE_CODE_FAILURE,
         net::HttpStatusCode::HTTP_PRECONDITION_FAILED, "",
         TachyonResponse::Status::kHttpError},
        {"Success", net::ERR_HTTP_RESPONSE_CODE_FAILURE,
         net::HttpStatusCode::HTTP_OK, "response",
         TachyonResponse::Status::kOk},
    }),
    [](const testing::TestParamInfo<TachyonResponseTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash::babelorca
