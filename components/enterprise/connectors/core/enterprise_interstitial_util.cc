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
        unsafe_resources_) {
  std::u16string custom_message = u"";

  // Threat info already ordered by severity
  if (!unsafe_resources_.empty() &&
      !unsafe_resources_[0].rt_lookup_response.threat_info().empty()) {
    custom_message = GetCustomMessageFromNavigationRule(
        unsafe_resources_[0]
            .rt_lookup_response.threat_info()[0]
            .matched_url_navigation_rule());
  }
  return custom_message;
}

}  // namespace enterprise_connectors
