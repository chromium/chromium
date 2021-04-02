// Copyright 2014 The Chromium Authors. All rights reserved.
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

MobileDevice::MobileDevice() {}
MobileDevice::~MobileDevice() {}

Status FindMobileDevice(std::string device_name,
                        std::unique_ptr<MobileDevice>* mobile_device) {
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          kMobileDevices, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.value)
    return Status(kUnknownError, "could not parse mobile device list because " +
                                     parsed_json.error_message);

  base::DictionaryValue* mobile_devices;
  if (!parsed_json.value->GetAsDictionary(&mobile_devices))
    return Status(kUnknownError, "malformed device metrics dictionary");

  base::DictionaryValue* device = NULL;
  if (!mobile_devices->GetDictionary(device_name, &device))
    return Status(kUnknownError, "must be a valid device");

  std::unique_ptr<MobileDevice> tmp_mobile_device(new MobileDevice());
  std::string device_metrics_string;
  if (!device->GetString("userAgent", &tmp_mobile_device->user_agent)) {
    return Status(kUnknownError,
                  "malformed device user agent: should be a string");
  }
  int width = 0;
  int height = 0;
  double device_scale_factor = 0.0;
  bool touch = true;
  bool mobile = true;
  if (!device->GetInteger("width",  &width)) {
    return Status(kUnknownError,
                  "malformed device width: should be an integer");
  }
  if (!device->GetInteger("height", &height)) {
    return Status(kUnknownError,
                  "malformed device height: should be an integer");
  }
  if (!device->GetDouble("deviceScaleFactor", &device_scale_factor)) {
    return Status(kUnknownError,
                  "malformed device scale factor: should be a double");
  }
  if (!device->GetBoolean("touch", &touch)) {
    return Status(kUnknownError,
                  "malformed touch: should be a bool");
  }
  if (!device->GetBoolean("mobile", &mobile)) {
    return Status(kUnknownError,
                  "malformed mobile: should be a bool");
  }
  tmp_mobile_device->device_metrics = std::make_unique<DeviceMetrics>(
      width, height, device_scale_factor, touch, mobile);

  *mobile_device = std::move(tmp_mobile_device);
  return Status(kOk);

}
