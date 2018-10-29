// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/os_user_manager.h"

#include <windows.h>
#include <lm.h>
#include <Shellapi.h>  // For <Shlobj.h>
#include <Shlobj.h>    // For SHFileOperation()
#include <sddl.h>      // For ConvertSidToStringSid()
#include <userenv.h>   // For GetUserProfileDirectory()
#include <wincrypt.h>  // For CryptXXX()

#include <atlconv.h>

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>

#include <iomanip>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

// static
OSUserManager** OSUserManager::GetInstanceStorage() {
  static OSUserManager* instance = new OSUserManager();
  return &instance;
}

// static
OSUserManager* OSUserManager::Get() {
  return *GetInstanceStorage();
}

// static
void OSUserManager::SetInstanceForTesting(OSUserManager* instance) {
  *GetInstanceStorage() = instance;
}

OSUserManager::~OSUserManager() {}

#define IS_PASSWORD_STRONG_ENOUGH()    \
  (cur_length > kMinPasswordLength) && \
      (has_upper + has_lower + has_digit + has_punct > 3)

HRESULT OSUserManager::GenerateRandomPassword(wchar_t* password, int length) {
  HRESULT hr;
  HCRYPTPROV prov;

  // TODO(rogerta): read password policy GPOs to see what the password policy
  // is for this machine in order to create one that adheres correctly.  For
  // now will generate a random password that fits typical strong password
  // policies on windows.
  const wchar_t kValidPasswordChars[] =
      L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      L"abcdefghijklmnopqrstuvwxyz"
      L"`1234567890-="
      L"~!@#$%^&*()_+"
      L"[]\\;',./"
      L"{}|:\"<>?";

  if (length < kMinPasswordLength)
    return E_INVALIDARG;

  if (!::CryptAcquireContext(&prov, nullptr, nullptr, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CryptAcquireContext hr=" << putHR(hr);
    return hr;
  }

  int cur_length;
  int has_upper;
  int has_lower;
  int has_digit;
  int has_punct;
  do {
    cur_length = 0;
    has_upper = 0;
    has_lower = 0;
    has_digit = 0;
    has_punct = 0;

    wchar_t* p = password;
    int remaining_length = length;

    while (remaining_length > 1) {
      BYTE r;
      if (!::CryptGenRandom(prov, sizeof(r), &r)) {
        hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "CryptGenRandom hr=" << putHR(hr);
        ::CryptReleaseContext(prov, 0);
        return hr;
      }

      wchar_t c =
          kValidPasswordChars[r % (base::size(kValidPasswordChars) - 1)];
      *p++ = c;
      ++cur_length;
      --remaining_length;

      // Check if we have all the requirements for a strong password.
      if (isupper(c))
        has_upper = 1;
      if (islower(c))
        has_lower = 1;
      if (isdigit(c))
        has_digit = 1;
      if (ispunct(c))
        has_punct = 1;

      if (IS_PASSWORD_STRONG_ENOUGH())
        break;
    }

    // Make sure password is terminated.
    *p = 0;

    // Because this is a random function, there is a chance that the password
    // might not be strong enough by the time the code reaches this point.  This
    // could happen if two categories of characters were never included.
    //
    // This highest probability of this happening would be if the caller
    // specified the shortest password allowed and the missing characters came
    // from the smallest two sets (say digits and lower case letters).
    //
    // This probability is equal to 1 minus the probability that all characters
    // are upper case or punctuation:
    //
    //     P(!strong) = 1 - P(all chars either upper or punctuation)
    //
    // There are valid 96 characters in all, of which 56 are not digits or
    // lower case.  The probability that any single character is upper case or
    // punctuation is 56/96.  If the minimum length is used, then:
    //
    //     P(all chars either upper or punctuation) = (56/96)^23 = 4.1e-6
    //
    //     P(!strong) = 1 - 1.8e-4 = 0.999996
    //
    // or about 4 in every million.  The exponent is 23 and not 24 because the
    // minimum password length includes the null terminiator.
    //
    // This means:
    //   - ~4 in every million will run this loop at least twice
    //   - ~4 in every 250 billion will run this loop at least thrice
    //   - ~4 in every 6.25e13 will run this loop at least four times
  } while (!IS_PASSWORD_STRONG_ENOUGH());

  ::CryptReleaseContext(prov, 0);

  return S_OK;
}

HRESULT OSUserManager::AddUser(const wchar_t* username,
                               const wchar_t* password,
                               const wchar_t* fullname,
                               const wchar_t* comment,
                               bool add_to_users_group,
                               BSTR* sid,
                               DWORD* error) {
  DCHECK(sid);

  bool user_found = false;
  USER_INFO_1 info;
  memset(&info, 0, sizeof(info));
  info.usri1_comment = _wcsdup(comment);
  info.usri1_flags =
      UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD | UF_NORMAL_ACCOUNT;
  info.usri1_name = const_cast<wchar_t*>(username);
  info.usri1_password = const_cast<wchar_t*>(password);
  info.usri1_priv = USER_PRIV_USER;

  NET_API_STATUS nsts =
      ::NetUserAdd(nullptr, 1, reinterpret_cast<LPBYTE>(&info), error);
  free(info.usri1_comment);

  // Set the user's full name.
  if (nsts == NERR_Success) {
    USER_INFO_1011 info1011;
    memset(&info1011, 0, sizeof(info1011));
    info1011.usri1011_full_name = const_cast<wchar_t*>(fullname);
    nsts = ::NetUserSetInfo(nullptr, info.usri1_name, 1011,
                            reinterpret_cast<LPBYTE>(&info1011), error);
    if (nsts != NERR_Success) {
      LOGFN(ERROR) << "NetUserSetInfo nsts=" << nsts;
    }
  } else if (nsts == NERR_UserExists) {
    // TODO: If adding the special "gaia" account might want to check that
    // account permissions are not too permissive.
    LOGFN(INFO) << "Using existing gaia user";
    user_found = true;
  } else {
    LOGFN(ERROR) << "NetUserAdd nsts=" << nsts;
    return HRESULT_FROM_WIN32(nsts);
  }

  // Get the new user's SID and return it to caller.
  LPBYTE buffer = nullptr;
  nsts = ::NetUserGetInfo(nullptr, info.usri1_name, 4, &buffer);
  if (nsts == NERR_Success) {
    const USER_INFO_4* user_info = reinterpret_cast<const USER_INFO_4*>(buffer);
    wchar_t* sidstr = nullptr;
    if (::ConvertSidToStringSid(user_info->usri4_user_sid, &sidstr)) {
      *sid = SysAllocString(T2COLE(sidstr));
      LOGFN(INFO) << "sid=" << sidstr;
      ::LocalFree(sidstr);
    } else {
      LOGFN(ERROR) << "Could not convert SID to string";
      *sid = nullptr;
      nsts = NERR_ProgNeedsExtraMem;
    }

    if (nsts == NERR_Success && add_to_users_group) {
      // Add to the "Users" group so that it appears on login screen.
      LOCALGROUP_MEMBERS_INFO_0 member_info;
      memset(&member_info, 0, sizeof(member_info));
      member_info.lgrmi0_sid = user_info->usri4_user_sid;
      nsts = ::NetLocalGroupAddMembers(
          nullptr, L"Users", 0, reinterpret_cast<LPBYTE>(&member_info), 1);
      if (nsts != NERR_Success && nsts != ERROR_MEMBER_IN_ALIAS) {
        LOGFN(ERROR) << "NetLocalGroupAddMembers nsts=" << nsts;
      } else {
        nsts = NERR_Success;
      }
    }

    ::NetApiBufferFree(buffer);
  }

  return (nsts == NERR_Success && user_found)
             ? HRESULT_FROM_WIN32(NERR_UserExists)
             : (nsts == NERR_Success ? S_OK : HRESULT_FROM_WIN32(nsts));
}

HRESULT OSUserManager::SetUserPassword(const wchar_t* username,
                                       const wchar_t* password,
                                       DWORD* error) {
  USER_INFO_1003 info1003;
  NET_API_STATUS nsts;
  memset(&info1003, 0, sizeof(info1003));
  info1003.usri1003_password = const_cast<wchar_t*>(password);
  nsts = ::NetUserSetInfo(nullptr, username, 1003,
                          reinterpret_cast<LPBYTE>(&info1003), error);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "Unable to change password for '" << username
                 << "' nsts=" << nsts;
  }

  return HRESULT_FROM_WIN32(nsts);
}

