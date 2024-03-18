// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"

#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"

namespace safe_browsing {

namespace {

bool MatchFound(const std::string& report_value,
                const std::string& filter_values) {
  std::vector<std::string> filter_values_list = base::SplitString(
      filter_values, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (const std::string& filter_value : filter_values_list) {
    if (base::EqualsCaseInsensitiveASCII(report_value, filter_value)) {
      return true;
    }
  }
  return false;
}

// Return the CSBRR report type for |threat_type|. We are only
// concerned with report types that HaTS will target.
std::string ThreatTypeToReportType(SBThreatType threat_type) {
  switch (threat_type) {
    case SBThreatType::SB_THREAT_TYPE_URL_PHISHING:
      return "URL_PHISHING";
    case SBThreatType::SB_THREAT_TYPE_URL_MALWARE:
      return "URL_MALWARE";
    case SBThreatType::SB_THREAT_TYPE_URL_UNWANTED:
      return "URL_UNWANTED";
    case SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return "URL_CLIENT_SIDE_PHISHING";
    default:
      return "UNSUPPORTED_THREAT_TYPE";
  }
}

}  // namespace

// static
bool SafeBrowsingHatsDelegate::IsSurveyCandidate(
    const SBThreatType& threat_type,
    const std::string& report_type_filter,
    const bool did_proceed,
    const std::string& did_proceed_filter) {
  if (!MatchFound(ThreatTypeToReportType(threat_type), report_type_filter)) {
    return false;
  }
  if (!MatchFound(did_proceed ? "TRUE" : "FALSE", did_proceed_filter)) {
    return false;
  }
  return true;
}

}  // namespace safe_browsing
