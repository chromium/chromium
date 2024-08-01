// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/os_user_manager.h"

#include <windows.h>

#include <atlcomcli.h>  // For CComBSTR
#include <atlconv.h>
#include <lm.h>
#include <malloc.h>
#include <memory.h>
#include <sddl.h>  // For ConvertSidToStringSid()
#include <stdlib.h>
#include <userenv.h>   // For GetUserProfileDirectory()
#include <wincrypt.h>  // For CryptXXX()

#include <iomanip>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace credential_provider {

namespace {

HRESULT GetDomainControllerServerForDomain(const wchar_t* domain,
                                           LPBYTE* server) {
  DCHECK(domain);
  std::wstring local_domain = OSUserManager::GetLocalDomain();
  // If the domain is the local domain, then there is no domain controller.
  if (wcsicmp(local_domain.c_str(), domain) == 0) {
    return S_OK;
  }

  NET_API_STATUS nsts = ::NetGetDCName(nullptr, domain, server);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "NetGetDCName nsts=" << nsts;
  }
  return HRESULT_FROM_WIN32(nsts);
}

}  // namespace

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

// static
bool OSUserManager::IsDeviceDomainJoined() {
  return base::win::IsEnrolledToDomain();
}

// static
std::wstring OSUserManager::GetLocalDomain() {
  // If the domain is the current computer, then there is no domain controller.
  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD length = std::size(computer_name);
  if (!::GetComputerNameW(computer_name, &length))
    return std::wstring();

  return std::wstring(computer_name, length);
}

OSUserManager::~OSUserManager() {}

#define IS_PASSWORD_STRONG_ENOUGH()    \
  (cur_length > kMinPasswordLength) && \
      (has_upper + has_lower + has_digit + has_punct > 3)

HRESULT OSUserManager::GenerateRandomPassword(wchar_t* password, int length) {
  LOGFN(VERBOSE);
  HRESULT hr;
  HCRYPTPROV prov;

  // TODO(rogerta): read password policy GPOs to see what the password policy
  // is for this machine in order to create one that adheres correctly.  For
  // now will generate a random password that fits typical strong password
  // policies on windows.
  const unsigned char kValidPasswordChars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "`1234567890-="
      "~!@#$%^&*()_+"
      "[]\\;',./"
      "{}|:\"<>?";

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

      unsigned char c =
          kValidPasswordChars[r % (std::size(kValidPasswordChars) - 1)];
      *p++ = c;
      ++cur_length;
      --remaining_length;

      // Check if we have all the requirements for a strong password.
      if (absl::ascii_isupper(c)) {
        has_upper = 1;
      }
      if (absl::ascii_islower(c)) {
        has_lower = 1;
      }
      if (absl::ascii_isdigit(c)) {
        has_digit = 1;
      }
      if (absl::ascii_ispunct(c)) {
        has_punct = 1;
      }

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

HRESULT OSUserManager::GetUserFullname(const wchar_t* domain,
                                       const wchar_t* username,
                                       std::wstring* fullname) {
  DCHECK(fullname);
  LPBYTE domain_server_buffer = nullptr;
  HRESULT hr =
      GetDomainControllerServerForDomain(domain, &domain_server_buffer);
  if (FAILED(hr))
    return hr;

  std::unique_ptr<wchar_t, void (*)(wchar_t*)> domain_to_query(
      reinterpret_cast<wchar_t*>(domain_server_buffer), [](wchar_t* p) {
        if (p)
          ::NetApiBufferFree(p);
      });

  LPBYTE buffer = nullptr;
  NET_API_STATUS nsts =
      ::NetUserGetInfo(domain_to_query.get(), username, 11, &buffer);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "NetUserGetInfo(get full name) nsts=" << nsts;
    return HRESULT_FROM_WIN32(nsts);
  }

  USER_INFO_11* user_info = reinterpret_cast<USER_INFO_11*>(buffer);
  *fullname = user_info->usri11_full_name;
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

  std::wstring local_users_group_name;
  // If adding to the local users group, make sure we can get the localized
  // name for the group before proceeding.
  if (add_to_users_group) {
    HRESULT hr = LookupLocalizedNameForWellKnownSid(WinBuiltinUsersSid,
                                                    &local_users_group_name);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "LookupLocalizedNameForWellKnownSid hr=" << putHR(hr);
      return hr;
    }
  }

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
      LOGFN(ERROR) << "NetUserSetInfo(set full name) nsts=" << nsts;
    }
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
      LOGFN(VERBOSE) << "sid=" << sidstr;
      ::LocalFree(sidstr);
    } else {
      LOGFN(ERROR) << "Could not convert SID to string";
      *sid = nullptr;
      nsts = NERR_ProgNeedsExtraMem;
    }

    if (nsts == NERR_Success && add_to_users_group) {
      // Add to the well known local users group so that it appears on login
      // screen.
      LOCALGROUP_MEMBERS_INFO_0 member_info;
      memset(&member_info, 0, sizeof(member_info));
      member_info.lgrmi0_sid = user_info->usri4_user_sid;
      nsts =
          ::NetLocalGroupAddMembers(nullptr, local_users_group_name.c_str(), 0,
                                    reinterpret_cast<LPBYTE>(&member_info), 1);
      if (nsts != NERR_Success && nsts != ERROR_MEMBER_IN_ALIAS) {
        LOGFN(ERROR) << "NetLocalGroupAddMembers nsts=" << nsts;
      } else {
        nsts = NERR_Success;
      }
    }

    ::NetApiBufferFree(buffer);
  }

  return (nsts == NERR_Success ? S_OK : HRESULT_FROM_WIN32(nsts));
}

