// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/os_gaia_user_manager.h"

#include <windows.h>

#include <lm.h>  // Needed for PNTSTATUS
#include <ntstatus.h>
#include <winternl.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For POLICY_ALL_ACCESS types

#include <atlcomcli.h>  // For CComBSTR
#include <sddl.h>       // For ConvertSidToStringSid()

#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"

namespace credential_provider {
namespace {

// Gets current sid for gaia user, it can be different from the stored one.
HRESULT GetCurrentGaiaSid(const int& size, wchar_t* current_sid) {
  LOGFN(VERBOSE);
  DCHECK(current_sid);

  HRESULT hr = S_OK;

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  wchar_t gaia_username[kWindowsUsernameBufferLength] = {0};
  hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                   std::size(gaia_username));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "RetrievePrivateData for gaia username hr=" << putHR(hr);
    return hr;
  }

  std::wstring sid;
  hr = OSUserManager::Get()->GetUserSID(OSUserManager::GetLocalDomain().c_str(),
                                        gaia_username, &sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "OSUserManager::Get()->GetUserSID hr=" << putHR(hr);
    return hr;
  }

  errno_t err = wcscpy_s(current_sid, size, sid.c_str());
  return err == 0 ? S_OK : E_FAIL;
}

// Compares gaia user sid saved during installation against current one.
// The "gaia" user has a password randomly generated as part of GCPW
// installation.
// This password is stored in Windows LSA store. However if the device where
// GCPW is running is cloned to another machine via sysprep, and then copied to
// many other machines, each machine would have the same password for the gaia
// user.
// To avoid this, since sysprep resets the machine/users sid,
// we save the gaia user sid to LSA during installation, and check if the stored
// sid is different each time the Windows login screen is shown.
// If they are different, (or the stored sid does not exist, which happens for
// previous versions) would mean the OS was just copied to a new machine after
// sysprep (or that we are upgrading version), so we change the gaia user
// password and update the stored sid to the current one.
// After that, each time we check, the sids should be the same and no password
// change will be needed.
HRESULT IsGaiaUserSidDifferent(bool* is_sid_different) {
  LOGFN(VERBOSE);
  DCHECK(is_sid_different);

  HRESULT hr = S_OK;

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  wchar_t stored_sid[kWindowsSidBufferLength] = {0};
  hr = policy->RetrievePrivateData(kLsaKeyGaiaSid, stored_sid,
                                   std::size(stored_sid));

  if (hr == HRESULT_FROM_NT(STATUS_OBJECT_NAME_NOT_FOUND)) {
    LOGFN(INFO) << "Stored SID for gaia user not found. hr=" << putHR(hr);
    *is_sid_different = true;
    return S_OK;
  }
  if (FAILED(hr)) {
    LOGFN(ERROR) << "RetrievePrivateData for stored gaia sid. hr=" << putHR(hr);
    return hr;
  }

  wchar_t current_sid[kWindowsSidBufferLength];
  hr = GetCurrentGaiaSid(std::size(current_sid), current_sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetCurrentGaiaSid hr=" << putHR(hr);
    return hr;
  }

  if (wcscmp(stored_sid, current_sid) != 0) {
    *is_sid_different = true;
  }
  return hr;
}

// Stores current gaia user sid in LSA.
HRESULT StoreCurrentGaiaSid() {
  LOGFN(VERBOSE);

  HRESULT hr = S_OK;
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  // Store current sid in LSA so the next time they will be the same.
  wchar_t sid_string[kWindowsSidBufferLength];
  hr = GetCurrentGaiaSid(std::size(sid_string), sid_string);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetCurrentGaiaSid hr=" << putHR(hr);
    return hr;
  }

  hr = policy->StorePrivateData(kLsaKeyGaiaSid, sid_string);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "StorePrivateData for gaia user sid hr=" << putHR(hr);
    return hr;
  }

  return hr;
}

}  // namespace

OSGaiaUserManager::~OSGaiaUserManager() {}

// static
OSGaiaUserManager** OSGaiaUserManager::GetInstanceStorage() {
  static OSGaiaUserManager* instance = new OSGaiaUserManager();
  return &instance;
}

// static
OSGaiaUserManager* OSGaiaUserManager::Get() {
  return *GetInstanceStorage();
}

