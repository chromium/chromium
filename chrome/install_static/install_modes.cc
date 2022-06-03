// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_modes.h"

#include "chrome/install_static/buildflags.h"

namespace install_static {

namespace {

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
std::wstring GetClientsKeyPathForApp(const wchar_t* app_guid) {
  return std::wstring(L"Software\\Google\\Update\\Clients\\").append(app_guid);
}

std::wstring GetClientStateKeyPathForApp(const wchar_t* app_guid) {
  return std::wstring(L"Software\\Google\\Update\\ClientState\\")
      .append(app_guid);
}

std::wstring GetClientStateMediumKeyPathForApp(const wchar_t* app_guid) {
  return std::wstring(L"Software\\Google\\Update\\ClientStateMedium\\")
      .append(app_guid);
}
#else
std::wstring GetUnregisteredKeyPathForProduct() {
  return std::wstring(L"Software\\").append(kProductPathName);
}
#endif

}  // namespace

std::wstring GetClientsKeyPath(const wchar_t* app_guid) {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct();
#else
  return GetClientsKeyPathForApp(app_guid);
#endif
}

std::wstring GetClientStateKeyPath(const wchar_t* app_guid) {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct();
#else
  return GetClientStateKeyPathForApp(app_guid);
#endif
}

std::wstring GetClientStateMediumKeyPath(const wchar_t* app_guid) {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct();
#else
  return GetClientStateMediumKeyPathForApp(app_guid);
#endif
}

}  // namespace install_static
