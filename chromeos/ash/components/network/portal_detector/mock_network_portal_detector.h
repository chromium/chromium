// Copyright 2017 The Chromium Authors
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

  MOCK_METHOD0(IsEnabled, bool());
  MOCK_METHOD0(Enable, void());
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_MOCK_NETWORK_PORTAL_DETECTOR_H_
