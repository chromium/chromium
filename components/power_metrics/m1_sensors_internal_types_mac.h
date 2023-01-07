// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_M1_SENSORS_INTERNAL_TYPES_MAC_H_
#define COMPONENTS_POWER_METRICS_M1_SENSORS_INTERNAL_TYPES_MAC_H_

#include <stdint.h>

// From:
// https://opensource.apple.com/source/IOHIDFamily/IOHIDFamily-421.6/IOHIDFamily/IOHIDEventTypes.h.auto.html

#define IOHIDEventFieldBase(type) (type << 16)

constexpr int64_t kIOHIDEventTypeTemperature = 15;

// From:
// https://opensource.apple.com/source/IOHIDFamily/IOHIDFamily-421.6/IOHIDFamily/AppleHIDUsageTables.h

// Usage pages
constexpr int kHIDPage_AppleVendor = 0xff00;

// Usage keys for `kHIDPage_AppleVendor`
constexpr int kHIDUsage_AppleVendor_TemperatureSensor = 0x0005;

#endif  // COMPONENTS_POWER_METRICS_M1_SENSORS_INTERNAL_TYPES_MAC_H_
