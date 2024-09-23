// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_test_utils.h"

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_constants.h"

namespace ash::enhanced_network_tts {
namespace {

bool HasOneDecimalDigit(std::optional<double> rate) {
  if (!rate.has_value())
    return false;
  return std::abs(static_cast<int>(rate.value() * 10) - rate.value() * 10) <
         kDoubleCompareAccuracy;
}

}  // namespace

std::string CreateCorrectRequest(const std::string& input_text,
                                 float rate,
                                 const std::string& voice_name,
                                 const std::string& lang) {
  return base::StringPrintf(kFullRequestTemplate, rate, lang.c_str(),
                            input_text.c_str(), lang.c_str(),
                            voice_name.c_str());
}

std::string CreateCorrectRequest(const std::string& input_text, float rate) {
  return base::StringPrintf(kSimpleRequestTemplate, rate, input_text.c_str());
}

std::string CreateServerResponse(const std::vector<uint8_t>& expected_output) {
  std::string encoded_output(expected_output.begin(), expected_output.end());
  encoded_output = base::Base64Encode(encoded_output);
  return base::StringPrintf(kTemplateResponse, encoded_output.c_str());
}

bool AreRequestsEqual(const std::string& json_a, const std::string& json_b) {
  std::optional<base::Value> parsed_a = base::JSONReader::Read(json_a);
  std::optional<base::Value> parsed_b = base::JSONReader::Read(json_b);
  base::Value::Dict& dict_a = parsed_a->GetDict();
  base::Value::Dict& dict_b = parsed_b->GetDict();

  const std::optional<double> rate_a =
      dict_a.FindDoubleByDottedPath(kSpeechFactorPath);
  const std::optional<double> rate_b =
      dict_b.FindDoubleByDottedPath(kSpeechFactorPath);
  // Speech rates should have only one decimal digit.
  if (!HasOneDecimalDigit(rate_a) || !HasOneDecimalDigit(rate_b))
    return false;
  // Compare the speech rates with |kDoubleCompareAccuracy|.
  if (std::abs(rate_a.value() - rate_b.value()) > kDoubleCompareAccuracy)
    return false;

  // Compare the dicts without the speech rates.
  dict_a.RemoveByDottedPath(kSpeechFactorPath);
  dict_b.RemoveByDottedPath(kSpeechFactorPath);
  return dict_a == dict_b;
}

}  // namespace ash::enhanced_network_tts
