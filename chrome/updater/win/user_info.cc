// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/user_info.h"

#include <string>

#include "base/check.h"
#include "chrome/updater/util/win_util.h"

namespace updater {

namespace {

// Sid of NT AUTHORITY\SYSTEM user.
constexpr wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

}  // namespace

HRESULT GetProcessUser(std::wstring* name,
                       std::wstring* domain,
                       std::wstring* sid) {
  CSid current_sid;

  HRESULT hr = GetProcessUserSid(&current_sid);
  if (FAILED(hr)) {
    return hr;
  }

  if (sid) {
    *sid = current_sid.Sid();
  }
  if (name) {
    *name = current_sid.AccountName();
  }
  if (domain) {
    *domain = current_sid.Domain();
  }

  return S_OK;
}

HRESULT GetProcessUserSid(CSid* sid) {
  CHECK(sid);

  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY) || !token.GetUser(sid)) {
    HRESULT hr = HRESULTFromLastError();
    std::wstring thread_sid;
    CHECK(FAILED(GetThreadUserSid(&thread_sid)));
    return hr;
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
  CAccessToken access_token;
  CSid user_sid;
  if (access_token.GetThreadToken(TOKEN_READ) &&
      access_token.GetUser(&user_sid)) {
    *sid = user_sid.Sid();
    return S_OK;
  }

  return HRESULTFromLastError();
}

}  // namespace updater
