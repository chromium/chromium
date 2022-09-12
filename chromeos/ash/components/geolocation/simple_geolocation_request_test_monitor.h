// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_TEST_MONITOR_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_TEST_MONITOR_H_

#include "base/component_export.h"

namespace ash {

class SimpleGeolocationRequest;

// This is global hook, that allows to monitor SimpleGeolocationRequest
// in tests.
//
// Note: we need COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) for
// tests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
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

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_TEST_MONITOR_H_
