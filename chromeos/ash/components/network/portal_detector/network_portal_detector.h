// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_

#include "base/component_export.h"

namespace ash {

// This is an interface class for the chromeos portal detector.
// See network_portal_detector_impl.h for details.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkPortalDetector {
 public:
  NetworkPortalDetector(const NetworkPortalDetector&) = delete;
  NetworkPortalDetector& operator=(const NetworkPortalDetector&) = delete;

  virtual ~NetworkPortalDetector() {}

  // Returns true if portal detection is enabled.
  virtual bool IsEnabled() = 0;

  // Enable portal detection. Once enabled, portal detection for the default
  // network will be handled any time the default network state changes.
  virtual void Enable() = 0;

 protected:
  NetworkPortalDetector() {}
};

// Manages a global NetworkPortalDetector instance that can be accessed across
// all ChromeOS components.
namespace network_portal_detector {
// Gets the instance of the NetworkPortalDetector. Return value should
// be used carefully in tests, because it can be changed "on the fly"
// by calls to InitializeForTesting().
COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkPortalDetector* GetInstance();

// Returns |true| if NetworkPortalDetector was Initialized and it is safe to
// call GetInstance.
COMPONENT_EXPORT(CHROMEOS_NETWORK) bool IsInitialized();

// Deletes the instance of the NetworkPortalDetector.
COMPONENT_EXPORT(CHROMEOS_NETWORK) void Shutdown();

COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetNetworkPortalDetector(NetworkPortalDetector* network_portal_detector);

// Initializes network portal detector for testing. The
// |network_portal_detector| will be owned by the internal pointer
// and deleted by Shutdown().
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void InitializeForTesting(NetworkPortalDetector* network_portal_detector);

// Returns true if the network portal detector has been set for testing.
COMPONENT_EXPORT(CHROMEOS_NETWORK) bool SetForTesting();

}  // namespace network_portal_detector

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
