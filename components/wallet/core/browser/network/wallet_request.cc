// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/wallet_request.h"

#include "base/values.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

namespace wallet {

namespace {

ClientInfo::ChromeClientInfo::Platform GetPlatform() {
#if BUILDFLAG(IS_WIN)
  return ClientInfo::ChromeClientInfo::PLATFORM_WINDOWS;
#elif BUILDFLAG(IS_MAC)
  return ClientInfo::ChromeClientInfo::PLATFORM_MACOS;
#elif BUILDFLAG(IS_CHROMEOS)
  return ClientInfo::ChromeClientInfo::PLATFORM_CHROMEOS;
#elif BUILDFLAG(IS_LINUX)
  return ClientInfo::ChromeClientInfo::PLATFORM_LINUX;
#elif BUILDFLAG(IS_ANDROID)
  return ClientInfo::ChromeClientInfo::PLATFORM_ANDROID;
#elif BUILDFLAG(IS_IOS)
  return ClientInfo::ChromeClientInfo::PLATFORM_IOS;
#else
  // Unsupported platforms like Fuchsia.
  return ClientInfo::ChromeClientInfo::PLATFORM_UNSPECIFIED;
#endif
}

}  // namespace

net::HttpRequestHeaders WalletRequest::GetRequestHeaders() const {
  return net::HttpRequestHeaders();
}

// static
ClientInfo WalletRequest::BuildClientInfo() {
  ClientInfo client_info;
  ClientInfo::ChromeClientInfo& chrome_client_info =
      *client_info.mutable_chrome_client_info();
  chrome_client_info.set_version(version_info::GetVersionNumber());
  chrome_client_info.set_platform(GetPlatform());
  return client_info;
}

}  // namespace wallet
