// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_LOAD_STRATEGY_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_LOAD_STRATEGY_H_

#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"

class Status;
class Timeout;

class PageLoadStrategy : public DevToolsEventListener {
 public:
  enum LoadingState {
    kUnknown,
    kLoading,
    kNotLoading,
  };

  virtual Status IsPendingNavigation(const Timeout* timeout,
                                     bool* is_pending) = 0;

  virtual void set_timed_out(bool timed_out) = 0;

  virtual void SetFrame(const std::string& new_frame_id) = 0;

  virtual bool IsNonBlocking() const = 0;

  // Types of page load strategies.
  static const char kNormal[];
  static const char kNone[];
  static const char kEager[];
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_LOAD_STRATEGY_H_
