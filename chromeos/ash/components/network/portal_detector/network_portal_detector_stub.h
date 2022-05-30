// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STUB_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STUB_H_

#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkPortalDetectorStub
    : public NetworkPortalDetector {
 public:
  NetworkPortalDetectorStub();

  NetworkPortalDetectorStub(const NetworkPortalDetectorStub&) = delete;
  NetworkPortalDetectorStub& operator=(const NetworkPortalDetectorStub&) =
      delete;

  ~NetworkPortalDetectorStub() override;

 private:
  // NetworkPortalDetector:
  void AddObserver(Observer* observer) override;
  void AddAndFireObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  CaptivePortalStatus GetCaptivePortalStatus() override;
  bool IsEnabled() override;
  void Enable(bool start_detection) override;
  void StartPortalDetection() override;
  void SetStrategy(PortalDetectorStrategy::StrategyId id) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
using ::chromeos::NetworkPortalDetectorStub;
}

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STUB_H_
