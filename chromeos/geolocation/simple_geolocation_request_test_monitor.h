// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_TEST_MONITOR_H_
#define CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_TEST_MONITOR_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace chromeos {

class SimpleGeolocationRequest;

// This is global hook, that allows to monitor SimpleGeolocationRequest
// in tests.
//
// Note: we need COMPONENT_EXPORT(CHROMEOS_GEOLOCATION) for tests.
class COMPONENT_EXPORT(CHROMEOS_GEOLOCATION)
    SimpleGeolocationRequestTestMonitor {
 public:
  SimpleGeolocationRequestTestMonitor();

  SimpleGeolocationRequestTestMonitor(
      const SimpleGeolocationRequestTestMonitor&) = delete;
  SimpleGeolocationRequestTestMonitor& operator=(
      const SimpleGeolocationRequestTestMonitor&) = delete;

  virtual ~SimpleGeolocationRequestTestMonitor();
  virtual void OnRequestCreated(SimpleGeolocationRequest* request);
  virtual void OnStart(SimpleGeolocationRequest* request);
};

}  // namespace chromeos

#endif  // CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_TEST_MONITOR_H_
