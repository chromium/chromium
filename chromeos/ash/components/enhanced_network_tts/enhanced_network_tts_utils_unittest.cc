// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_utils.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_constants.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::enhanced_network_tts {

using EnhancedNetworkTtsUtilsTest = testing::Test;

TEST_F(EnhancedNetworkTtsUtilsTest, FormatJsonRequest) {
  const std::string utterance = "Hello, World!";
  const float rate = 1.0;
  const std::string voice = "test_name";
  const std::string language = "en-US";
  const std::string expected_text =
      CreateCorrectRequest(utterance, rate, voice, language);
  const std::string formatted_text = FormatJsonRequest(
      mojom::TtsRequest::New(utterance, rate, voice, language));
  EXPECT_TRUE(AreRequestsEqual(formatted_text, expected_text));
}

TEST_F(EnhancedNetworkTtsUtilsTest, FormatJsonRequestWithUtteranceOnly) {
  const std::string utterance = "Hello, World!";
  const float rate = 1.0;
  const std::string expected_text = CreateCorrectRequest(utterance, rate);
  const std::string formatted_text = FormatJsonRequest(
      mojom::TtsRequest::New(utterance, rate, std::nullopt, std::nullopt));

  EXPECT_TRUE(AreRequestsEqual(formatted_text, expected_text));
}

TEST_F(EnhancedNetworkTtsUtilsTest, FormatJsonRequestWithQuotes) {
  const std::string quotes = "Hello, \"World!\"";
  const std::string quotes_for_template = "Hello, \\\"World!\\\"";
  const float rate = 1.0;
  const std::string expected_text =
      CreateCorrectRequest(quotes_for_template, rate);
  const std::string formatted_text = FormatJsonRequest(
      mojom::TtsRequest::New(quotes, rate, std::nullopt, std::nullopt));
  EXPECT_TRUE(AreRequestsEqual(formatted_text, expected_text));
}

TEST_F(EnhancedNetworkTtsUtilsTest, FormatJsonRequestWithDifferentRates) {
  std::string utterance = "Rate will be capped to kMaxRate";
  float rate = kMaxRate + 1.0f;
  std::string expected_text = CreateCorrectRequest(utterance, kMaxRate);
  std::string formatted_text = FormatJsonRequest(
      mojom::TtsRequest::New(utterance, rate, std::nullopt, std::nullopt));
  EXPECT_TRUE(AreRequestsEqual(formatted_text, expected_text));

  utterance = "Rate will be floored to kMinRate";
  rate = kMinRate - 0.1f;
  expected_text = CreateCorrectRequest(utterance, kMinRate);
  formatted_text = FormatJsonRequest(
      mojom::TtsRequest::New(utterance, rate, std::nullopt, std::nullopt));
  EXPECT_TRUE(AreRequestsEqual(formatted_text, expected_text));

  utterance = "Rate has precision of 0.1";
  rate = 3.5111111;
  expected_text = CreateCorrectRequest(utterance, 3.5f);
  formatted_text = FormatJsonRequest(
      mojom::TtsRequest::New(utterance, rate, std::nullopt, std::nullopt));
  EXPECT_TRUE(AreRequestsEqual(formatted_text, expected_text));
}

TEST_F(EnhancedNetworkTtsUtilsTest, FindTextBreaks) {
  std::u16string utterance = u"";
  int length_limit = 10;
  std::vector<uint16_t> expected_output = {};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  utterance = u"utterance is shorter than length_limit";
  length_limit = 1000;
  expected_output = {37};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  utterance = u"limit is 1";
  length_limit = 1;
  expected_output = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  // Index ref: 012345678901234
  utterance = u"Sent 1! Sent 2!";
  length_limit = 4;
  // 3 = word end of "Sent"
  // 7 = first sentence end
  // 11 = word end of "Sent"
  // 14 = second sentence end
  expected_output = {3, 7, 11, 14};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  // Index ref: 01234567890123456789012
  utterance = u"Sent 1! Sent 2. Sent 3!";
  length_limit = 8;
  // 7 = first sentence end
  // 15 = second sentence end
  // 22 = third sentence end
  expected_output = {7, 15, 22};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  // Index ref: 01234567890123456
  utterance = u"Sent 1! Sent two!";
  length_limit = 3;
  // 2 = over length limit at char 'n'
  // 5 = word end of "1"
  // 7 = first sentence end
  // 10 = over length limit at char 'n'
  // 11 = word end of "Sent"
  // 14 = over length limit at char 'w'
  // 16 = second sentence end
  expected_output = {2, 5, 7, 10, 11, 14, 16};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);
}

