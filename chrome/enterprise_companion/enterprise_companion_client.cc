// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_client.h"

#include "chrome/enterprise_companion/enterprise_companion_branding.h"

namespace enterprise_companion {

namespace {
#if BUILDFLAG(IS_MAC)
constexpr char kServerName[] = MAC_BUNDLE_IDENTIFIER_STRING ".service";
#elif BUILDFLAG(IS_LINUX)
constexpr char kServerName[] =
    "/run/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING "/service.sk";
#elif BUILDFLAG(IS_WIN)
constexpr wchar_t kServerName[] = PRODUCT_FULLNAME_STRING L"Service";
#endif
}  // namespace

mojo::NamedPlatformChannel::ServerName GetServerName() {
  return kServerName;
}

}  // namespace enterprise_companion
