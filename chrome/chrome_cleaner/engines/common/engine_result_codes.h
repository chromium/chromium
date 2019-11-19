// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_RESULT_CODES_H_
#define CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_RESULT_CODES_H_

#include <stdint.h>

namespace chrome_cleaner {

// Represents result codes from the sandboxed engine.
enum EngineResultCode : uint32_t {
  kSuccess = 0x0,
  kCancelled = 0x1,
  kInvalidParameter = 0x2,
  kWrongState = 0x3,
  kAlreadyShutDown = 0x4,
  kNotEnoughSpace = 0x5,
  kSandboxUnavailable = 0x6,
  kCleaningFailed = 0x7,
  kCleanupInitializationFailed = 0x8,

  kEngineInternal = 0x100,
  kModuleLoadFailure = 0x101,
  kModuleException = 0x102,
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_RESULT_CODES_H_
