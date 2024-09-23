// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_H_

#include <string>

#include "url/gurl.h"

namespace autofill {
class SavePasswordProgressLogger;
}  // namespace autofill

class PrefService;

namespace password_manager {

enum class LeakDetectionInitiator;

// The base class for requests for checking if {username, password} pair was
// leaked in the internet.
class LeakDetectionCheck {
 public:
  LeakDetectionCheck() = default;
  virtual ~LeakDetectionCheck() = default;

  // Not copyable or movable
  LeakDetectionCheck(const LeakDetectionCheck&) = delete;
  LeakDetectionCheck& operator=(const LeakDetectionCheck&) = delete;
  LeakDetectionCheck(LeakDetectionCheck&&) = delete;
  LeakDetectionCheck& operator=(LeakDetectionCheck&&) = delete;

  // Starts checking |username| and |password| pair asynchronously.
  // |url| is used later for presentation in the UI but not for actual business
  // logic. The method should be called only once per lifetime of the object.
  virtual void Start(LeakDetectionInitiator initiator,
                     const GURL& url,
                     std::u16string username,
                     std::u16string password) = 0;

  // Determines whether the leak check can be started depending on `prefs`. Will
  // use `logger` for logging if non-null. Leak check can be blocked if
  // |origin_url| appears on SafeBrowsingAllowlistDomains setting.
  // It should be set to either:
  // - URL of the frame that contains the submitted password form
  // - top frame URL
  // - credential URL if none of above are available (ie in
  // BulkLeakCheckServiceAdapter::OnEdited)
  static bool CanStartLeakCheck(
      const PrefService& prefs,
      const GURL& form_url,
      std::unique_ptr<autofill::SavePasswordProgressLogger> logger);

 private:
  // Leak check is blocked for domains from SafeBrowsingAllowlistDomains policy
  static bool IsURLBlockedByPolicy(
      const PrefService& prefs,
      const GURL& form_url,
      autofill::SavePasswordProgressLogger* logger);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_H_