TEST_F(EnhancedNetworkTtsUtilsTest, GetResultOnError) {
  mojom::TtsResponsePtr result =
      GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(),
            mojom::TtsRequestError::kReceivedUnexpectedData);

  result = GetResultOnError(mojom::TtsRequestError::kServerError);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(), mojom::TtsRequestError::kServerError);

  result = GetResultOnError(mojom::TtsRequestError::kRequestOverride);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(), mojom::TtsRequestError::kRequestOverride);

  result = GetResultOnError(mojom::TtsRequestError::kEmptyUtterance);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(), mojom::TtsRequestError::kEmptyUtterance);
}

TEST_F(EnhancedNetworkTtsUtilsTest, UnpackJsonResponseSucceed) {
  const std::vector<uint8_t> response_data = {1, 2, 5};
  const std::string server_response = CreateServerResponse(response_data);
  std::optional<base::Value> json = base::JSONReader::Read(server_response);

  mojom::TtsResponsePtr result = UnpackJsonResponse(
      json->GetList(), 0 /* start_index */, true /* is_last_request */);

  EXPECT_TRUE(result->is_data());
  EXPECT_EQ(result->get_data()->audio, std::vector<uint8_t>({1, 2, 5}));
  EXPECT_TRUE(result->get_data()->last_data);
  EXPECT_EQ(result->get_data()->time_info[0]->text_offset, 0u);

  result = UnpackJsonResponse(json->GetList(), 4 /* start_index */,
                              true /* is_last_request */);

  EXPECT_TRUE(result->is_data());
  EXPECT_EQ(result->get_data()->audio, std::vector<uint8_t>({1, 2, 5}));
  EXPECT_TRUE(result->get_data()->last_data);
  EXPECT_EQ(result->get_data()->time_info[0]->text_offset, 4u);

  result = UnpackJsonResponse(json->GetList(), 4 /* start_index */,
                              false /* is_last_request */);

  EXPECT_TRUE(result->is_data());
  EXPECT_EQ(result->get_data()->audio, std::vector<uint8_t>({1, 2, 5}));
  EXPECT_FALSE(result->get_data()->last_data);
  EXPECT_EQ(result->get_data()->time_info[0]->text_offset, 4u);
}

TEST_F(EnhancedNetworkTtsUtilsTest,
       UnpackJsonResponseFailsWithWrongResponseFormat) {
  const std::string encoded_response = "[{}, {}, {}]";
  std::optional<base::Value> json = base::JSONReader::Read(encoded_response);

  mojom::TtsResponsePtr result = UnpackJsonResponse(
      json->GetList(), 0 /* start_index */, true /* is_last_request */);

  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(),
            mojom::TtsRequestError::kReceivedUnexpectedData);
}

TEST_F(EnhancedNetworkTtsUtilsTest,
       UnpackJsonResponseFailsWithWrongDataFormat) {
  // The response data is not correctly base64 encoded, but is a valid JSON string embedded
  // within a valid JSON message (kTemplateResponse). It should still be rejected due to not
  // being valid base64.
  const std::string encoded_response =
      base::StringPrintf(kTemplateResponse, "a b");
  std::optional<base::Value> json = base::JSONReader::Read(encoded_response);

  mojom::TtsResponsePtr result = UnpackJsonResponse(
      json->GetList(), 0 /* start_index */, true /* is_last_request */);

  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(),
            mojom::TtsRequestError::kReceivedUnexpectedData);
}

}  // namespace ash::enhanced_network_tts
