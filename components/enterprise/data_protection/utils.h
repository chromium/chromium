// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_PROTECTION_UTILS_H_
#define COMPONENTS_ENTERPRISE_DATA_PROTECTION_UTILS_H_

#include <string>

#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"

namespace enterprise_data_protection {

// A structure holding all data protection settings for a given URL.
struct UrlSettings {
  UrlSettings();
  UrlSettings(const UrlSettings&);
  UrlSettings& operator=(const UrlSettings&);
  ~UrlSettings();

  // The watermark text that should apply to tabs showing this URL.  An empty
  // string means no watermark should be shown.
  std::string watermark_text;

  bool allow_screenshots = true;

  bool operator==(const UrlSettings& other) const;

  // URL settings that imply no data protections are enabled.
  static const UrlSettings& None();
};

// Returns a `UrlSettings` object representing the restrictions
// applied by a `RTLookupResponse`.
UrlSettings GetUrlSettings(
    const std::string& identifier,
    const safe_browsing::RTLookupResponse* rt_lookup_response);

// Return the watermark string to display if present in `threat_info`.
std::string GetWatermarkString(
    const std::string& identifier,
    const safe_browsing::MatchedUrlNavigationRule& rule);

}  // namespace enterprise_data_protection

#endif  // COMPONENTS_ENTERPRISE_DATA_PROTECTION_UTILS_H_
