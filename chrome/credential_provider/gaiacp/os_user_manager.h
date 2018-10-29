// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_USER_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_USER_MANAGER_H_

#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

typedef wchar_t* BSTR;

namespace credential_provider {

// Manages OS users on the system.
class OSUserManager {
 public:
  // Minimum length for password buffer when calling GenerateRandomPassword().
  static const int kMinPasswordLength = 24;

  static OSUserManager* Get();

  virtual ~OSUserManager();

  // Generates a cryptographically secure random password.
  virtual HRESULT GenerateRandomPassword(wchar_t* password, int length);

  // Creates a new OS user on the system with the given credentials.  If
  // |add_to_users_group| is true, the Os user is added to the machine's
  // "Users" group which allows interactive logon.  The OS user's SID is
  // returned in |sid|.
  virtual HRESULT AddUser(const wchar_t* username,
                          const wchar_t* password,
                          const wchar_t* fullname,
                          const wchar_t* comment,
                          bool add_to_users_group,
                          BSTR* sid,
                          DWORD* error);

  // Changes the password of the given OS user.
  virtual HRESULT SetUserPassword(const wchar_t* username,
                                  const wchar_t* password,
                                  DWORD* error);

  // Creates a logon token for the given user.  If |interactive| is true the
  // token is of type interactive otherwise it is of type batch.
  virtual HRESULT CreateLogonToken(const wchar_t* username,
                                   const wchar_t* password,
                                   bool interactive,
                                   base::win::ScopedHandle* token);

  // Gets the SID of the given OS user.  The caller owns the pointer |sid| and
  // should free it with a call to LocalFree().
  virtual HRESULT GetUserSID(const wchar_t* username, PSID* sid);

  // Finds a user created from a gaia account by its SID.  Returns S_OK if a
  // user with the given SID exists, HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)
  // if not, or an arbitrary error otherwise.  If |username| is non-null and
  // |length| is greater than zero, the username associated with the SID is
  // returned.
  virtual HRESULT FindUserBySID(const wchar_t* sid,
                                wchar_t* username,
                                DWORD length);

  // Removes the user from the machine.
  virtual HRESULT RemoveUser(const wchar_t* username, const wchar_t* password);

  // This method is called either from FakeOSUserManager or from dllmain.cc when
  // setting fakes from one module to another.
  static void SetInstanceForTesting(OSUserManager* factory);

 protected:
  OSUserManager() {}

  // Returns the storage used for the instance pointer.
  static OSUserManager** GetInstanceStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_USER_MANAGER_H_
