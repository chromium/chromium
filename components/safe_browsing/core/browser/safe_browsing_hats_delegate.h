// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_HATS_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_HATS_DELEGATE_H_

#include "base/functional/callback.h"

namespace safe_browsing {

class SafeBrowsingHatsDelegate {
 public:
  SafeBrowsingHatsDelegate() = default;
  virtual ~SafeBrowsingHatsDelegate() = default;

  SafeBrowsingHatsDelegate(const SafeBrowsingHatsDelegate&) = delete;
  SafeBrowsingHatsDelegate& operator=(const SafeBrowsingHatsDelegate&) = delete;

  // A wrapper for the HaTS service LaunchSurvey method.
  virtual void LaunchSurvey(
      // Survey identifier.
      const std::string& trigger,
      // Called if survey is shown.
      base::OnceClosure success_callback,
      // Called if survey isn't shown.
      base::OnceClosure failure_callback,
      // Named boolean values sent with user survey responses.
      const std::map<std::string, bool>& product_specific_bits_data = {},
      // Named string values sent with user survey responses.
      const std::map<std::string, std::string>& product_specific_string_data =
          {}) = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_HATS_DELEGATE_H_
