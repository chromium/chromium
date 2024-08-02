// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_TEST_TEST_UTILS_H_
#define CHROME_ENTERPRISE_COMPANION_TEST_TEST_UTILS_H_

#include "base/process/process.h"

namespace enterprise_companion {

// Waits for a multi-process test child to exit without blocking the main
// sequence, returning its exit code. Expects the process to exit within the
// test action timeout.
int WaitForProcess(base::Process&);

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_TEST_TEST_UTILS_H_
