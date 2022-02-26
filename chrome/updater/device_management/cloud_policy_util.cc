// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Code in this file are branched from
// components/policy/core/common/cloud/cloud_policy_constants.cc and
// components/policy/core/common/cloud/cloud_policy_util.cc. In the long
// term, we should reuse the code (crbug/1219760).

#include "chrome/updater/device_management/cloud_policy_util.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include <Windows.h>  // For GetComputerNameW()
// SECURITY_WIN32 must be defined in order to get
// EXTENDED_NAME_FORMAT enumeration.
#define SECURITY_WIN32 1
#include <security.h>
#undef SECURITY_WIN32
#include <wincred.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_MAC)
#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <stddef.h>
#include <sys/sysctl.h>
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX)
#include <limits.h>  // For HOST_NAME_MAX
#endif

#include <utility>

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX)
#include "base/system/sys_info.h"
#endif

#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/version_info.h"

namespace updater {

namespace policy {

constexpr uint8_t kPolicyVerificationKey[] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00,
    0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xA7, 0xB3, 0xF9,
    0x0D, 0xC7, 0xC7, 0x8D, 0x84, 0x3D, 0x4B, 0x80, 0xDD, 0x9A, 0x2F, 0xF8,
    0x69, 0xD4, 0xD1, 0x14, 0x5A, 0xCA, 0x04, 0x4B, 0x1C, 0xBC, 0x28, 0xEB,
    0x5E, 0x10, 0x01, 0x36, 0xFD, 0x81, 0xEB, 0xE4, 0x3C, 0x16, 0x40, 0xA5,
    0x8A, 0xE6, 0x08, 0xEE, 0xEF, 0x39, 0x1F, 0x6B, 0x10, 0x29, 0x50, 0x84,
    0xCE, 0xEE, 0x33, 0x5C, 0x48, 0x4A, 0x33, 0xB0, 0xC8, 0x8A, 0x66, 0x0D,
    0x10, 0x11, 0x9D, 0x6B, 0x55, 0x4C, 0x9A, 0x62, 0x40, 0x9A, 0xE2, 0xCA,
    0x21, 0x01, 0x1F, 0x10, 0x1E, 0x7B, 0xC6, 0x89, 0x94, 0xDA, 0x39, 0x69,
    0xBE, 0x27, 0x28, 0x50, 0x5E, 0xA2, 0x55, 0xB9, 0x12, 0x3C, 0x79, 0x6E,
    0xDF, 0x24, 0xBF, 0x34, 0x88, 0xF2, 0x5E, 0xD0, 0xC4, 0x06, 0xEE, 0x95,
    0x6D, 0xC2, 0x14, 0xBF, 0x51, 0x7E, 0x3F, 0x55, 0x10, 0x85, 0xCE, 0x33,
    0x8F, 0x02, 0x87, 0xFC, 0xD2, 0xDD, 0x42, 0xAF, 0x59, 0xBB, 0x69, 0x3D,
    0xBC, 0x77, 0x4B, 0x3F, 0xC7, 0x22, 0x0D, 0x5F, 0x72, 0xC7, 0x36, 0xB6,
    0x98, 0x3D, 0x03, 0xCD, 0x2F, 0x68, 0x61, 0xEE, 0xF4, 0x5A, 0xF5, 0x07,
    0xAE, 0xAE, 0x79, 0xD1, 0x1A, 0xB2, 0x38, 0xE0, 0xAB, 0x60, 0x5C, 0x0C,
    0x14, 0xFE, 0x44, 0x67, 0x2C, 0x8A, 0x08, 0x51, 0x9C, 0xCD, 0x3D, 0xDB,
    0x13, 0x04, 0x57, 0xC5, 0x85, 0xB6, 0x2A, 0x0F, 0x02, 0x46, 0x0D, 0x2D,
    0xCA, 0xE3, 0x3F, 0x84, 0x9E, 0x8B, 0x8A, 0x5F, 0xFC, 0x4D, 0xAA, 0xBE,
    0xBD, 0xE6, 0x64, 0x9F, 0x26, 0x9A, 0x2B, 0x97, 0x69, 0xA9, 0xBA, 0x0B,
    0xBD, 0x48, 0xE4, 0x81, 0x6B, 0xD4, 0x4B, 0x78, 0xE6, 0xAF, 0x95, 0x66,
    0xC1, 0x23, 0xDA, 0x23, 0x45, 0x36, 0x6E, 0x25, 0xF3, 0xC7, 0xC0, 0x61,
    0xFC, 0xEC, 0x66, 0x9D, 0x31, 0xD4, 0xD6, 0xB6, 0x36, 0xE3, 0x7F, 0x81,
    0x87, 0x02, 0x03, 0x01, 0x00, 0x01};

const char kPolicyVerificationKeyHash[] = "1:356l7w";

std::string GetPolicyVerificationKey() {
  return std::string(reinterpret_cast<const char*>(kPolicyVerificationKey),
                     sizeof(kPolicyVerificationKey));
}

std::string GetMachineName() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, HOST_NAME_MAX) == 0)  // Success.
    return hostname;
  return std::string();
#elif BUILDFLAG(IS_MAC)
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
#else
  NOTREACHED();
  return std::string();
#endif
}

std::string GetOSVersion() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  return base::SysInfo::OperatingSystemVersion();
#elif BUILDFLAG(IS_WIN)
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

std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
GetBrowserDeviceIdentifier() {
  std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
      device_identifier =
          std::make_unique<enterprise_management::BrowserDeviceIdentifier>();
  device_identifier->set_computer_name(GetMachineName());
#if BUILDFLAG(IS_WIN)
  device_identifier->set_serial_number(base::WideToUTF8(
      base::win::WmiComputerSystemInfo::Get().serial_number()));
#else
  device_identifier->set_serial_number("");
#endif
  return device_identifier;
}

}  // namespace policy

}  // namespace updater
