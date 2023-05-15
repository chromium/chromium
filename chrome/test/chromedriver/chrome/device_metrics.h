// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_METRICS_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_METRICS_H_

struct DeviceMetrics {
  DeviceMetrics(int width, int height, double device_scale_factor, bool touch,
                bool mobile);
  DeviceMetrics() = default;
  DeviceMetrics(const DeviceMetrics&) = default;
  ~DeviceMetrics() = default;

  DeviceMetrics& operator=(const DeviceMetrics&) = default;

  int width = 0;
  int height = 0;
  double device_scale_factor = 0;
  bool touch = true;
  bool mobile = true;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_METRICS_H_
