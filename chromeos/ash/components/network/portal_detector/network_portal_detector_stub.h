// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STUB_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STUB_H_

#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"

namespace ash {

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
  bool IsEnabled() override;
  void Enable() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STUB_H_
