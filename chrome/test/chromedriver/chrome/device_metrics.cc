// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/device_metrics.h"

DeviceMetrics::DeviceMetrics(int width,
                             int height,
                             double device_scale_factor,
                             bool touch,
                             bool mobile)
    : width(width),
      height(height),
      device_scale_factor(device_scale_factor),
      touch(touch),
      mobile(mobile) {}
