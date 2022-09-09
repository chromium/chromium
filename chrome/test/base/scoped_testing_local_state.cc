// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_testing_local_state.h"

#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

ScopedTestingLocalState::ScopedTestingLocalState(
    TestingBrowserProcess* browser_process)
    : browser_process_(browser_process) {
  RegisterLocalState(local_state_.registry());
  EXPECT_FALSE(browser_process->local_state());
  browser_process->SetLocalState(&local_state_);
}

ScopedTestingLocalState::~ScopedTestingLocalState() {
  EXPECT_EQ(&local_state_, browser_process_->local_state());
  browser_process_->SetLocalState(nullptr);
}
