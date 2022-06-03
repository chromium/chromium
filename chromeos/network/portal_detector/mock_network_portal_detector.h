// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PORTAL_DETECTOR_MOCK_NETWORK_PORTAL_DETECTOR_H_
#define CHROMEOS_NETWORK_PORTAL_DETECTOR_MOCK_NETWORK_PORTAL_DETECTOR_H_

#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockNetworkPortalDetector : public NetworkPortalDetector {
 public:
  MockNetworkPortalDetector();

  MockNetworkPortalDetector(const MockNetworkPortalDetector&) = delete;
  MockNetworkPortalDetector& operator=(const MockNetworkPortalDetector&) =
      delete;

  ~MockNetworkPortalDetector() override;

  MOCK_METHOD1(AddObserver,
               void(chromeos::NetworkPortalDetector::Observer* observer));
  MOCK_METHOD1(RemoveObserver,
               void(chromeos::NetworkPortalDetector::Observer* observer));
  MOCK_METHOD1(AddAndFireObserver,
               void(chromeos::NetworkPortalDetector::Observer* observer));
  MOCK_METHOD0(GetCaptivePortalStatus,
               chromeos::NetworkPortalDetector::CaptivePortalStatus());
  MOCK_METHOD0(IsEnabled, bool());
  MOCK_METHOD1(Enable, void(bool start_detection));
  MOCK_METHOD0(StartPortalDetection, void());
  MOCK_METHOD1(SetStrategy,
               void(chromeos::PortalDetectorStrategy::StrategyId id));
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when //chromeos/network moved to ash.
namespace ash {
using ::chromeos::MockNetworkPortalDetector;
}  // namespace ash

#endif  // CHROMEOS_NETWORK_PORTAL_DETECTOR_MOCK_NETWORK_PORTAL_DETECTOR_H_
