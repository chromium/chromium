// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/scoped_token_privilege.h"

namespace installer {

ScopedTokenPrivilege::ScopedTokenPrivilege(const wchar_t* privilege_name)
    : is_enabled_(false) {
  HANDLE temp_handle;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &temp_handle)) {
    return;
  }
  token_.Set(temp_handle);

  LUID privilege_luid;
  if (!::LookupPrivilegeValue(nullptr, privilege_name, &privilege_luid)) {
    token_.Close();
    return;
  }

  // Adjust the token's privileges to enable |privilege_name|. If this privilege
  // was already enabled, |previous_privileges_|.PrivilegeCount will be set to 0
  // and we then know not to disable this privilege upon destruction.
  TOKEN_PRIVILEGES tp;
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Luid = privilege_luid;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  DWORD return_length;
  if (!::AdjustTokenPrivileges(token_.Get(), FALSE, &tp,
                               sizeof(TOKEN_PRIVILEGES), &previous_privileges_,
                               &return_length)) {
    token_.Close();
    return;
  }

  is_enabled_ = true;
}

ScopedTokenPrivilege::~ScopedTokenPrivilege() {
  if (is_enabled_ && previous_privileges_.PrivilegeCount != 0) {
    ::AdjustTokenPrivileges(token_.Get(), FALSE, &previous_privileges_,
                            sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
  }
}

}  // namespace installer
