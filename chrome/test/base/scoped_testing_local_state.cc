// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_testing_local_state.h"

#include "chrome/test/base/testing_browser_process.h"

ScopedTestingLocalState::ScopedTestingLocalState(
    TestingBrowserProcess* browser_process)
    : browser_process_(browser_process) {}

ScopedTestingLocalState::~ScopedTestingLocalState() = default;

TestingPrefServiceSimple* ScopedTestingLocalState::Get() {
  return browser_process_->GetTestingLocalState();
}
