// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_device.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/mobile_device_list.h"
#include "chrome/test/chromedriver/chrome/status.h"

MobileDevice::MobileDevice() = default;
MobileDevice::MobileDevice(const MobileDevice&) = default;
MobileDevice::~MobileDevice() = default;
MobileDevice& MobileDevice::operator=(const MobileDevice&) = default;

Status FindMobileDevice(std::string device_name, MobileDevice* mobile_device) {
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
