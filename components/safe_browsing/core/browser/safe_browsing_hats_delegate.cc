// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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

}  // namespace

// static
bool SafeBrowsingHatsDelegate::IsSurveyCandidate(
    const ClientSafeBrowsingReportRequest::ReportType& report_type,
    const std::string& report_type_filter,
    const bool did_proceed,
    const std::string& did_proceed_filter) {
  if (!MatchFound(ClientSafeBrowsingReportRequest::ReportType_Name(report_type),
                  report_type_filter)) {
    return false;
  }
  if (!MatchFound(did_proceed ? "TRUE" : "FALSE", did_proceed_filter)) {
    return false;
  }
  return true;
}

}  // namespace safe_browsing
