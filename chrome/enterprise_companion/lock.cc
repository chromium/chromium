// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/lock.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "components/named_system_lock/lock.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#endif

namespace {

#if BUILDFLAG(IS_LINUX)
constexpr char kLockName[] = "/" PRODUCT_FULLNAME_STRING ".lock";
#elif BUILDFLAG(IS_MAC)
constexpr char kLockName[] = MAC_BUNDLE_IDENTIFIER_STRING ".lock";
#elif BUILDFLAG(IS_WIN)
constexpr wchar_t kLockName[] = L"Global\\G" PRODUCT_FULLNAME_STRING;

base::win::SecurityDescriptor GetAdminDaclSecurityDescriptor() {
  base::win::SecurityDescriptor sd;
  sd.set_owner(base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators));
  sd.set_group(base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators));
  sd.SetDaclEntry(base::win::WellKnownSid::kLocalSystem,
                  base::win::SecurityAccessMode::kGrant, GENERIC_ALL, 0);
  sd.SetDaclEntry(base::win::WellKnownSid::kBuiltinAdministrators,
                  base::win::SecurityAccessMode::kGrant, GENERIC_ALL, 0);
  return sd;
}
#endif

}  // namespace

namespace enterprise_companion {

std::unique_ptr<ScopedLock> CreateScopedLock(base::TimeDelta timeout) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return named_system_lock::ScopedLock::Create(kLockName, timeout);
#elif BUILDFLAG(IS_WIN)
  base::win::SecurityDescriptor sd = GetAdminDaclSecurityDescriptor();
  SECURITY_DESCRIPTOR absolute_sd = sd.ToAbsolute();
  SECURITY_ATTRIBUTES sa = {sizeof(sa), &absolute_sd, FALSE};
  return named_system_lock::ScopedLock::Create(kLockName, &sa, timeout);
#endif
}

}  // namespace enterprise_companion