HRESULT OSUserManager::CreateNewUser(const wchar_t* base_username,
                                     const wchar_t* password,
                                     const wchar_t* fullname,
                                     const wchar_t* comment,
                                     bool add_to_users_group,
                                     int max_attempts,
                                     BSTR* final_username,
                                     BSTR* sid) {
  DCHECK(base_username);
  DCHECK(password);
  DCHECK(fullname);
  DCHECK(comment);
  DCHECK(final_username);
  DCHECK(sid);

  LOGFN(VERBOSE) << "Creating a new user: " << base_username;

  wchar_t new_username[kWindowsUsernameBufferLength];
  errno_t err = wcscpy_s(new_username, std::size(new_username), base_username);
  if (err != 0) {
    LOGFN(ERROR) << "wcscpy_s errno=" << err;
    return E_FAIL;
  }

  // Keep trying to create the user account until an unused username can be
  // found or |max_attempts| has been reached.
  for (int i = 0; i < max_attempts; ++i) {
    CComBSTR new_sid;
    DWORD error;
    HRESULT hr = AddUser(new_username, password, fullname, comment,
                         add_to_users_group, &new_sid, &error);
    if (hr == HRESULT_FROM_WIN32(NERR_UserExists)) {
      std::wstring next_username = base_username;
      std::wstring next_username_suffix =
          base::NumberToWString(i + kInitialDuplicateUsernameIndex);
      // Create a new user name that fits in |kWindowsUsernameBufferLength|
      if (next_username.size() + next_username_suffix.size() >
          (kWindowsUsernameBufferLength - 1)) {
        next_username =
            next_username.substr(0, (kWindowsUsernameBufferLength - 1) -
                                        next_username_suffix.size()) +
            next_username_suffix;
      } else {
        next_username += next_username_suffix;
      }
      LOGFN(VERBOSE) << "Username '" << new_username
                     << "' already exists. Trying '" << next_username << "'";

      err = wcscpy_s(new_username, std::size(new_username),
                     next_username.c_str());
      if (err != 0) {
        LOGFN(ERROR) << "wcscpy_s errno=" << err;
        return E_FAIL;
      }

      continue;
    } else if (FAILED(hr)) {
      LOGFN(ERROR) << "AddUser hr=" << putHR(hr);
      return hr;
    }

    *sid = ::SysAllocString(new_sid);
    *final_username = ::SysAllocString(new_username);
    return S_OK;
  }

  return HRESULT_FROM_WIN32(NERR_UserExists);
}

