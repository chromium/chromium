// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/non_blocking_navigation_tracker.h"

NonBlockingNavigationTracker::~NonBlockingNavigationTracker() {}

Status NonBlockingNavigationTracker::IsPendingNavigation(
    const Timeout* timeout,
    bool* is_pending) {
  *is_pending = false;
  return Status(kOk);
}

void NonBlockingNavigationTracker::set_timed_out(bool timed_out) {}

void NonBlockingNavigationTracker::SetFrame(const std::string& new_frame_id) {}

bool NonBlockingNavigationTracker::IsNonBlocking() const {
  return true;
}
