// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_NON_BLOCKING_NAVIGATION_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_NON_BLOCKING_NAVIGATION_TRACKER_H_

#include "chrome/test/chromedriver/chrome/page_load_strategy.h"

class Timeout;
class Status;

class NonBlockingNavigationTracker : public PageLoadStrategy {
 public:
  NonBlockingNavigationTracker() {}

  ~NonBlockingNavigationTracker() override;

  // Overridden from PageLoadStrategy:
  Status IsPendingNavigation(const Timeout* timeout, bool* is_pending) override;
  void set_timed_out(bool timed_out) override;
  void SetFrame(const std::string& new_frame_id) override;
  bool IsNonBlocking() const override;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_NON_BLOCKING_NAVIGATION_TRACKER_H_