HRESULT OSUserManager::SetDefaultPasswordChangePolicies(
    const wchar_t* domain,
    const wchar_t* username) {
  USER_INFO_1008 info1008;
  DWORD error;
  memset(&info1008, 0, sizeof(info1008));
  info1008.usri1008_flags =
      UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD | UF_NORMAL_ACCOUNT;
  NET_API_STATUS nsts = ::NetUserSetInfo(
      domain, username, 1008, reinterpret_cast<LPBYTE>(&info1008), &error);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "NetUserSetInfo(set password policies) nsts=" << nsts;
  }
  return HRESULT_FROM_WIN32(nsts);
}

HRESULT OSUserManager::ChangeUserPassword(const wchar_t* domain,
                                          const wchar_t* username,
                                          const wchar_t* old_password,
                                          const wchar_t* new_password) {
  LOGFN(VERBOSE);
  LPBYTE domain_server_buffer = nullptr;
  HRESULT hr =
      GetDomainControllerServerForDomain(domain, &domain_server_buffer);
  if (FAILED(hr))
    return hr;

  std::unique_ptr<wchar_t, void (*)(wchar_t*)> domain_to_query(
      reinterpret_cast<wchar_t*>(domain_server_buffer), [](wchar_t* p) {
        if (p)
          ::NetApiBufferFree(p);
      });

  // Remove the UF_PASSWD_CANT_CHANGE flag temporarily so that the password can
  // be changed.
  LPBYTE buffer = nullptr;
  NET_API_STATUS nsts =
      ::NetUserGetInfo(domain_to_query.get(), username, 4, &buffer);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "NetUserGetInfo(get password change flag) nsts=" << nsts;
    return HRESULT_FROM_WIN32(nsts);
  }

  USER_INFO_4* user_info = reinterpret_cast<USER_INFO_4*>(buffer);
  DWORD original_user_flags = user_info->usri4_flags;

  bool flags_changed = false;
  if ((user_info->usri4_flags & UF_PASSWD_CANT_CHANGE) != 0) {
    user_info->usri4_flags &= ~UF_PASSWD_CANT_CHANGE;
    nsts =
        ::NetUserSetInfo(domain_to_query.get(), username, 4, buffer, nullptr);
    if (nsts != NERR_Success) {
      LOGFN(ERROR) << "NetUserSetInfo(allow password change) nsts=" << nsts;
      ::NetApiBufferFree(buffer);
      return HRESULT_FROM_WIN32(nsts);
    }

    flags_changed = true;
  }

  NET_API_STATUS changepassword_nsts =
      ::NetUserChangePassword(domain, username, old_password, new_password);
  if (changepassword_nsts != NERR_Success) {
    LOGFN(ERROR) << "Unable to change password for '" << username
                 << "' domain '" << domain << "' nsts=" << changepassword_nsts;
  }

  if (flags_changed) {
    user_info->usri4_flags = original_user_flags;
    nsts =
        ::NetUserSetInfo(domain_to_query.get(), username, 4, buffer, nullptr);
    if (nsts != NERR_Success) {
      LOGFN(ERROR) << "NetUserSetInfo(reset password change flag) nsts="
                   << nsts;
    }
  }

  ::NetApiBufferFree(buffer);

  return HRESULT_FROM_WIN32(changepassword_nsts);
}

