// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_SCOPED_HANDLE_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_SCOPED_HANDLE_H_

#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

namespace credential_provider {
namespace extension {

class ScHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  ScHandleTraits() = delete;
  ScHandleTraits(const ScHandleTraits&) = delete;
  ScHandleTraits& operator=(const ScHandleTraits&) = delete;

  static bool CloseHandle(SC_HANDLE handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  static bool IsHandleValid(SC_HANDLE handle) { return handle != nullptr; }

  static SC_HANDLE NullHandle() { return nullptr; }
};

typedef base::win::GenericScopedHandle<ScHandleTraits,
                                       base::win::DummyVerifierTraits>
    ScopedScHandle;

class TimerTraits {
 public:
  using Handle = HANDLE;

  TimerTraits() = delete;
  TimerTraits(const TimerTraits&) = delete;
  TimerTraits& operator=(const TimerTraits&) = delete;

  static bool CloseHandle(HANDLE handle) {
    return ::DeleteTimerQueue(handle) != FALSE;
  }

  static bool IsHandleValid(HANDLE handle) { return handle != nullptr; }

  static HANDLE NullHandle() { return nullptr; }
};

typedef base::win::GenericScopedHandle<TimerTraits,
                                       base::win::DummyVerifierTraits>
    ScopedTimerHandle;

}  // namespace extension
}  // namespace credential_provider
#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_SCOPED_HANDLE_H_