HRESULT OSUserManager::CreateLogonToken(const wchar_t* username,
                                        const wchar_t* password,
                                        bool interactive,
                                        base::win::ScopedHandle* token) {
  return ::credential_provider::CreateLogonToken(username, password,
                                                 interactive, token);
}

HRESULT OSUserManager::GetUserSID(const wchar_t* username, PSID* sid) {
  DCHECK(username);
  DCHECK(sid);

  LPBYTE buffer = nullptr;
  NET_API_STATUS nsts = ::NetUserGetInfo(nullptr, username, 4, &buffer);
  if (nsts == NERR_Success) {
    const USER_INFO_4* user_info = reinterpret_cast<const USER_INFO_4*>(buffer);
    if (::IsValidSid(user_info->usri4_user_sid)) {
      DWORD sid_length = GetLengthSid(user_info->usri4_user_sid);
      *sid = ::LocalAlloc(LMEM_FIXED, sid_length);
      ::CopySid(sid_length, *sid, user_info->usri4_user_sid);
    } else {
      LOGFN(ERROR) << "Invalid SID for username=" << username;
    }
    ::NetApiBufferFree(buffer);
  } else {
    LOGFN(ERROR) << "NetUserGetInfo nsts=" << nsts;
  }
  return HRESULT_FROM_WIN32(nsts);
}

