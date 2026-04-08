// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_DETECT_INAPPROPRIATE_EXIT_H_
#define CHROME_COMMON_MAC_DETECT_INAPPROPRIATE_EXIT_H_

namespace chrome {

// Installs an `atexit` handler that transforms inappropriate calls to `exit`
// into `CHECK` failures, allowing them to become visible as crashes.
//
// This is intended to be an aid in troubleshooting https://crbug.com/474158974,
// https://crbug.com/485292574, https://crbug.com/498266556, and others, where
// OS library code is calling `exit(69)` directly.
//
// TODO(mark): Remove when the above bugs are understood.
void InitializeExitSixtyNineDetector();

}  // namespace chrome

#endif  // CHROME_COMMON_MAC_DETECT_INAPPROPRIATE_EXIT_H_
