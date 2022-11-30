// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_NETWORK_CHECKER_H_
#define CHROME_CHROME_CLEANER_LOGGING_NETWORK_CHECKER_H_

#include "base/time/time.h"
#include "url/gurl.h"

namespace chrome_cleaner {

// Base class that provides methods to test whether Safe Browsing is reachable
// or not, and to wait for it to be reachable.
class NetworkChecker {
 public:
  virtual ~NetworkChecker() = default;

  // Returns whether Safe Browsing is currently reachable or not.
  virtual bool IsSafeBrowsingReachable(const GURL& upload_url) const = 0;

  // Blocks until Safe Browsing is reachable, up to |wait_time|, whichever
  // happens first. Returns whether Safe Browsing is reachable or not when this
  // function exits.
  virtual bool WaitForSafeBrowsing(const GURL& upload_url,
                                   const base::TimeDelta& wait_time) = 0;

  // Cancels all current and future waits, to speed up system shutdown.
  virtual void CancelWaitForShutdown() = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_NETWORK_CHECKER_H_
