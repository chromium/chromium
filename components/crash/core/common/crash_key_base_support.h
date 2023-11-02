// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_BASE_SUPPORT_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_BASE_SUPPORT_H_

namespace crash_reporter {

// This initializes //base to support crash keys via the interface in
// base/debug/crash_logging.h.
void InitializeCrashKeyBaseSupport();

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_BASE_SUPPORT_H_
