// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_H_

#include "base/time/time.h"

namespace safe_browsing {

// Observes TailoredSecurityService bit and handles notification calls.
class TailoredSecurityServiceObserver {
 public:
  // Called when the Tailored Security bit changed to |enabled|, and provides
  // the last time it was changed (not including the current update).
  virtual void OnTailoredSecurityBitChanged(bool enabled,
                                            base::Time previous_update) {}

  // Called when sync notification message needs to be shown.
  virtual void OnSyncNotificationMessageRequest(bool is_enabled) {}

  // Called when the service is being destroyed.
  virtual void OnTailoredSecurityServiceDestroyed() {}

 protected:
  virtual ~TailoredSecurityServiceObserver() = default;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_H_
