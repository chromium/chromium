// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/gemini/get_gemini_status_request.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/test/bind.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

constexpr char kTestGaiaId[] = "12345";

TEST(GetGeminiStatusRequestTest, GetRelativeUrl) {
  GetGeminiStatusRequest request(kTestGaiaId, base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(), "v1/users/12345:getGeminiStatus");
}

TEST(GetGeminiStatusRequestTest, GetRequestType) {
  GetGeminiStatusRequest request(kTestGaiaId, base::DoNothing());
  EXPECT_EQ(request.GetRequestType(), google_apis::HttpRequestMethod::kGet);
}

TEST(GetGeminiStatusRequestTest, GetRequestBody) {
  GetGeminiStatusRequest request(kTestGaiaId, base::DoNothing());
  EXPECT_EQ(request.GetRequestBody(), std::nullopt);
}

TEST(GetGeminiStatusRequestTest, OnSuccessEnabled) {
  std::optional<bool> callback_result;
  GetGeminiStatusRequest request(
      kTestGaiaId, base::BindLambdaForTesting(
                       [&callback_result](std::optional<bool> result) {
                         callback_result = result;
                       }));

  base::DictValue response_dict;
  response_dict.Set(kGeminiStatus, kGeminiEnabled);
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  ASSERT_TRUE(callback_result.has_value());
  EXPECT_TRUE(callback_result.value());
}

TEST(GetGeminiStatusRequestTest, OnSuccessDisabled) {
  std::optional<bool> callback_result;
  GetGeminiStatusRequest request(
      kTestGaiaId, base::BindLambdaForTesting(
                       [&callback_result](std::optional<bool> result) {
                         callback_result = result;
                       }));

  base::DictValue response_dict;
  response_dict.Set(kGeminiStatus, kGeminiDisabled);
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  ASSERT_TRUE(callback_result.has_value());
  EXPECT_FALSE(callback_result.value());
}

TEST(GetGeminiStatusRequestTest, OnSuccessInvalidValue) {
  std::optional<bool> callback_result = true;
  GetGeminiStatusRequest request(
      kTestGaiaId, base::BindLambdaForTesting(
                       [&callback_result](std::optional<bool> result) {
                         callback_result = result;
                       }));

  base::DictValue response_dict;
  response_dict.Set(kGeminiStatus, "INVALID_STATUS_VALUE");
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  EXPECT_FALSE(callback_result.has_value());
}

TEST(GetGeminiStatusRequestTest, OnSuccessMissingStatusKey) {
  std::optional<bool> callback_result = true;
  GetGeminiStatusRequest request(
      kTestGaiaId, base::BindLambdaForTesting(
                       [&callback_result](std::optional<bool> result) {
                         callback_result = result;
                       }));

  base::DictValue response_dict;
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  EXPECT_FALSE(callback_result.has_value());
}

TEST(GetGeminiStatusRequestTest, OnSuccessInvalidResponseStructure) {
  std::optional<bool> callback_result = true;
  GetGeminiStatusRequest request(
      kTestGaiaId, base::BindLambdaForTesting(
                       [&callback_result](std::optional<bool> result) {
                         callback_result = result;
                       }));

  request.OnSuccess(std::make_unique<base::Value>("not_a_dictionary"));

  EXPECT_FALSE(callback_result.has_value());
}

TEST(GetGeminiStatusRequestTest, OnError) {
  bool called = false;
  std::optional<bool> callback_result = true;
  GetGeminiStatusRequest request(
      kTestGaiaId, base::BindLambdaForTesting(
                       [&callback_result, &called](std::optional<bool> result) {
                         called = true;
                         callback_result = result;
                       }));

  request.OnError(google_apis::ApiErrorCode::HTTP_NOT_FOUND);

  EXPECT_TRUE(called);
  EXPECT_FALSE(callback_result.has_value());
}

}  // namespace
}  // namespace ash::boca
