// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_client.h"

namespace enterprise_companion {

mojo::NamedPlatformChannel::ServerName GetServerName() {
#if BUILDFLAG(IS_MAC)
  return "org.chromium.ChromeEnterpriseCompanion.service";
#elif BUILDFLAG(IS_LINUX)
  return "/run/Chromium/ChromeEnterpriseCompanion/service.sk";
#elif BUILDFLAG(IS_WIN)
  return L"ChromeEnterpriseCompanionService";
#endif
}

}  // namespace enterprise_companion
