// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_device.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/mobile_device_list.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

// The values are taken from GetUnifiedPlatform() function
// in content/common/user_agent.cc file
const char kAndroidUnifiedPlatformName[] = "Linux; Android 10; K";
const char kChromeOSUnifiedPlatformName[] = "X11; CrOS x86_64 14541.0.0";
const char kMacOSUnifiedPlatformName[] = "Macintosh; Intel Mac OS X 10_15_7";
const char kWindowsUnifiedPlatformName[] = "Windows NT 10.0; Win64; x64";
const char kFuchsiaUnifiedPlatformName[] = "Fuchsia";
const char kLinuxUnifiedPlatformName[] = "X11; Linux x86_64";
const char kFrozenUserAgentTemplate[] =
    "Mozilla/5.0 (%s) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s.0.0.0 "
    "%s"
    "Safari/537.36";

struct Platform {
  const char* name;
  const char* unified_name;
  bool mobile_support = false;
};

Platform kAndroidPlatform{"Android", kAndroidUnifiedPlatformName, true};

Platform kChromeOSPlatform{
    "Chrome OS",
    kChromeOSUnifiedPlatformName,
    false,
};

Platform kChromiumOSPlatform{
    "Chromium OS",
    kChromeOSUnifiedPlatformName,
    false,
};

Platform kMacOSPlatform{
    "macOS",
    kMacOSUnifiedPlatformName,
    false,
};

Platform kWindowsPlatform{
    "Windows",
    kWindowsUnifiedPlatformName,
    false,
};

Platform kFuchsiaPlatform{
    "Fuchsia",
    kFuchsiaUnifiedPlatformName,
    false,
};

Platform kLinuxPlatform{
    "Linux",
    kLinuxUnifiedPlatformName,
    false,
};

const Platform* kPlatformsWithReducedUserAgentSupport[] = {
    &kAndroidPlatform, &kChromeOSPlatform, &kChromiumOSPlatform,
    &kMacOSPlatform,   &kWindowsPlatform,  &kFuchsiaPlatform,
    &kLinuxPlatform,
};

}  // namespace

MobileDevice::MobileDevice() = default;
MobileDevice::MobileDevice(const MobileDevice&) = default;
MobileDevice::~MobileDevice() = default;
MobileDevice& MobileDevice::operator=(const MobileDevice&) = default;

Status MobileDevice::FindMobileDevice(std::string device_name,
                                      MobileDevice* mobile_device) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kMobileDevices, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.has_value())
    return Status(kUnknownError, "could not parse mobile device list because " +
                                     parsed_json.error().message);

  if (!parsed_json->is_dict())
    return Status(kUnknownError, "malformed device metrics dictionary");
  base::Value::Dict& mobile_devices = parsed_json->GetDict();

  const base::Value::Dict* device =
      mobile_devices.FindDictByDottedPath(device_name);
  if (!device)
    return Status(kUnknownError, "must be a valid device");

  MobileDevice tmp_mobile_device;
  const std::string* maybe_ua = device->FindString("userAgent");
  if (!maybe_ua) {
    return Status(kUnknownError,
                  "malformed device user agent: should be a string");
  }
  tmp_mobile_device.user_agent = *maybe_ua;

  absl::optional<int> maybe_width = device->FindInt("width");
  absl::optional<int> maybe_height = device->FindInt("height");
  if (!maybe_width) {
    return Status(kUnknownError,
                  "malformed device width: should be an integer");
  }
  if (!maybe_height) {
    return Status(kUnknownError,
                  "malformed device height: should be an integer");
  }

  absl::optional<double> maybe_device_scale_factor =
      device->FindDouble("deviceScaleFactor");
  if (!maybe_device_scale_factor) {
    return Status(kUnknownError,
                  "malformed device scale factor: should be a double");
  }
  absl::optional<bool> touch = device->FindBool("touch");
  if (!touch) {
    return Status(kUnknownError, "malformed touch: should be a bool");
  }
  absl::optional<bool> mobile = device->FindBool("mobile");
  if (!mobile) {
    return Status(kUnknownError, "malformed mobile: should be a bool");
  }
  tmp_mobile_device.device_metrics = DeviceMetrics(
      *maybe_width, *maybe_height, *maybe_device_scale_factor, *touch, *mobile);

  *mobile_device = std::move(tmp_mobile_device);
  return Status(kOk);
}

// The idea is borrowed from
// https://developer.chrome.com/docs/privacy-sandbox/user-agent/snippets/
bool MobileDevice::GuessPlatform(const std::string& user_agent,
                                 std::string* platform) {
  static const std::vector<std::pair<std::string, std::string>>
      prefix_to_platform = {
          std::make_pair("Mozilla/5.0 (Lin", kAndroidPlatform.name),
          std::make_pair("Mozilla/5.0 (Win", kWindowsPlatform.name),
          std::make_pair("Mozilla/5.0 (Mac", kMacOSPlatform.name),
          std::make_pair("Mozilla/5.0 (X11; C", kChromeOSPlatform.name),
          std::make_pair("Mozilla/5.0 (X11; L", kLinuxPlatform.name),
          std::make_pair("Mozilla/5.0 (Fuchsia", kFuchsiaPlatform.name),
      };
  for (auto p : prefix_to_platform) {
    if (base::StartsWith(user_agent, p.first)) {
      *platform = p.second;
      return true;
    }
  }
  return false;
}

std::vector<std::string> MobileDevice::GetReducedUserAgentPlatforms() {
  std::vector<std::string> result;
  for (const Platform* p : kPlatformsWithReducedUserAgentSupport) {
    result.push_back(p->name);
  }
  return result;
}

Status MobileDevice::GetReducedUserAgent(std::string major_version,
                                         std::string* reduced_user_agent) {
  if (!client_hints.has_value()) {
    return Status{kUnknownError,
                  "unable to construct userAgent without client hints"};
  }
  for (const Platform* p : kPlatformsWithReducedUserAgentSupport) {
    if (base::StringPiece(p->name) != client_hints->platform) {
      continue;
    }
    std::string device_compat =
        client_hints->mobile && p->mobile_support ? "Mobile " : "";
    *reduced_user_agent =
        base::StringPrintf(kFrozenUserAgentTemplate, p->unified_name,
                           major_version.c_str(), device_compat.c_str());
    return Status{kOk};
  }
  return Status{kUnknownError,
                "unable to construct userAgent for platform: \"" +
                    client_hints->platform + "\""};
}