HRESULT OSGaiaUserManager::CreateGaiaUser(PSID* sid) {
  LOGFN(VERBOSE);
  DCHECK(sid);
  HRESULT hr = S_OK;
  OSUserManager* manager = OSUserManager::Get();

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  // Generate a random password for the new gaia account.
  wchar_t password[kWindowsPasswordBufferLength] = {0};
  hr = manager->GenerateRandomPassword(password, std::size(password));
  if (FAILED(hr)) {
    SecurelyClearBuffer(password, std::size(password));
    LOGFN(ERROR) << "GenerateRandomPassword hr=" << putHR(hr);
    return hr;
  }

  CComBSTR sid_bstr;
  CComBSTR gaia_username_bstr;
  // Keep trying to create the special Gaia account used to run the UI until
  // an unused username can be found or kMaxUsernameAttempts has been reached.
  hr = manager->CreateNewUser(
      kDefaultGaiaAccountName, password,
      GetStringResource(IDS_GAIA_ACCOUNT_FULLNAME_BASE).c_str(),
      GetStringResource(IDS_GAIA_ACCOUNT_COMMENT_BASE).c_str(),
      /*add_to_users_group=*/false, kMaxUsernameAttempts, &gaia_username_bstr,
      &sid_bstr);
  if (FAILED(hr)) {
    SecurelyClearBuffer(password, std::size(password));
    LOGFN(ERROR) << "CreateNewUser hr=" << putHR(hr);
    return hr;
  }

  hr = policy->StorePrivateData(kLsaKeyGaiaPassword, password);
  SecurelyClearBuffer(password, std::size(password));
  if (FAILED(hr)) {
    LOGFN(ERROR) << "StoreGaiaPassword hr=" << putHR(hr);
    return hr;
  }

  // Save the gaia username in a machine secret area.
  hr = policy->StorePrivateData(kLsaKeyGaiaUsername, gaia_username_bstr);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "StorePrivateData for gaia user name hr=" << putHR(hr);
    return hr;
  }

  // Save the sid in a machine secret area.
  hr = StoreCurrentGaiaSid();
  if (FAILED(hr)) {
    LOGFN(ERROR) << "StoreCurrentGaiaSid hr=" << putHR(hr);
    return hr;
  }

  wchar_t sid_string[kWindowsSidBufferLength];
  hr = GetCurrentGaiaSid(std::size(sid_string), sid_string);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetCurrentGaiaSid hr=" << putHR(hr);
    return hr;
  }

  if (!::ConvertStringSidToSid(sid_string, sid)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ConvertStringSidToSid sid=" << sid_string
                 << " hr=" << putHR(hr);
    return hr;
  }
  return hr;
}

HRESULT OSGaiaUserManager::ChangeGaiaUserPasswordIfNeeded() {
  LOGFN(VERBOSE);
  HRESULT hr;

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  if (!policy) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  bool is_sid_different = false;
  OSUserManager* manager = OSUserManager::Get();

  // gaia user must already exist.
  hr = IsGaiaUserSidDifferent(&is_sid_different);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "IsGaiaUserSidDifferent hr=" << putHR(hr);
    return hr;
  }

  if (is_sid_different) {
    // Change gaia user password and update sid to the current one.
    wchar_t gaia_username[kWindowsUsernameBufferLength] = {0};
    hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                     std::size(gaia_username));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "RetrievePrivateData for gaia username hr=" << putHR(hr);
      return hr;
    }

    wchar_t new_password[kWindowsPasswordBufferLength] = {0};
    hr = manager->GenerateRandomPassword(new_password, std::size(new_password));
    if (FAILED(hr)) {
      SecurelyClearBuffer(new_password, std::size(new_password));
      LOGFN(ERROR) << "GenerateRandomPassword hr=" << putHR(hr);
      return hr;
    }

    wchar_t current_password[kWindowsPasswordBufferLength] = {0};
    hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, current_password,
                                     std::size(current_password));
    if (FAILED(hr)) {
      SecurelyClearBuffer(new_password, std::size(current_password));
      LOGFN(ERROR) << "RetrievePrivateData hr=" << putHR(hr);
      return hr;
    }

    hr = manager->ChangeUserPassword(OSUserManager::GetLocalDomain().c_str(),
                                     gaia_username, current_password,
                                     new_password);
    SecurelyClearBuffer(current_password, std::size(current_password));
    if (FAILED(hr)) {
      SecurelyClearBuffer(new_password, std::size(new_password));
      LOGFN(ERROR) << "ChangeUserPassword hr=" << putHR(hr);
      return hr;
    }

    hr = policy->StorePrivateData(kLsaKeyGaiaPassword, new_password);
    SecurelyClearBuffer(new_password, std::size(new_password));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "StoreGaiaPassword hr=" << putHR(hr);
      return hr;
    }

    LOGFN(INFO) << "Password succesfully reset for gaia user.";

    hr = StoreCurrentGaiaSid();
    if (FAILED(hr)) {
      LOGFN(ERROR) << "StoreCurrentGaiaSid hr=" << putHR(hr);
      return hr;
    }
    LOGFN(INFO) << "Current SID stored for gaia user.";
    return hr;
  }
  LOGFN(VERBOSE) << "Gaia user password not changed.";

  return hr;
}

void OSGaiaUserManager::SetFakesForTesting(FakesForTesting* fakes) {
  DCHECK(fakes);

  if (fakes->os_user_manager_for_testing) {
    OSUserManager::SetInstanceForTesting(fakes->os_user_manager_for_testing);
  }
  if (fakes->scoped_lsa_policy_creator) {
    ScopedLsaPolicy::SetCreatorForTesting(fakes->scoped_lsa_policy_creator);
  }
}

}  // namespace credential_provider
