// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_SCOPED_SERVICE_HANDLE_H_
#define CHROME_CHROME_CLEANER_OS_SCOPED_SERVICE_HANDLE_H_

#include <windows.h>

#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace chrome_cleaner {

class ScHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  ScHandleTraits() = delete;
  ScHandleTraits(const ScHandleTraits&) = delete;
  ScHandleTraits& operator=(const ScHandleTraits&) = delete;

  // Closes the handle.
  static bool CloseHandle(SC_HANDLE handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  // Returns true if the handle value is valid.
  static bool IsHandleValid(SC_HANDLE handle) { return handle != nullptr; }

  // Returns null handle value.
  static SC_HANDLE NullHandle() { return nullptr; }
};

typedef base::win::GenericScopedHandle<ScHandleTraits,
                                       base::win::DummyVerifierTraits>
    ScopedScHandle;

class ScopedServiceHandle {
 public:
  ScopedServiceHandle();
  ~ScopedServiceHandle();

  bool IsValid() const {
    return service_manager_.IsValid() && service_.IsValid();
  }

  SC_HANDLE get() const { return service_.Get(); }

  bool OpenService(const wchar_t* service_name,
                   DWORD service_manager_desired_access,
                   DWORD service_desired_access) {
    DCHECK(service_name);
    DCHECK(!service_manager_.IsValid());
    DCHECK(!service_.IsValid());

    // Get a handle to the service manager.
    service_manager_.Set(
        ::OpenSCManager(nullptr, nullptr, service_manager_desired_access));
    if (!service_manager_.IsValid()) {
      PLOG(ERROR) << "Cannot open service manager.";
      return false;
    }

    // Get a handle to the service.
    service_.Set(::OpenService(service_manager_.Get(), service_name,
                               service_desired_access));
    if (!service_.IsValid()) {
      // The service doesn't exists, the returned service handle is null.
      DWORD last_error = GetLastError();
      if (last_error == ERROR_SERVICE_DOES_NOT_EXIST) {
        LOG(INFO) << "Service '" << service_name << "' doesn't exists.";
        return true;
      }

      // Unable to open the service.
      PLOG(ERROR) << "Cannot open service '" << service_name << "'.";
      return false;
    }
    // Return the service handle.
    return true;
  }

 protected:
  ScopedScHandle service_manager_;
  ScopedScHandle service_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_SCOPED_SERVICE_HANDLE_H_
