// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/enterprise_interstitial_util.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/safe_browsing/core/common/features.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

std::u16string GetCustomMessageFromNavigationRule(
    const safe_browsing::MatchedUrlNavigationRule& rule) {
  std::u16string custom_message = u"";
  if (rule.has_custom_message()) {
    for (const auto& custom_segment :
         rule.custom_message().message_segments()) {
      if (custom_segment.has_link() && GURL(custom_segment.link()).is_valid()) {
        base::StrAppend(&custom_message,
                        {u"<a target=\"_blank\" href=\"",
                         base::UTF8ToUTF16(custom_segment.link()), u"\">",
                         base::UTF8ToUTF16(custom_segment.text()), u"</a>"});
      } else {
        base::StrAppend(&custom_message,
                        {base::UTF8ToUTF16(custom_segment.text())});
      }
    }
  }
  return custom_message;
}

}  // namespace

std::u16string GetUrlFilteringCustomMessage(
    const std::vector<security_interstitials::UnsafeResource>&
        unsafe_resources) {
  std::u16string custom_message = u"";
  int highest_severity_verdict = 0;

  if (!unsafe_resources.empty() &&
      !unsafe_resources[0].rt_lookup_response.threat_info().empty()) {
    const auto& threat_infos =
        unsafe_resources[0].rt_lookup_response.threat_info();

    // If it exists, We pick a non-empty custom message from all matched rules
    // at the same highest severity level.
    for (const auto& threat_info : threat_infos) {
      if (!threat_info.has_matched_url_navigation_rule() ||
          !threat_info.has_verdict_type()) {
        continue;
      }
      int current_verdict = static_cast<int>(threat_info.verdict_type());

      // Verdict type is an enum from 1 to 100 representing the danger
      // confidence level.
      if (current_verdict >= highest_severity_verdict) {
        std::u16string message = GetCustomMessageFromNavigationRule(
            threat_info.matched_url_navigation_rule());
        if (!message.empty()) {
          custom_message = message;
        }
        highest_severity_verdict = current_verdict;
      }
    }
  }
  return custom_message;
}

}  // namespace enterprise_connectors
