// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_device.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/client_hints.h"
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

Status ParsePresetDeviceMetrics(const base::Value::Dict& device_metrics_dict,
                                DeviceMetrics* device_metrics) {
  std::optional<int> maybe_width = device_metrics_dict.FindInt("width");
  std::optional<int> maybe_height = device_metrics_dict.FindInt("height");
  if (!maybe_width) {
    return Status(kUnknownError,
                  "malformed device width: should be an integer");
  }
  if (!maybe_height) {
    return Status(kUnknownError,
                  "malformed device height: should be an integer");
  }
  std::optional<double> maybe_device_scale_factor =
      device_metrics_dict.FindDouble("deviceScaleFactor");
  if (!maybe_device_scale_factor) {
    return Status(kUnknownError,
                  "malformed device scale factor: should be a double");
  }
  std::optional<bool> touch = device_metrics_dict.FindBool("touch");
  if (!touch) {
    return Status(kUnknownError, "malformed touch: should be a bool");
  }
  std::optional<bool> mobile = device_metrics_dict.FindBool("mobile");
  if (!mobile) {
    return Status(kUnknownError, "malformed mobile: should be a bool");
  }
  *device_metrics = DeviceMetrics(*maybe_width, *maybe_height,
                                  *maybe_device_scale_factor, *touch, *mobile);
  return Status{kOk};
}

Status ParsePresetClientHints(const base::Value::Dict& client_hints_dict,
                              ClientHints* client_hints) {
  std::optional<bool> mobile = client_hints_dict.FindBool("mobile");
  if (!mobile.has_value()) {
    return Status(kUnknownError,
                  "malformed clientHints.mobile: should be a boolean");
  }
  const std::string* platform = client_hints_dict.FindString("platform");
  if (!platform) {
    return Status(kUnknownError,
                  "malformed clientHints.platform: should be a string");
  }
  const std::string* platform_version =
      client_hints_dict.FindString("platformVersion");
  if (!platform_version) {
    return Status(kUnknownError,
                  "malformed clientHints.platformVersion: should be a string");
  }
  const std::string* architecture =
      client_hints_dict.FindString("architecture");
  if (!architecture) {
    return Status(kUnknownError,
                  "malformed clientHints.architecture: should be a string");
  }
  const std::string* model = client_hints_dict.FindString("model");
  if (!model) {
    return Status(kUnknownError,
                  "malformed clientHints.model: should be a string");
  }
  const std::string* bitness = client_hints_dict.FindString("bitness");
  if (!bitness) {
    return Status(kUnknownError,
                  "malformed clientHints.bitness: should be a string");
  }
  std::optional<bool> wow64 = client_hints_dict.FindBool("wow64");
  if (!wow64.has_value()) {
    return Status(kUnknownError,
                  "malformed clientHints.wow64: should be a boolean");
  }

  client_hints->architecture = *architecture;
  client_hints->bitness = *bitness;
  client_hints->mobile = mobile.value();
  client_hints->model = *model;
  client_hints->platform = *platform;
  client_hints->platform_version = *platform_version;
  client_hints->wow64 = wow64.value();
  return Status{kOk};
}

}  // namespace

MobileDevice::MobileDevice() = default;

MobileDevice::MobileDevice(const MobileDevice&) = default;

MobileDevice::MobileDevice(MobileDevice&&) = default;

MobileDevice& MobileDevice::operator=(const MobileDevice&) = default;

MobileDevice& MobileDevice::operator=(MobileDevice&&) = default;

MobileDevice::~MobileDevice() = default;

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

  Status status{kOk};

  // Parse device metrics
  const base::Value::Dict* maybe_device_metrics_dict =
      device->FindDict("deviceMetrics");
  if (!maybe_device_metrics_dict) {
    return Status(kUnknownError,
                  "malformed deviceMetrics: should be a dictionary");
  }
  DeviceMetrics device_metrics;
  status =
      ParsePresetDeviceMetrics(*maybe_device_metrics_dict, &device_metrics);
  if (status.IsError()) {
    return status;
  }
  tmp_mobile_device.device_metrics = std::move(device_metrics);

  // Parsing the client hints
  ClientHints client_hints;
  if (device->Find("clientHints")) {
    const base::Value::Dict* maybe_client_hints_dict =
        device->FindDict("clientHints");
    if (!maybe_client_hints_dict) {
      return Status(kUnknownError,
                    "malformed clientHints: should be a dictionary");
    }
    status = ParsePresetClientHints(*maybe_client_hints_dict, &client_hints);
    if (status.IsError()) {
      return status;
    }
  } else {
    // Client Hints have to be initialized with some default values.
    // Otherwise the browser will use its own Client Hints that will most likely
    // contradict the information in the User Agent.
    // If device type is "phone" then it is mobile, otherwise it is not.
    // However if there is no information about device type then we assume that
    // most likely it is mobile.
    client_hints.mobile = true;
    const std::string* maybe_type = device->FindString("type");
    if (maybe_type && *maybe_type != "phone") {
      client_hints.mobile = false;
    }
    client_hints.brands = std::vector<BrandVersion>();
    client_hints.full_version_list = std::vector<BrandVersion>();
    VLOG(logging::LOGGING_INFO)
        << "No 'clientHints' found. Emulation might be inadequate. "
        << "Inferring clientHints as: "
        << "{architecture='" << client_hints.architecture << "'"
        << ", bitness='" << client_hints.bitness << "'"
        << ", brands=[]"
        << ", fullVersionList=[]"
        << ", mobile=" << std::boolalpha << client_hints.mobile << ", model='"
        << client_hints.model << "'"
        << ", platform='" << client_hints.platform << "'"
        << ", platformVersion='" << client_hints.platform_version << "'"
        << ", wow64=" << std::boolalpha << client_hints.wow64 << "}";
  }
  tmp_mobile_device.client_hints = std::move(client_hints);

  *mobile_device = std::move(tmp_mobile_device);
  return Status(kOk);
}

Status MobileDevice::GetKnownMobileDeviceNamesForTesting(
    std::vector<std::string>* result) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kMobileDevices, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.has_value()) {
    return Status(kUnknownError, "could not parse mobile device list because " +
                                     parsed_json.error().message);
  }

  if (!parsed_json->is_dict()) {
    return Status(kUnknownError, "malformed device metrics dictionary");
  }
  base::Value::Dict& mobile_devices = parsed_json->GetDict();

  for (auto [key, value] : mobile_devices) {
    result->push_back(std::move(key));
  }
  return Status{kOk};
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

Status MobileDevice::GetReducedUserAgent(
    std::string major_version,
    std::string* reduced_user_agent) const {
  if (!client_hints.has_value()) {
    return Status{kUnknownError,
                  "unable to construct userAgent without client hints"};
  }
  for (const Platform* p : kPlatformsWithReducedUserAgentSupport) {
    if (std::string_view(p->name) != client_hints->platform) {
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
