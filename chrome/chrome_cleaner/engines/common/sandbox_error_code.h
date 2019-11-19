// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_COMMON_SANDBOX_ERROR_CODE_H_
#define CHROME_CHROME_CLEANER_ENGINES_COMMON_SANDBOX_ERROR_CODE_H_

#include <stdint.h>

namespace chrome_cleaner {

// A list of custom error codes that can be returned by sandbox callbacks.
// These enums must be defined such that their values don't conflict with
// Windows error codes (so they must set bit 29 to 1).
enum SandboxErrorCode : uint32_t {
  INVALID_DW_ACCESS = 0x10000000,
  NULL_ROOT_KEY = 0x10000001,
  INVALID_SUBKEY_STRING = 0x10000002,
  NULL_ROOT_AND_RELATIVE_SUB_KEY = 0x10000003,
  NULL_OUTPUT_HANDLE = 0x10000004,
  NULL_SUB_KEY = 0x10000005,
  FILE_NAME_TOO_LONG = 0x10000006,
  NULL_FILE_NAME = 0x10000007,
  NULL_NAME = 0x10000008,
  NAME_TOO_LONG = 0x10000009,
  INVALID_KEY = 0x1000000A,
  INVALID_VALUE_NAME = 0x1000000B,
  INVALID_VALUE = 0x1000000C,
  BAD_SID = 0x1000000D,
  NULL_FIND_HANDLE = 0x1000000E,
  NULL_DATA_HANDLE = 0x1000000F,
  RELATIVE_PATH_NOT_ALLOWED = 0x10000010,
  INVALID_FILE_PATH = 0x10000011,
  INTERNAL_ERROR = 0x1FFFFFFF,
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_COMMON_SANDBOX_ERROR_CODE_H_
