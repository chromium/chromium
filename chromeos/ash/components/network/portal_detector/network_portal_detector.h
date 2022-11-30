// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_

#include <string>

#include "base/component_export.h"
#include "base/notreached.h"

namespace ash {

// This is an interface class for the chromeos portal detector.
// See network_portal_detector_impl.h for details.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkPortalDetector {
 public:
  enum CaptivePortalStatus {
    CAPTIVE_PORTAL_STATUS_UNKNOWN = 0,
    CAPTIVE_PORTAL_STATUS_OFFLINE = 1,
    CAPTIVE_PORTAL_STATUS_ONLINE = 2,
    CAPTIVE_PORTAL_STATUS_PORTAL = 3,
    CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED = 4,
    CAPTIVE_PORTAL_STATUS_COUNT,
    kMaxValue = CAPTIVE_PORTAL_STATUS_COUNT  // For UMA_HISTOGRAM_ENUMERATION
  };

  NetworkPortalDetector(const NetworkPortalDetector&) = delete;
  NetworkPortalDetector& operator=(const NetworkPortalDetector&) = delete;

  virtual ~NetworkPortalDetector() {}

  // Returns CaptivePortalStatus for the the default network or UNKNOWN.
  virtual CaptivePortalStatus GetCaptivePortalStatus() = 0;

  // Returns true if portal detection is enabled.
  virtual bool IsEnabled() = 0;

  // Enable portal detection. This will do nothing if the EULA has not been
  // accepted (i.e. OOBE has not completed) since we do not want to show a
  // captive portal signin page until the user accepts the EULA. Once accepted,
  // Enable() should be called again.
  // Once enabled, portal detection for the default network will be handled any
  // time the default network state changes.
  virtual void Enable() = 0;

  // Returns non-localized string representation of |status|.
  static std::string CaptivePortalStatusString(CaptivePortalStatus status);

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

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::NetworkPortalDetector;
namespace network_portal_detector {
using ::ash::network_portal_detector::GetInstance;
using ::ash::network_portal_detector::InitializeForTesting;
}  // namespace network_portal_detector
}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
