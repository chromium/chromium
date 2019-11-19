// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is only used for OS_FUCHSIA, since there is no crash reporter
// for that platform.

#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"

#if !BUILDFLAG(USE_CRASH_KEY_STUBS)
#error "This file should only be compiled when using stubs."
#endif

namespace crash_reporter {

namespace internal {

void CrashKeyStringImpl::Set(base::StringPiece value) {}

void CrashKeyStringImpl::Clear() {}

bool CrashKeyStringImpl::is_set() const {
  return false;
}

}  // namespace internal

void InitializeCrashKeys() {}

std::string GetCrashKeyValue(const std::string& key_name) {
  return std::string();
}

void InitializeCrashKeysForTesting() {}

void ResetCrashKeysForTesting() {}

}  // namespace crash_reporter
