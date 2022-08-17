// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_MOCK_NETWORK_PORTAL_DETECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_MOCK_NETWORK_PORTAL_DETECTOR_H_

#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockNetworkPortalDetector : public NetworkPortalDetector {
 public:
  MockNetworkPortalDetector();

  MockNetworkPortalDetector(const MockNetworkPortalDetector&) = delete;
  MockNetworkPortalDetector& operator=(const MockNetworkPortalDetector&) =
      delete;

  ~MockNetworkPortalDetector() override;

  MOCK_METHOD1(AddObserver, void(NetworkPortalDetector::Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(NetworkPortalDetector::Observer* observer));
  MOCK_METHOD1(AddAndFireObserver,
               void(NetworkPortalDetector::Observer* observer));
  MOCK_METHOD0(GetCaptivePortalStatus,
               NetworkPortalDetector::CaptivePortalStatus());
  MOCK_METHOD0(IsEnabled, bool());
  MOCK_METHOD0(Enable, void());
  MOCK_METHOD0(StartPortalDetection, void());
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_MOCK_NETWORK_PORTAL_DETECTOR_H_
