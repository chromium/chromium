// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_SCOPED_TOKEN_PRIVILEGE_H_
#define CHROME_INSTALLER_UTIL_SCOPED_TOKEN_PRIVILEGE_H_

#include <windows.h>

#include "base/win/scoped_handle.h"

namespace installer {

// This class is available for Windows only and will enable the privilege
// defined by |privilege_name| on the current process' token. The privilege will
// be disabled upon the ScopedTokenPrivilege's destruction (unless it was
// already enabled when the ScopedTokenPrivilege object was constructed).
// Some privileges might require admin rights to be enabled (check is_enabled()
// to know whether |privilege_name| was successfully enabled).
class ScopedTokenPrivilege {
 public:
  ScopedTokenPrivilege() = delete;

  explicit ScopedTokenPrivilege(const wchar_t* privilege_name);

  ScopedTokenPrivilege(const ScopedTokenPrivilege&) = delete;
  ScopedTokenPrivilege& operator=(const ScopedTokenPrivilege&) = delete;

  ~ScopedTokenPrivilege();

  // Always returns true unless the privilege could not be enabled.
  bool is_enabled() const { return is_enabled_; }

 private:
  // Always true unless the privilege could not be enabled.
  bool is_enabled_;

  // A scoped handle to the current process' token. This will be closed
  // preemptively should enabling the privilege fail in the constructor.
  base::win::ScopedHandle token_;

  // The previous state of the privilege this object is responsible for. As set
  // by AdjustTokenPrivileges() upon construction.
  TOKEN_PRIVILEGES previous_privileges_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_SCOPED_TOKEN_PRIVILEGE_H_
