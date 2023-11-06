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
  // use `logger` for logging if non-null.
  static bool CanStartLeakCheck(
      const PrefService& prefs,
      std::unique_ptr<autofill::SavePasswordProgressLogger> logger);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_H_
