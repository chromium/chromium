// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/user_info.h"

#include <string>

#include "base/check.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "chrome/updater/util/win_util.h"

namespace updater {

namespace {

// Sid of NT AUTHORITY\SYSTEM user.
constexpr wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

}  // namespace

HRESULT GetProcessUser(std::wstring* name,
                       std::wstring* domain,
                       std::wstring* sid) {
  auto token = base::win::AccessToken::FromCurrentProcess();
  if (!token) {
    return HRESULTFromLastError();
  }

  base::win::Sid user_sid = token->User();
  auto sddl = user_sid.ToSddlString();
  if (!sddl) {
    return E_FAIL;
  }

  if (sid) {
    *sid = *sddl;
  }

  if (name || domain) {
    // Use LookupAccountSid to resolve account name and domain.
    PSID psid = user_sid.GetPSID();
    DWORD name_len = 0;
    DWORD domain_len = 0;
    SID_NAME_USE use = SidTypeUnknown;
    ::LookupAccountSid(/*lpSystemName=*/nullptr, psid, /*lpName=*/nullptr,
                       &name_len, /*lpReferencedDomainName=*/nullptr,
                       &domain_len, &use);
    if (name_len > 0 && domain_len > 0) {
      std::wstring account_name(name_len, L'\0');
      std::wstring account_domain(domain_len, L'\0');
      if (::LookupAccountSid(nullptr, psid, account_name.data(), &name_len,
                             account_domain.data(), &domain_len, &use)) {
        // LookupAccountSid returns lengths including null terminator on the
        // first call, but writes the string without null on second call.
        // Trim the trailing null.
        account_name.resize(name_len);
        account_domain.resize(domain_len);
        if (name) {
          *name = std::move(account_name);
        }
        if (domain) {
          *domain = std::move(account_domain);
        }
      }
    }
  }

  return S_OK;
}

bool IsLocalSystemUser() {
  std::wstring user_sid;
  HRESULT hr = GetProcessUser(nullptr, nullptr, &user_sid);
  return SUCCEEDED(hr) && user_sid.compare(kSystemPrincipalSid) == 0;
}

HRESULT GetThreadUserSid(std::wstring* sid) {
  CHECK(sid);
  auto token = base::win::AccessToken::FromCurrentThread(
      /*open_as_self=*/false);
  if (!token) {
    return HRESULTFromLastError();
  }

  auto sddl = token->User().ToSddlString();
  if (!sddl) {
    return HRESULTFromLastError();
  }

  *sid = std::move(*sddl);
  return S_OK;
}

}  // namespace updater
