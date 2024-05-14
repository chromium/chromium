// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_util.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

#if BUILDFLAG(IS_WIN)
#include <Windows.h>  // For GetComputerNameW()
// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32
#include <wincred.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_APPLE)
#include <stddef.h>
#include <sys/sysctl.h>
#endif

#if BUILDFLAG(IS_MAC)
#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include <limits.h>  // For HOST_NAME_MAX
#endif

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#if BUILDFLAG(IS_WIN)
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "base/win/wmi.h"
#endif
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "base/ios/device_util.h"
#endif

namespace policy {

namespace em = enterprise_management;

std::string GetMachineName() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_FUCHSIA)
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, HOST_NAME_MAX) == 0)  // Success.
    return hostname;
  return std::string();
#elif BUILDFLAG(IS_IOS)
  // Use the Vendor ID as the machine name.
  return ios::device_util::GetVendorId();
#elif BUILDFLAG(IS_MAC)
  // Do not use NSHost currentHost, as it's very slow. http://crbug.com/138570
  SCDynamicStoreContext context = {0, NULL, NULL, NULL};
  base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
      kCFAllocatorDefault, CFSTR("chrome_sync"), NULL, &context));
  base::apple::ScopedCFTypeRef<CFStringRef> machine_name(
      SCDynamicStoreCopyLocalHostName(store.get()));
  if (machine_name.get())
    return base::SysCFStringRefToUTF8(machine_name.get());

  // Fall back to get computer name.
  base::apple::ScopedCFTypeRef<CFStringRef> computer_name(
      SCDynamicStoreCopyComputerName(store.get(), NULL));
  if (computer_name.get())
    return base::SysCFStringRefToUTF8(computer_name.get());

  // If all else fails, return to using a slightly nicer version of the hardware
  // model. Warning: This will soon return just a useless "Mac" string.
  std::string model = base::SysInfo::HardwareModelName();
  std::optional<base::SysInfo::HardwareModelNameSplit> split =
      base::SysInfo::SplitHardwareModelNameDoNotUse(model);

  if (!split) {
    return model;
  }

  return split.value().category;
#elif BUILDFLAG(IS_WIN)
  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
  DWORD size = std::size(computer_name);
  if (::GetComputerNameW(computer_name, &size)) {
    std::string result;
    bool conversion_successful = base::WideToUTF8(computer_name, size, &result);
    DCHECK(conversion_successful);
    return result;
  }
  return std::string();
#elif BUILDFLAG(IS_ANDROID)
  return std::string();
#elif BUILDFLAG(IS_CHROMEOS)
  NOTREACHED_IN_MIGRATION();
  return std::string();
#else
#error Unsupported platform
#endif
}

std::string GetOSVersion() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  return base::SysInfo::OperatingSystemVersion();
#elif BUILDFLAG(IS_WIN)
  base::win::OSInfo::VersionNumber version_number =
      base::win::OSInfo::GetInstance()->version_number();
  return base::StringPrintf("%u.%u.%u.%u", version_number.major,
                            version_number.minor, version_number.build,
                            version_number.patch);
#else
  NOTREACHED_IN_MIGRATION();
  return std::string();
#endif
}

std::string GetOSPlatform() {
  return std::string(version_info::GetOSType());
}

std::string GetOSArchitecture() {
  return base::SysInfo::OperatingSystemArchitecture();
}

std::string GetOSUsername() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_APPLE)
  struct passwd* creds = getpwuid(getuid());
  if (!creds || !creds->pw_name)
    return std::string();

  return creds->pw_name;
#elif BUILDFLAG(IS_WIN)
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
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  if (!user_manager::UserManager::IsInitialized())
    return std::string();
  auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return std::string();
  return user->GetAccountId().GetUserEmail();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (init_params->DeviceAccount()) {
    return init_params->DeviceAccount()->raw_email;
  }
  // Fallback if init params are missing.
  struct passwd* creds = getpwuid(getuid());
  if (!creds || !creds->pw_name)
    return std::string();

  return creds->pw_name;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40200780): This should be fully implemented when there is
  // support in fuchsia.
  return std::string();
#else
  NOTREACHED_IN_MIGRATION();
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

std::string GetDeviceName() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::string(
      ash::system::StatisticsProvider::GetInstance()->GetMachineID().value_or(
          ""));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (init_params->DeviceProperties() &&
      init_params->DeviceProperties()->serial_number.has_value()) {
    return init_params->DeviceProperties()->serial_number.value();
  }
  return GetMachineName();
#else
  return GetMachineName();
#endif
}

std::unique_ptr<em::BrowserDeviceIdentifier> GetBrowserDeviceIdentifier() {
  std::unique_ptr<em::BrowserDeviceIdentifier> device_identifier =
      std::make_unique<em::BrowserDeviceIdentifier>();
  device_identifier->set_computer_name(GetMachineName());
#if BUILDFLAG(IS_WIN)
  device_identifier->set_serial_number(base::WideToUTF8(
      base::win::WmiComputerSystemInfo::Get().serial_number()));
#else
  device_identifier->set_serial_number("");
#endif
  return device_identifier;
}

#if BUILDFLAG(IS_WIN)
void GetBrowserDeviceIdentifierAsync(
    base::OnceCallback<
        void(std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>)>
        callback) {
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTaskAndReplyWithResult(FROM_HERE,
                                   base::BindOnce(&GetBrowserDeviceIdentifier),
                                   std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN)

bool IsMachineLevelUserCloudPolicyType(const std::string& type) {
  return type == dm_protocol::kChromeMachineLevelUserCloudPolicyType;
}

}  // namespace policy