HRESULT OSUserManager::SetUserPassword(const wchar_t* domain,
                                       const wchar_t* username,
                                       const wchar_t* password) {
  LPBYTE domain_server_buffer = nullptr;
  HRESULT hr =
      GetDomainControllerServerForDomain(domain, &domain_server_buffer);
  if (FAILED(hr))
    return hr;

  std::unique_ptr<wchar_t, void (*)(wchar_t*)> domain_to_query(
      reinterpret_cast<wchar_t*>(domain_server_buffer), [](wchar_t* p) {
        if (p)
          ::NetApiBufferFree(p);
      });

  DWORD error = 0;
  USER_INFO_1003 info1003;
  NET_API_STATUS nsts;
  memset(&info1003, 0, sizeof(info1003));
  info1003.usri1003_password = const_cast<wchar_t*>(password);
  nsts = ::NetUserSetInfo(domain_to_query.get(), username, 1003,
                          reinterpret_cast<LPBYTE>(&info1003), &error);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "Unable to change password for '" << username
                 << "' nsts=" << nsts;
  }

  return HRESULT_FROM_WIN32(nsts);
}

HRESULT OSUserManager::SetUserFullname(const wchar_t* domain,
                                       const wchar_t* username,
                                       const wchar_t* full_name) {
  LPBYTE domain_server_buffer = nullptr;
  HRESULT hr =
      GetDomainControllerServerForDomain(domain, &domain_server_buffer);
  if (FAILED(hr))
    return hr;

  std::unique_ptr<wchar_t, void (*)(wchar_t*)> domain_to_query(
      reinterpret_cast<wchar_t*>(domain_server_buffer), [](wchar_t* p) {
        if (p)
          ::NetApiBufferFree(p);
      });

  DWORD error = 0;
  USER_INFO_1011 info1011;
  NET_API_STATUS nsts;
  memset(&info1011, 0, sizeof(info1011));
  info1011.usri1011_full_name = const_cast<wchar_t*>(full_name);

  nsts = ::NetUserSetInfo(domain_to_query.get(), username, 1011,
                          reinterpret_cast<LPBYTE>(&info1011), &error);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "Unable to change full name on the account for '"
                 << username << "' nsts=" << nsts;
  }

  return HRESULT_FROM_WIN32(nsts);
}

HRESULT OSUserManager::IsWindowsPasswordValid(const wchar_t* domain,
                                              const wchar_t* username,
                                              const wchar_t* password) {
  // Check if the user exists before trying to log them on, because an error
  // of ERROR_LOGON_FAILURE will be returned if the user does not exist
  // or if the password is invalid. This function only wants to return
  // S_FALSE on an ERROR_LOGON_FAILURE if the user exists.
  PSID sid;
  HRESULT hr = GetUserSID(domain, username, &sid);

  if (SUCCEEDED(hr)) {
    ::LocalFree(sid);
    base::win::ScopedHandle handle;
    hr = CreateLogonToken(domain, username, password, /*interactive=*/true,
                          &handle);
    if (SUCCEEDED(hr))
      return hr;

    if (hr == HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE)) {
      return S_FALSE;
      // The following error codes represent sign in restrictions for the user
      // that are returned if the user's password is valid. In these cases we
      // don't want to return that the password is not valid. This is used to
      // make sure that we don't think we need to update the user's password
      // when in fact it is valid but they just can't sign in.
    } else if (hr == HRESULT_FROM_WIN32(ERROR_ACCOUNT_RESTRICTION) ||
               hr == HRESULT_FROM_WIN32(ERROR_INVALID_LOGON_HOURS) ||
               hr == HRESULT_FROM_WIN32(ERROR_INVALID_WORKSTATION) ||
               hr == HRESULT_FROM_WIN32(ERROR_ACCOUNT_DISABLED) ||
               hr == HRESULT_FROM_WIN32(ERROR_LOGON_TYPE_NOT_GRANTED)) {
      return S_OK;
    }
  }

  return hr;
}

HRESULT OSUserManager::CreateLogonToken(const wchar_t* domain,
                                        const wchar_t* username,
                                        const wchar_t* password,
                                        bool interactive,
                                        base::win::ScopedHandle* token) {
  return ::credential_provider::CreateLogonToken(domain, username, password,
                                                 interactive, token);
}

