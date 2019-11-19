// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace proximity_auth {

// Interface for setting and getting persistent user preferences. There is an
// implementation for a logged in profile and the device local state when the
// user is logged out.
class ProximityAuthPrefManager {
 public:
  ProximityAuthPrefManager() {}
  virtual ~ProximityAuthPrefManager() {}

  // Returns true if EasyUnlock is allowed. Note: there is no corresponding
  // setter because this pref is pushed through an enterprise policy. Note that
  // this pref completely disables EasyUnlock, hiding even the UI. See
  // IsEasyUnlockEnabled() for comparison.
  virtual bool IsEasyUnlockAllowed() const = 0;

  // Returns true if EasyUnlock is enabled, i.e. the user has gone through the
  // setup flow and has at least one phone as an unlock key. Compare to
  // IsEasyUnlockAllowed(), which completely removes the feature from existence.
  virtual void SetIsEasyUnlockEnabled(bool is_easy_unlock_enabled) const = 0;
  virtual bool IsEasyUnlockEnabled() const = 0;

  // Returns true if EasyUnlock has ever been enabled, regardless of whether the
  // feature is currently enabled or disabled. Compare to IsEasyUnlockEnabled(),
  // which flags the latter case.
  virtual void SetEasyUnlockEnabledStateSet() const = 0;
  virtual bool IsEasyUnlockEnabledStateSet() const = 0;

  // Setter and getter for the timestamp of the last time the promotion was
  // shown to the user.
  virtual void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms) = 0;
  virtual int64_t GetLastPromotionCheckTimestampMs() const = 0;

  // Setter and getter for the number of times the promotion was shown to the
  // user.
  virtual void SetPromotionShownCount(int count) = 0;
  virtual int GetPromotionShownCount() const = 0;

  // Getter for whether EasyUnlock is allowed for ChromeOS login (in addition to
  // screen lock).
  virtual bool IsChromeOSLoginAllowed() const = 0;

  // Setter and getter for whether EasyUnlock is enabled for ChromeOS login (in
  // addition to screen lock).
  virtual void SetIsChromeOSLoginEnabled(bool is_enabled) = 0;
  virtual bool IsChromeOSLoginEnabled() const = 0;

  // Setter and getter for whether the "Signin with Smart Lock is disabled"
  // message on the login screen has been shown.
  virtual void SetHasShownLoginDisabledMessage(bool has_shown) = 0;
  virtual bool HasShownLoginDisabledMessage() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthPrefManager);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H_
