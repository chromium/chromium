// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/simple_geolocation_request_test_monitor.h"

namespace ash {

SimpleGeolocationRequestTestMonitor::SimpleGeolocationRequestTestMonitor() =
    default;

SimpleGeolocationRequestTestMonitor::~SimpleGeolocationRequestTestMonitor() =
    default;

void SimpleGeolocationRequestTestMonitor::OnRequestCreated(
    SimpleGeolocationRequest* request) {}

void SimpleGeolocationRequestTestMonitor::OnStart(
    SimpleGeolocationRequest* request) {}

}  // namespace ash