HRESULT OSUserManager::GetUserSID(const wchar_t* domain,
                                  const wchar_t* username,
                                  std::wstring* sid_string) {
  DCHECK(sid_string);
  sid_string->clear();

  PSID sid;
  HRESULT hr = GetUserSID(domain, username, &sid);

  if (SUCCEEDED(hr)) {
    wchar_t* sid_buffer;
    if (::ConvertSidToStringSid(sid, &sid_buffer)) {
      *sid_string = sid_buffer;
      ::LocalFree(sid_buffer);
    } else {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "ConvertSidToStringSid hr=" << putHR(hr);
    }
    ::LocalFree(sid);
  }

  return hr;
}

HRESULT OSUserManager::GetUserSID(const wchar_t* domain,
                                  const wchar_t* username,
                                  PSID* sid) {
  DCHECK(username);
  DCHECK(sid);

  char sid_buffer[256];
  DWORD sid_length = std::size(sid_buffer);
  wchar_t user_domain_buffer[kWindowsDomainBufferLength];
  DWORD domain_length = std::size(user_domain_buffer);
  SID_NAME_USE use;
  std::wstring username_with_domain = std::wstring(domain) + L"\\" + username;

  if (!::LookupAccountName(nullptr, username_with_domain.c_str(), sid_buffer,
                           &sid_length, user_domain_buffer, &domain_length,
                           &use)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());

    LOGFN(VERBOSE) << "LookupAccountName failed with hr=" << putHR(hr);

    wchar_t sid_buffer_temp[256];
    if (FAILED(GetSidFromDomainAccountInfo(domain, username, sid_buffer_temp,
                                           std::size(sid_buffer_temp)))) {
      LOGFN(ERROR) << "GetSidFromDomainAccountInfo failed";

      return hr;
    }

    if (!::ConvertStringSidToSid(sid_buffer_temp, sid)) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "ConvertStringSidToSid sid=" << sid
                   << " hr=" << putHR(hr);
      return hr;
    }

    return S_OK;
  }

  // Check that the domain of the user found with LookupAccountName matches what
  // is requested.
  if (wcsicmp(domain, user_domain_buffer) != 0) {
    LOGFN(ERROR) << "Domain mismatch " << domain << " " << user_domain_buffer;

    return HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
  }

  *sid = ::LocalAlloc(LMEM_FIXED, sid_length);
  ::CopySid(sid_length, *sid, sid_buffer);

  return S_OK;
}

HRESULT OSUserManager::FindUserBySID(const wchar_t* sid,
                                     wchar_t* username,
                                     DWORD username_size,
                                     wchar_t* domain,
                                     DWORD domain_size) {
  PSID psid;
  if (!::ConvertStringSidToSidW(sid, &psid)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ConvertStringSidToSidW sid=" << sid << " hr=" << putHR(hr);
    return hr;
  }

  HRESULT hr = S_OK;
  DWORD name_length = username ? username_size : 0;
  wchar_t local_domain_buffer[kWindowsDomainBufferLength];
  DWORD domain_length = std::size(local_domain_buffer);
  SID_NAME_USE use;
  if (!::LookupAccountSid(nullptr, psid, username, &name_length,
                          local_domain_buffer, &domain_length, &use)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    if (hr != HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
      if (username_size == 0 &&
          hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
        hr = S_OK;
      }
    }
  }

  if (domain_size) {
    if (domain_size <= domain_length)
      return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    wcscpy_s(domain, domain_size, local_domain_buffer);
  }

  std::wstring username_str = (username == nullptr) ? L"" : username;
  std::wstring domain_str = (domain == nullptr) ? L"" : domain;
  LOGFN(VERBOSE) << "username=" << username_str << " domain=" << domain_str;

  ::LocalFree(psid);
  return hr;
}

