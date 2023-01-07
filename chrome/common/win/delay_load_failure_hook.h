// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WIN_DELAY_LOAD_FAILURE_HOOK_H_
#define CHROME_COMMON_WIN_DELAY_LOAD_FAILURE_HOOK_H_

namespace chrome {

// This should be called early in process startup (before any delay load
// failures) to disable the delay load hooks for the current module.
void DisableDelayLoadFailureHooksForCurrentModule();

}  // namespace chrome

#endif  // CHROME_COMMON_WIN_DELAY_LOAD_FAILURE_HOOK_H_
