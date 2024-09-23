// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/m1_sensors_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDDeviceKeys.h>
#import <IOKit/hidsystem/IOHIDServiceClient.h>

#include <utility>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/power_metrics/m1_sensors_internal_types_mac.h"

extern "C" {

extern IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef);
extern int IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef client,
                                             CFDictionaryRef match);
extern CFTypeRef IOHIDServiceClientCopyEvent(IOHIDServiceClientRef,
                                             int64_t,
                                             int32_t,
                                             int64_t);
extern double IOHIDEventGetFloatValue(CFTypeRef, int32_t);
}

namespace power_metrics {

namespace {

std::optional<double> GetEventFloatValue(IOHIDServiceClientRef service,
                                         int64_t event_type) {
  base::apple::ScopedCFTypeRef<CFTypeRef> event(
      IOHIDServiceClientCopyEvent(service, event_type, 0, 0));
  if (!event)
    return std::nullopt;
  return IOHIDEventGetFloatValue(event.get(), IOHIDEventFieldBase(event_type));
}

}  // namespace

M1SensorsReader::TemperaturesCelsius::TemperaturesCelsius() = default;
M1SensorsReader::TemperaturesCelsius::TemperaturesCelsius(
    const TemperaturesCelsius&) noexcept = default;
M1SensorsReader::TemperaturesCelsius::~TemperaturesCelsius() = default;

M1SensorsReader::~M1SensorsReader() = default;

// static
std::unique_ptr<M1SensorsReader> M1SensorsReader::Create() {
  base::apple::ScopedCFTypeRef<IOHIDEventSystemClientRef> system(
      IOHIDEventSystemClientCreate(kCFAllocatorDefault));

  if (!system) {
    return nullptr;
  }

  NSDictionary* filter = @{
    @kIOHIDPrimaryUsagePageKey : @(kHIDPage_AppleVendor),
    @kIOHIDPrimaryUsageKey : @(kHIDUsage_AppleVendor_TemperatureSensor),
  };
  IOHIDEventSystemClientSetMatching(system.get(),
                                    base::apple::NSToCFPtrCast(filter));

  return base::WrapUnique(new M1SensorsReader(std::move(system)));
}

M1SensorsReader::TemperaturesCelsius M1SensorsReader::ReadTemperatures() {
  base::apple::ScopedCFTypeRef<CFArrayRef> services(
      IOHIDEventSystemClientCopyServices(system_.get()));

  // There are multiple temperature sensors on P-Cores and E-Cores. Count and
  // sum values to compute average later.
  int num_p_core_temp = 0;
  int num_e_core_temp = 0;
  double sum_p_core_temp = 0;
  double sum_e_core_temp = 0;

  for (CFIndex i = 0; i < CFArrayGetCount(services.get()); ++i) {
    IOHIDServiceClientRef service =
        (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services.get(), i);

    base::apple::ScopedCFTypeRef<CFStringRef> product(
        base::apple::CFCast<CFStringRef>(
            IOHIDServiceClientCopyProperty(service, CFSTR(kIOHIDProductKey))));
    if (!product) {
      continue;
    }

    if (CFStringHasPrefix(product.get(), CFSTR("pACC MTR Temp Sensor"))) {
      std::optional<double> temp =
          GetEventFloatValue(service, kIOHIDEventTypeTemperature);
      if (temp.has_value()) {
        num_p_core_temp += 1;
        sum_p_core_temp += temp.value();
      }
    }

    if (CFStringHasPrefix(product.get(), CFSTR("eACC MTR Temp Sensor"))) {
      std::optional<double> temp =
          GetEventFloatValue(service, kIOHIDEventTypeTemperature);
      if (temp.has_value()) {
        num_e_core_temp += 1;
        sum_e_core_temp += temp.value();
      }
    }
  }

  TemperaturesCelsius temperatures;
  if (num_p_core_temp > 0)
    temperatures.p_cores = sum_p_core_temp / num_p_core_temp;
  if (num_e_core_temp > 0)
    temperatures.e_cores = sum_e_core_temp / num_e_core_temp;

  return temperatures;
}

M1SensorsReader::M1SensorsReader(
    base::apple::ScopedCFTypeRef<IOHIDEventSystemClientRef> system)
    : system_(std::move(system)) {}

}  // namespace power_metrics
