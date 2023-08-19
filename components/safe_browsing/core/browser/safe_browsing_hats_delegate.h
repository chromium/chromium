// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_HATS_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_HATS_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

// Named string-valued Product Specific Data.
typedef std::map<std::string, std::string> SurveyStringData;

class SafeBrowsingHatsDelegate {
 public:
  SafeBrowsingHatsDelegate() = default;
  virtual ~SafeBrowsingHatsDelegate() = default;

  SafeBrowsingHatsDelegate(const SafeBrowsingHatsDelegate&) = delete;
  SafeBrowsingHatsDelegate& operator=(const SafeBrowsingHatsDelegate&) = delete;

  // A wrapper for the HaTS service LaunchSurvey method.
  virtual void LaunchRedWarningSurvey(
      // Named string values sent with user survey responses.
      const SurveyStringData& product_specific_string_data = {}) = 0;

  // Determines if the associated user is a candidate for a HaTS survey.
  static bool IsSurveyCandidate(const SBThreatType& threat_type,
                                const std::string& report_type_filter,
                                const bool did_proceed,
                                const std::string& did_proceed_filter);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_HATS_DELEGATE_H_
