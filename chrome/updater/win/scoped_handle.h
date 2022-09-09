// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SCOPED_HANDLE_H_
#define CHROME_UPDATER_WIN_SCOPED_HANDLE_H_

#include <windows.h>

#include "base/scoped_generic.h"

namespace updater {

namespace internal {

struct ScopedFileHANDLECloseTraits {
  static HANDLE InvalidValue() { return INVALID_HANDLE_VALUE; }
  static void Free(HANDLE handle) { ::CloseHandle(handle); }
};

struct ScopedKernelHANDLECloseTraits {
  static HANDLE InvalidValue() { return nullptr; }
  static void Free(HANDLE handle) { ::CloseHandle(handle); }
};

}  // namespace internal

using ScopedFileHANDLE =
    base::ScopedGeneric<HANDLE, internal::ScopedFileHANDLECloseTraits>;
using ScopedKernelHANDLE =
    base::ScopedGeneric<HANDLE, internal::ScopedKernelHANDLECloseTraits>;

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SCOPED_HANDLE_H_
