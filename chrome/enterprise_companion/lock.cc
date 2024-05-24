// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/lock.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/named_system_lock/lock.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/atl.h"
#endif

namespace {

#if BUILDFLAG(IS_LINUX)
constexpr char kLockName[] = "/ChromeEnterpriseCompanion.lock";
#elif BUILDFLAG(IS_MAC)
constexpr char kLockName[] = "org.chromium.ChromeEnterpriseCompanion.lock";
#elif BUILDFLAG(IS_WIN)
constexpr wchar_t kLockName[] = L"Global\\GChromeEnterpriseCompanion";

CSecurityDesc GetAdminDaclSecurityDescriptor() {
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), GENERIC_ALL);
  dacl.AddAllowedAce(Sids::Admins(), GENERIC_ALL);
  CSecurityDesc sd;
  sd.SetOwner(Sids::Admins());
  sd.SetGroup(Sids::Admins());
  sd.SetDacl(dacl);
  sd.MakeAbsolute();
  return sd;
}
#endif

}  // namespace

namespace enterprise_companion {

std::unique_ptr<ScopedLock> CreateScopedLock() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return named_system_lock::ScopedLock::Create(kLockName, base::Seconds(0));
#elif BUILDFLAG(IS_WIN)
  CSecurityAttributes sa =
      CSecurityAttributes(GetAdminDaclSecurityDescriptor());
  return named_system_lock::ScopedLock::Create(kLockName, &sa,
                                               base::Seconds(0));
#endif
}

}  // namespace enterprise_companion