HRESULT OSUserManager::FindUserBySID(const wchar_t* sid,
                                     wchar_t* username,
                                     DWORD length) {
  PSID psid;
  if (!::ConvertStringSidToSidW(sid, &psid)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ConvertStringSidToSidW sid=" << sid << " hr=" << putHR(hr);
    return hr;
  }

  // Maximum domain length is 256 characters including null.
  // https://support.microsoft.com/en-ca/help/909264/naming-conventions-in-active-directory-for-computers-domains-sites-and
  HRESULT hr = S_OK;
  DWORD name_length = username ? length : 0;
  wchar_t domain[256];
  DWORD domain_length = base::size(domain);
  SID_NAME_USE use;
  if (!::LookupAccountSid(nullptr, psid, username, &name_length, domain,
                          &domain_length, &use)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    if (hr != HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
      if (length == 0 && hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
        hr = S_OK;
      } else {
        LOGFN(ERROR) << "LookupAccountSid hr=" << putHR(hr);
      }
    }
  }

  LOGFN(INFO) << "username=" << username;
  ::LocalFree(psid);
  return hr;
}

HRESULT OSUserManager::RemoveUser(const wchar_t* username,
                                  const wchar_t* password) {
  DCHECK(username);
  DCHECK(password);

  // Get the user's profile directory.
  base::win::ScopedHandle token;
  wchar_t profiledir[MAX_PATH + 1];

  // Get the user's profile directory.  Try a batch logon first, and if that
  // fails then try an interactive logon.
  HRESULT hr =
      CreateLogonToken(username, password, /*interactive=*/false, &token);
  if (FAILED(hr))
    hr = CreateLogonToken(username, password, /*interactive=*/true, &token);

  if (SUCCEEDED(hr)) {
    // Get the gaia user's profile directory so that it can be deleted.
    DWORD length = base::size(profiledir) - 1;
    if (!::GetUserProfileDirectory(token.Get(), profiledir, &length)) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      if (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        LOGFN(ERROR) << "GetUserProfileDirectory hr=" << putHR(hr);
      profiledir[0] = 0;
    } else {
      // Double null terminate the profile directory for SHFileOperation().
      profiledir[length] = 0;
    }
  } else {
    LOGFN(ERROR) << "CreateLogonToken hr=" << putHR(hr);
  }

  // Remove the OS user.
  NET_API_STATUS nsts = ::NetUserDel(nullptr, username);
  if (nsts != NERR_Success)
    LOGFN(ERROR) << "NetUserDel nsts=" << nsts;

  // Force delete the user's profile directory.
  if (profiledir[0] != 0) {
    SHFILEOPSTRUCT op;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_DELETE;
    op.pFrom = profiledir;  // Double null terminated above.
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI | FOF_SILENT;

    int ret = ::SHFileOperation(&op);
    if (ret != 0) {
      LOGFN(ERROR) << "SHFileOperation ret=" << ret;
    }
  }

  return S_OK;
}

}  // namespace credential_provider
