// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_OBSERVER_H_
#define CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_OBSERVER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/user_education/recent_session_tracker.h"

class RecentSessionObserver {
 public:
  RecentSessionObserver();
  RecentSessionObserver(const RecentSessionObserver&) = delete;
  void operator=(const RecentSessionObserver&) = delete;
  virtual ~RecentSessionObserver();

  // Initialization once the tracker is available.
  virtual void Init(RecentSessionTracker& tracker) = 0;

  // Adds a callback that will be called when a new session starts for a
  // low-usage profile.
  base::CallbackListSubscription AddLowUsageSessionCallback(
      base::RepeatingClosure callback);

 protected:
  void NotifyLowUsageSession();

 private:
  FRIEND_TEST_ALL_PREFIXES(LowUsageHelpControllerBrowsertest,
                           PromoOnNewSession);

  // Since sometimes a new session will happen right at startup, before other
  // services can try to listen, cache whether there has been a new low-usage
  // session since startup so that notifications can be sent immediately on
  // observation.
  bool session_since_startup_ = false;

  // The list of callbacks for when a low-usage session starts.
  base::RepeatingClosureList callbacks_;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_OBSERVER_H_
