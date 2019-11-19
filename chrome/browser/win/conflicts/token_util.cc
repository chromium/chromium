// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/token_util.h"

#include <windows.h>

#include <stdint.h>

#include "base/win/scoped_handle.h"

namespace {

// Checks if the |token| is member of |group_sid|. |token| must be an
// impersonating token. Use a null handle to check for the token of the current
// thread. Returns false on error.
bool IsMemberOfGroupSID(SID* group_sid, HANDLE token) {
  BOOL is_member = FALSE;
  return ::CheckTokenMembership(token, group_sid, &is_member) && !!is_member;
}

}  // namespace

bool HasAdminRights() {
  // Get the SID for the administrators group.
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  uint8_t sid_bytes[SECURITY_MAX_SID_SIZE];
  SID* administrators_sid = reinterpret_cast<SID*>(sid_bytes);
  if (!::CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr,
                            administrators_sid, &sid_size)) {
    return false;
  }

  // Check if the current token is member of the built-in Administrators group.
  if (IsMemberOfGroupSID(administrators_sid, nullptr))
    return true;

  // In the case that UAC is enabled, it's possible that the current token is
  // filtered. So check the linked token in case it is a member of the built-in
  // Administrators group.
  HANDLE current_token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &current_token))
    return false;
  base::win::ScopedHandle scoped_current_token(current_token);

  HANDLE linked_token = nullptr;
  DWORD linked_token_size = sizeof(linked_token);
  if (!::GetTokenInformation(scoped_current_token.Get(), TokenLinkedToken,
                             &linked_token, linked_token_size,
                             &linked_token_size)) {
    return false;
  }
  base::win::ScopedHandle scoped_linked_token(linked_token);

  return IsMemberOfGroupSID(administrators_sid, scoped_linked_token.Get());
}
