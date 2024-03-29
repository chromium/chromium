// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_OBSERVER_H_
#define CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_OBSERVER_H_

#include "chrome/browser/user_education/recent_session_tracker.h"

class RecentSessionObserver {
 public:
  RecentSessionObserver() = default;
  RecentSessionObserver(const RecentSessionObserver&) = delete;
  void operator=(const RecentSessionObserver&) = delete;
  virtual ~RecentSessionObserver() = default;

  // Initialization once the tracker is available.
  virtual void Init(RecentSessionTracker& tracker) = 0;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_RECENT_SESSION_OBSERVER_H_
