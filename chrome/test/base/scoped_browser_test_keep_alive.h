// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SCOPED_BROWSER_TEST_KEEP_ALIVE_H_
#define CHROME_TEST_BASE_SCOPED_BROWSER_TEST_KEEP_ALIVE_H_

#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

class Profile;

// Keeps the process and profile alive across a last-browser close/reopen.
// Release while a browser or another process keep-alive still exists.
class ScopedBrowserTestKeepAlive {
 public:
  explicit ScopedBrowserTestKeepAlive(Profile& profile);
  ScopedBrowserTestKeepAlive(const ScopedBrowserTestKeepAlive&) = delete;
  ScopedBrowserTestKeepAlive& operator=(const ScopedBrowserTestKeepAlive&) =
      delete;
  ~ScopedBrowserTestKeepAlive();

 private:
  ScopedKeepAlive keep_alive_;
  ScopedProfileKeepAlive profile_keep_alive_;
};

#endif  // CHROME_TEST_BASE_SCOPED_BROWSER_TEST_KEEP_ALIVE_H_
