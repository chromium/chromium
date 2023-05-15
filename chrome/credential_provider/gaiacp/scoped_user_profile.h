// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_SCOPED_USER_PROFILE_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_SCOPED_USER_PROFILE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"

namespace credential_provider {

class FakeScopedUserProfileFactory;

// Class that ensure an account's user profile and its home directory are
// properly created.
class ScopedUserProfile {
 public:
  static std::unique_ptr<ScopedUserProfile> Create(
      const std::wstring& sid,
      const std::wstring& domain,
      const std::wstring& username,
      const std::wstring& password);

  virtual ~ScopedUserProfile();

  // Saves Gaia information to the account's KHCU registry hive.
  virtual HRESULT SaveAccountInfo(const base::Value::Dict& properties);

 protected:
  // This constructor is used by the derived fake class to bypass the
  // initialization code in the public constructor that will fail because the
  // tests are not running elevated.
  ScopedUserProfile();

  HRESULT ExtractAssociationInformation(const base::Value::Dict& properties,
                                        std::wstring* sid,
                                        std::wstring* id,
                                        std::wstring* email,
                                        std::wstring* token_handle);

  HRESULT RegisterAssociation(const std::wstring& sid,
                              const std::wstring& id,
                              const std::wstring& email,
                              const std::wstring& token_handle,
                              const std::wstring& last_token_valid_millis);

 private:
  friend class FakeScopedUserProfileFactory;

  bool IsValid();

  // Waits for the specified user's profile to be created.  The credprov
  // does not have backup/restore privileges, so cannot call LoadUserProfile()
  // directly.  winlogon.exe does have the privileges, but somehow they do
  // not get transferred.
  //
  // Another option is CreateProcessWithLogonW(...LOGON_WITH_PROFILE...),
  // however a process running as SYSTEM is not permitted to call this.  The
  // workaround used in this class is to simply wait for the user's profile
  // directory and registry hive to be loaded by the system itself.  This
  // code must be run after the credprov tells winlogon to log the user in.
  ScopedUserProfile(const std::wstring& sid,
                    const std::wstring& domain,
                    const std::wstring& username,
                    const std::wstring& password);

  bool WaitForProfileCreation(const std::wstring& sid);

  base::win::ScopedHandle token_;

  // Gets storage of the function pointer used to create instances of this
  // class for tests.
  using CreatorFunc = decltype(Create);
  using CreatorCallback = base::RepeatingCallback<CreatorFunc>;
  static CreatorCallback* GetCreatorFunctionStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_SCOPED_USER_PROFILE_H_
