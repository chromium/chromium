// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/fingerprint_parser.h"

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/payments/core/error_logger.h"

namespace payments {
namespace {

bool IsUpperCaseHexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

uint8_t HexDigitToByte(char c) {
  DCHECK(IsUpperCaseHexDigit(c));
  return base::checked_cast<uint8_t>(c >= '0' && c <= '9' ? c - '0'
                                                          : c - 'A' + 10);
}

}  // namespace

std::vector<uint8_t> FingerprintStringToByteArray(const std::string& input,
                                                  const ErrorLogger& log) {
  std::vector<uint8_t> output;
  if (!base::IsStringASCII(input)) {
    log.Error("Fingerprint should be an ASCII string.");
    return output;
  }

  const size_t kLength = 32 * 3 - 1;
  if (input.size() != kLength) {
    log.Error(base::StringPrintf(
        "Fingerprint \"%s\" should contain exactly %zu characters.",
        (input.size() > kLength ? (input.substr(0, kLength) + "...") : input)
            .c_str(),
        kLength));
    return output;
  }

  for (size_t i = 0; i < input.size(); i += 3) {
    if (i < input.size() - 2 && input[i + 2] != ':') {
      log.Error(
          base::StringPrintf("Bytes in fingerprint \"%s\" should be separated "
                             "by \":\" characters.",
                             input.c_str()));
      output.clear();
      return output;
    }

    char big_end = input[i];
    char little_end = input[i + 1];
    if (!IsUpperCaseHexDigit(big_end) || !IsUpperCaseHexDigit(little_end)) {
      log.Error(base::StringPrintf(
          "Bytes in fingerprint \"%s\" should be upper case hex digits 0-9 and "
          "A-F.",
          input.c_str()));
      output.clear();
      return output;
    }

    output.push_back(HexDigitToByte(big_end) * static_cast<uint8_t>(16) +
                     HexDigitToByte(little_end));
  }

  return output;
}

}  // namespace payments
