// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/device_signals/core/common/platform_utils.h"

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/login_util.h"
#include "base/mac/mac_util.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/platform_wrapper.h"
#include "components/device_signals/core/common/signals_features.h"

namespace device_signals {

namespace {

std::optional<std::string> TryGetStringFromDictionary(NSDictionary* dictionary,
                                                      NSString* key) {
  NSString* value = [dictionary objectForKey:key];
  if (!value) {
    return std::nullopt;
  }

  return base::SysNSStringToUTF8(value);
}

std::string GetHardwareMACAddress() {
  NSString* const wifiInterfaceName = @"en0";
  base::apple::ScopedCFTypeRef<CFArrayRef> interfaces(
      SCNetworkInterfaceCopyAll());
  if (!interfaces) {
    return std::string();
  }

  NSString* macAddress = nil;
  CFIndex count = CFArrayGetCount(interfaces.get());
  for (CFIndex i = 0; i < count; i++) {
    SCNetworkInterfaceRef interface =
        (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(interfaces.get(), i);
    NSString* bsdName =
        (__bridge NSString*)SCNetworkInterfaceGetBSDName(interface);

    if ([bsdName caseInsensitiveCompare:wifiInterfaceName] != NSOrderedSame) {
      continue;
    }

    CFStringRef hardwareAddressRef =
        SCNetworkInterfaceGetHardwareAddressString(interface);
    if (hardwareAddressRef) {
      macAddress = (__bridge NSString*)hardwareAddressRef;
    }

    // Only one network interface can have the name "en0". No need to check
    // further.
    break;
  }

  return base::SysNSStringToUTF8(macAddress);
}

}  // namespace

base::FilePath GetCrowdStrikeZtaFilePath() {
  static constexpr base::FilePath::CharType kZtaFilePath[] = FILE_PATH_LITERAL(
      "/Library/Application Support/CrowdStrike/ZeroTrustAssessment/data.zta");
  return base::FilePath(kZtaFilePath);
}

std::string GetDeviceModel() {
  return base::SysInfo::HardwareModelName();
}

std::string GetSerialNumber() {
  return base::mac::GetPlatformSerialNumber();
}

SettingValue GetScreenlockSecured() {
  std::optional<bool> result = base::mac::IsScreenLockEnabled();
  if (!result.has_value()) {
    return SettingValue::UNKNOWN;
  }

  return result.value() ? SettingValue::ENABLED : SettingValue::DISABLED;
}

SettingValue GetDiskEncrypted() {
  base::FilePath fdesetup_path("/usr/bin/fdesetup");
  if (!PlatformWrapper::Get()->PathExists(fdesetup_path)) {
    return SettingValue::UNKNOWN;
  }

  base::CommandLine command(fdesetup_path);
  command.AppendArg("status");
  std::string output;
  if (!PlatformWrapper::Get()->Execute(command, &output)) {
    return SettingValue::UNKNOWN;
  }

  if (output.find("FileVault is On") != std::string::npos) {
    return SettingValue::ENABLED;
  }
  if (output.find("FileVault is Off") != std::string::npos) {
    return SettingValue::DISABLED;
  }

  return SettingValue::UNKNOWN;
}

std::vector<std::string> internal::GetMacAddressesImpl() {
  std::vector<std::string> result;
  struct ifaddrs* ifa = nullptr;

  if (getifaddrs(&ifa) != 0) {
    return result;
  }

  struct ifaddrs* interface = ifa;
  for (; interface != nullptr; interface = interface->ifa_next) {
    if (interface->ifa_addr == nullptr ||
        interface->ifa_addr->sa_family != AF_LINK) {
      continue;
    }
    struct sockaddr_dl* sdl =
        reinterpret_cast<struct sockaddr_dl*>(interface->ifa_addr);
    if (!sdl || sdl->sdl_alen != 6) {
      continue;
    }
    char* link_address = static_cast<char*>(LLADDR(sdl));
    result.push_back(base::StringPrintf(
        "%02x:%02x:%02x:%02x:%02x:%02x", link_address[0] & 0xff,
        link_address[1] & 0xff, link_address[2] & 0xff, link_address[3] & 0xff,
        link_address[4] & 0xff, link_address[5] & 0xff));
  }
  freeifaddrs(ifa);

  auto hardware_mac = GetHardwareMACAddress();
  if (!hardware_mac.empty()) {
    result.push_back(hardware_mac);
  }

  return result;
}

std::optional<CrowdStrikeSignals> GetCrowdStrikeSignals() {
  if (!enterprise_signals::features::IsDetectedAgentSignalCollectionEnabled()) {
    return std::nullopt;
  }

  static constexpr base::FilePath::CharType kFlaconCtlPath[] =
      FILE_PATH_LITERAL(
          "/Applications/Falcon.app/Contents/Resources/falconctl");
  base::FilePath falcon_ctl_path(kFlaconCtlPath);
  if (!PlatformWrapper::Get()->PathExists(falcon_ctl_path)) {
    return std::nullopt;
  }

  base::CommandLine command(falcon_ctl_path);
  command.AppendArg("info");
  std::string output;
  if (!PlatformWrapper::Get()->Execute(command, &output) && !output.empty()) {
    return std::nullopt;
  }

  // Output should be a plist encoded as XML.
  @autoreleasepool {
    NSData* plist_data = [NSData dataWithBytes:output.data()
                                        length:output.length()];
    NSDictionary* all_keys =
        base::apple::ObjCCastStrict<NSDictionary>([NSPropertyListSerialization
            propertyListWithData:plist_data
                         options:NSPropertyListImmutable
                          format:nil
                           error:nil]);

    if (!all_keys) {
      return std::nullopt;
    }

    const auto& customer_id = TryGetStringFromDictionary(all_keys, @"cid");
    const auto& agent_id = TryGetStringFromDictionary(all_keys, @"aid");
    if (customer_id || agent_id) {
      return CrowdStrikeSignals{
          base::ToLowerASCII(customer_id.value_or(std::string())),
          base::ToLowerASCII(agent_id.value_or(std::string()))};
    }
  }

  return std::nullopt;
}

}  // namespace device_signals
