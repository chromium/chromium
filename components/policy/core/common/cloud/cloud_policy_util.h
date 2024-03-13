// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_UTIL_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_UTIL_H_

#include <memory>
#include <string>

#include "components/policy/policy_export.h"
#include "components/version_info/channel.h"

#if BUILDFLAG(IS_WIN)
#include "base/functional/callback_forward.h"
#endif

namespace enterprise_management {
class BrowserDeviceIdentifier;
enum Channel : int;
}  // namespace enterprise_management

namespace policy {

// Returns the name of the machine. This function is platform specific.
POLICY_EXPORT std::string GetMachineName();

// Returns the OS version of the machine. This function is platform specific.
POLICY_EXPORT std::string GetOSVersion();

// Returns the OS platform of the machine. This function is platform specific.
POLICY_EXPORT std::string GetOSPlatform();

// Returns the bitness of the OS. This function is platform specific.
POLICY_EXPORT std::string GetOSArchitecture();

// Returns the username of the logged in user in the OS. This function is
// platform specific. Note that on Windows, this returns the username including
// the domain, whereas on POSIX, this just returns the username.
POLICY_EXPORT std::string GetOSUsername();

// Converts |version_info::Channel| to |enterprise_management::Channel|.
POLICY_EXPORT enterprise_management::Channel ConvertToProtoChannel(
    version_info::Channel channel);

// Returns the name of the device. This is equivalent to GetMachineName on
// non-CrOS platforms and returns the serial number of the device on CrOS.
POLICY_EXPORT std::string GetDeviceName();

// Returns the browser device identifier for non-CrOS platforms. It includes
// several identifiers we collect from the device.
POLICY_EXPORT std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
GetBrowserDeviceIdentifier();

#if BUILDFLAG(IS_WIN)
// Get browser device identifier for non-CrOS platforms, in a background thread.
// It includes several identifiers we collect from the device.
POLICY_EXPORT void GetBrowserDeviceIdentifierAsync(
    base::OnceCallback<
        void(std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>)>
        callback);
#endif  // BUILDFLAG(IS_WIN)

// Returns true if the given policy type corresponds to the machine-level user
// cloud policy type of the current platform.
POLICY_EXPORT bool IsMachineLevelUserCloudPolicyType(const std::string& type);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_UTIL_H_
