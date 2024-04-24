// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"

#include "base/logging.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

namespace {

bool set_for_testing_ = false;
NetworkPortalDetector* network_portal_detector_ = nullptr;

}  // namespace

namespace network_portal_detector {

void InitializeForTesting(NetworkPortalDetector* network_portal_detector) {
  if (network_portal_detector) {
    CHECK(!set_for_testing_) << "InitializeForTesting is called twice";
    delete network_portal_detector_;
    network_portal_detector_ = network_portal_detector;
    set_for_testing_ = true;
  } else {
    network_portal_detector_ = nullptr;
    set_for_testing_ = false;
  }
}

bool IsInitialized() {
  return network_portal_detector_;
}

bool SetForTesting() {
  return set_for_testing_;
}

void Shutdown() {
  CHECK(network_portal_detector_ || set_for_testing_)
      << "Shutdown() called without Initialize()";
  delete network_portal_detector_;
  network_portal_detector_ = nullptr;
}

NetworkPortalDetector* GetInstance() {
  CHECK(network_portal_detector_) << "GetInstance() called before Initialize()";
  return network_portal_detector_;
}

void SetNetworkPortalDetector(NetworkPortalDetector* network_portal_detector) {
  CHECK(!network_portal_detector_)
      << "NetworkPortalDetector was initialized twice.";
  NET_LOG(EVENT) << "SetNetworkPortalDetector";
  network_portal_detector_ = network_portal_detector;
}

}  // namespace network_portal_detector

}  // namespace ash
