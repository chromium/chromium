// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/web_payments_telemetry.h"

#include <cstdint>
#include <optional>
#include <string>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/third_party/mozilla/url_parse.h"

namespace payments {

namespace {

// Histogram and UKM names.
constexpr char kChallengeRequestHistogram[] =
    "Payments.ThreeDSecure.ChallengeRequest";
constexpr char kChallengeResponseHistogram[] =
    "Payments.ThreeDSecure.ChallengeResponse";

// Query parameter keys.
constexpr char kChallengeRequestKey[] = "cReq";
constexpr char kChallengeResponseKey[] = "cRes";

// Challenge Response JSON keys.
constexpr char kTransactionStatusKey[] = "transStatus";

ThreeDSecureTransactionStatus GetThreeDSecureTransactionStatus(
    std::string_view trans_status) {
  // TransStatus should always be a single character.
  if (trans_status.size() != 1) {
    return ThreeDSecureTransactionStatus::kUnknown;
  }

  switch (base::ToUpperASCII(trans_status[0])) {
    case 'Y':
      return ThreeDSecureTransactionStatus::kSuccess;
    case 'N':
      return ThreeDSecureTransactionStatus::kDenied;
    case 'U':
      return ThreeDSecureTransactionStatus::kCouldNotBePerformed;
    case 'A':
      return ThreeDSecureTransactionStatus::kAttemptsProcessingPerformed;
    case 'C':
      return ThreeDSecureTransactionStatus::kChallengeRequired;
    case 'D':
      return ThreeDSecureTransactionStatus::kChallengeRequiredDecoupled;
    case 'R':
      return ThreeDSecureTransactionStatus::kRejected;
    case 'I':
      return ThreeDSecureTransactionStatus::kInformationalOnly;
    case 'S':
      return ThreeDSecureTransactionStatus::kChallengeUsingSPC;
    default:
      return ThreeDSecureTransactionStatus::kUnknown;
  }
}

ThreeDSecureTransactionStatus ParseChallengeResponse(
    std::string_view cres_val) {
  std::string decoded_json;
  std::string unescaped_cres = base::UnescapeBinaryURLComponent(cres_val);

  if (!base::Base64UrlDecode(unescaped_cres,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded_json)) {
    // The `cres` value could be formatted as JWE (JSON Web Encryption)
    // which is distinguishable from the other formats by the presence
    // of periods in the string. In this case, we won't be able to parse
    // it, so we record it as encrypted JSON.
    return unescaped_cres.find('.') != std::string::npos
               ? ThreeDSecureTransactionStatus::kJSONEncrypted
               : ThreeDSecureTransactionStatus::kUnknown;
  }

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(decoded_json, base::JSON_PARSE_RFC);
  if (!dict) {
    return ThreeDSecureTransactionStatus::kUnknown;
  }

  for (const auto [key, value] : *dict) {
    if (base::EqualsCaseInsensitiveASCII(key, kTransactionStatusKey) &&
        value.is_string()) {
      return GetThreeDSecureTransactionStatus(value.GetString());
    }
  }

  return ThreeDSecureTransactionStatus::kUnknown;
}

}  // namespace

void RecordThreeDSecureTelemetryFromFormData(std::string_view form_data,
                                             ukm::SourceId source_id) {
  url::Component query(form_data);
  url::Component key;
  url::Component value;
  while (url::ExtractQueryKeyValue(form_data, &query, &key, &value)) {
    std::string_view key_view = key.AsViewOn(form_data);
    if (base::EqualsCaseInsensitiveASCII(key_view, kChallengeRequestKey)) {
      base::UmaHistogramBoolean(kChallengeRequestHistogram, true);
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest(source_id)
          .SetChallengeRequest(true)
          .Record(ukm::UkmRecorder::Get());
    } else if (base::EqualsCaseInsensitiveASCII(key_view,
                                                kChallengeResponseKey)) {
      ThreeDSecureTransactionStatus result =
          ParseChallengeResponse(value.AsViewOn(form_data));
      base::UmaHistogramEnumeration(kChallengeResponseHistogram, result);
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse(source_id)
          .SetChallengeResponse(static_cast<int64_t>(result))
          .Record(ukm::UkmRecorder::Get());
    }
  }
}

}  // namespace payments
