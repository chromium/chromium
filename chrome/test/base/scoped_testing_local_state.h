// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SCOPED_TESTING_LOCAL_STATE_H_
#define CHROME_TEST_BASE_SCOPED_TESTING_LOCAL_STATE_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/testing_pref_service.h"

class TestingBrowserProcess;

// DEPRECATED: This class no longer have any effect. TestingBrowserProcess has
// its own testing local state, so unit tests can just use it without setting up
// one.
// TODO(crbug.com/422039036): Remove this class and existing usage.
//
// Helper class to temporarily set up a |local_state| in the global
// TestingBrowserProcess (for most unit tests it's NULL).
class ScopedTestingLocalState {
 public:
  explicit ScopedTestingLocalState(TestingBrowserProcess* browser_process);
  ScopedTestingLocalState(const ScopedTestingLocalState&) = delete;
  ScopedTestingLocalState& operator=(const ScopedTestingLocalState&) = delete;
  ~ScopedTestingLocalState();

  TestingPrefServiceSimple* Get();

 private:
  raw_ptr<TestingBrowserProcess> browser_process_;
};

#endif  // CHROME_TEST_BASE_SCOPED_TESTING_LOCAL_STATE_H_