HRESULT OSUserManager::FindUserBySidWithFallback(const wchar_t* sid,
                                                 wchar_t* username,
                                                 DWORD username_length,
                                                 wchar_t* domain,
                                                 DWORD domain_length) {
  HRESULT hr = OSUserManager::Get()->FindUserBySID(
      sid, username, username_length, domain, domain_length);

  if (FAILED(hr)) {
    // Although FindUserBySID is failed, we can still obtain the domain and
    // username from the user properties. This is especially needed if an AD
    // workstation can't reach domain controller to login an account which
    // previously logged in on the same device.
    if (SUCCEEDED(GetUserProperty(sid, base::UTF8ToWide(kKeyDomain), domain,
                                  &domain_length)) &&
        SUCCEEDED(GetUserProperty(sid, base::UTF8ToWide(kKeyUsername), username,
                                  &username_length))) {
      LOGFN(VERBOSE) << "Obtained domain: " << domain
                     << " and user: " << username << " from registry!";
      hr = S_OK;
    } else {
      hr = E_FAIL;
    }
  }
  return hr;
}

bool OSUserManager::IsUserDomainJoined(const std::wstring& sid) {
  LOGFN(VERBOSE) << "sid=" << sid;

  wchar_t username[kWindowsUsernameBufferLength];
  wchar_t domain[kWindowsDomainBufferLength];

  HRESULT hr = FindUserBySidWithFallback(
      sid.c_str(), username, std::size(username), domain, std::size(domain));

  if (FAILED(hr)) {
    LOGFN(ERROR) << "IsUserDomainJoined sid=" << sid << " hr=" << putHR(hr);
    return hr;
  }

  bool domain_joined = !base::EqualsCaseInsensitiveASCII(
      domain, OSUserManager::GetLocalDomain().c_str());
  LOGFN(VERBOSE) << "sid=" << sid << " domain_joined=" << domain_joined;

  return domain_joined;
}

HRESULT OSUserManager::RemoveUser(const wchar_t* username,
                                  const wchar_t* password) {
  DCHECK(username);
  DCHECK(password);

  // Get the user's profile directory.
  base::win::ScopedHandle token;
  wchar_t profiledir[MAX_PATH + 1];

  std::wstring local_domain = OSUserManager::GetLocalDomain();

  // Get the user's profile directory.  Try a batch logon first, and if that
  // fails then try an interactive logon.
  HRESULT hr = CreateLogonToken(local_domain.c_str(), username, password,
                                /*interactive=*/false, &token);
  if (FAILED(hr))
    hr = CreateLogonToken(local_domain.c_str(), username, password,
                          /*interactive=*/true, &token);

  if (SUCCEEDED(hr)) {
    // Get the gaia user's profile directory so that it can be deleted.
    DWORD length = std::size(profiledir) - 1;
    if (!::GetUserProfileDirectory(token.Get(), profiledir, &length)) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      if (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        LOGFN(ERROR) << "GetUserProfileDirectory hr=" << putHR(hr);
      profiledir[0] = 0;
    }
  } else {
    LOGFN(ERROR) << "CreateLogonToken hr=" << putHR(hr);
  }

  // Remove the OS user.
  NET_API_STATUS nsts = ::NetUserDel(nullptr, username);
  if (nsts != NERR_Success)
    LOGFN(ERROR) << "NetUserDel nsts=" << nsts;

  // Force delete the user's profile directory.
  if (*profiledir && !base::DeletePathRecursively(base::FilePath(profiledir)))
    LOGFN(ERROR) << "base::DeleteFile";

  return S_OK;
}

HRESULT OSUserManager::ModifyUserAccessWithLogonHours(const wchar_t* domain,
                                                      const wchar_t* username,
                                                      bool allow) {
  BYTE buffer[21] = {0x0};
  memset(buffer, allow ? 0xff : 0x0, sizeof(buffer));
  USER_INFO_1020 user_info{UNITS_PER_WEEK, buffer};

  NET_API_STATUS nsts = ::NetUserSetInfo(
      domain, username, 1020, reinterpret_cast<BYTE*>(&user_info), nullptr);
  if (nsts != NERR_Success) {
    LOGFN(ERROR) << "NetUserSetInfo(set logon time) nsts=" << nsts;
    return HRESULT_FROM_WIN32(nsts);
  }

  return S_OK;
}

}  // namespace credential_provider
