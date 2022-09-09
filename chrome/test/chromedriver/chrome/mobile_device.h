// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_

#include <memory>
#include <string>

#include "chrome/test/chromedriver/chrome/device_metrics.h"

class Status;

struct MobileDevice {
  MobileDevice();
  ~MobileDevice();
  std::unique_ptr<DeviceMetrics> device_metrics;
  std::string user_agent;
};

Status FindMobileDevice(std::string device_name,
                        std::unique_ptr<MobileDevice>* mobile_device);

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_MOBILE_DEVICE_H_
