// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_util.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <Windows.h>  // For GetComputerNameW()
// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32
#include <wincred.h>
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) || defined(OS_MACOSX)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(OS_MACOSX)
#include <stddef.h>
#include <sys/sysctl.h>
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include <limits.h>  // For HOST_NAME_MAX
#endif

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "components/version_info/version_info.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if defined(OS_WIN)
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#endif

namespace policy {

namespace em = enterprise_management;

std::string GetMachineName() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, HOST_NAME_MAX) == 0)  // Success.
    return hostname;
  return std::string();
#elif defined(OS_MACOSX)
// TODO(crbug.com/1024115): Find a different replacement for -[NSHost
// currentHost] on iOS.
#if !defined(OS_IOS)
  // Do not use NSHost currentHost, as it's very slow. http://crbug.com/138570
  SCDynamicStoreContext context = {0, NULL, NULL, NULL};
  base::ScopedCFTypeRef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
      kCFAllocatorDefault, CFSTR("chrome_sync"), NULL, &context));
  base::ScopedCFTypeRef<CFStringRef> machine_name(
      SCDynamicStoreCopyLocalHostName(store.get()));
  if (machine_name.get())
    return base::SysCFStringRefToUTF8(machine_name.get());

  // Fall back to get computer name.
  base::ScopedCFTypeRef<CFStringRef> computer_name(
      SCDynamicStoreCopyComputerName(store.get(), NULL));
  if (computer_name.get())
    return base::SysCFStringRefToUTF8(computer_name.get());
#endif  // !OS_IOS

  // If all else fails, return to using a slightly nicer version of the
  // hardware model.
  char modelBuffer[256];
  size_t length = sizeof(modelBuffer);
  if (!sysctlbyname("hw.model", modelBuffer, &length, NULL, 0)) {
    for (size_t i = 0; i < length; i++) {
      if (base::IsAsciiDigit(modelBuffer[i]))
        return std::string(modelBuffer, 0, i);
    }
    return std::string(modelBuffer, 0, length);
  }
  return std::string();
#elif defined(OS_WIN)
  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
  DWORD size = base::size(computer_name);
  if (::GetComputerNameW(computer_name, &size)) {
    std::string result;
    bool conversion_successful = base::WideToUTF8(computer_name, size, &result);
    DCHECK(conversion_successful);
    return result;
  }
  return std::string();
#else
  NOTREACHED();
  return std::string();
#endif
}

std::string GetOSVersion() {
#if defined(OS_LINUX) || defined(OS_MACOSX)
  return base::SysInfo::OperatingSystemVersion();
#elif defined(OS_WIN)
  base::win::OSInfo::VersionNumber version_number =
      base::win::OSInfo::GetInstance()->version_number();
  return base::StringPrintf("%d.%d.%d.%d", version_number.major,
                            version_number.minor, version_number.build,
                            version_number.patch);
#else
  NOTREACHED();
  return std::string();
#endif
}

std::string GetOSPlatform() {
  return version_info::GetOSType();
}

std::string GetOSArchitecture() {
  return base::SysInfo::OperatingSystemArchitecture();
}

std::string GetOSUsername() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) || defined(OS_MACOSX)
  struct passwd* creds = getpwuid(getuid());
  if (!creds || !creds->pw_name)
    return std::string();

  return creds->pw_name;
#elif defined(OS_WIN)
  WCHAR username[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
  DWORD username_length = sizeof(username);

  // The SAM compatible username works on both standalone workstations and
  // domain joined machines.  The form is "DOMAIN\username", where DOMAIN is the
  // the name of the machine for standalone workstations.
  if (!::GetUserNameEx(::NameSamCompatible, username, &username_length) ||
      username_length <= 0) {
    return std::string();
  }

  return base::WideToUTF8(username);
#elif defined(OS_CHROMEOS)
  if (!user_manager::UserManager::IsInitialized())
    return std::string();
  auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return std::string();
  return user->GetAccountId().GetUserEmail();
#else
  NOTREACHED();
  return std::string();
#endif
}

em::Channel ConvertToProtoChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return em::CHANNEL_UNKNOWN;
    case version_info::Channel::CANARY:
      return em::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return em::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return em::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return em::CHANNEL_STABLE;
  }
}

}  // namespace policy
