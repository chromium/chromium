// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
#define CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_

#include "base/component_export.h"
#include "base/notreached.h"
// TODO(https://crbug.com/1164001): forward declare NetworkState when moved to
// chrome/browser/ash/.
#include "chromeos/network/network_state.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"

namespace chromeos {

// This is an interface for a chromeos portal detector that allows for
// observation of captive portal state. It supports retries based on a portal
// detector strategy.
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

  class Observer {
   public:
    // Called when portal detection is completed for |network|, or
    // when observers add themselves via AddAndFireObserver(). In the
    // second case, |network| is the active network and |state| is a
    // current portal state for the active network, which can be
    // currently in the unknown state, for instance, if portal
    // detection is in process for the active network. Note, that
    // |network| may be null.
    virtual void OnPortalDetectionCompleted(
        const NetworkState* network,
        const CaptivePortalStatus status) = 0;

    // Called on Shutdown, allows removal of observers. Primarly used in tests.
    virtual void OnShutdown() {}

   protected:
    virtual ~Observer() {}
  };

  NetworkPortalDetector(const NetworkPortalDetector&) = delete;
  NetworkPortalDetector& operator=(const NetworkPortalDetector&) = delete;

  virtual ~NetworkPortalDetector() {}

  // Adds |observer| to the observers list.
  virtual void AddObserver(Observer* observer) = 0;

  // Adds |observer| to the observers list and immediately calls
  // OnPortalDetectionCompleted() with the active network (which may
  // be null) and captive portal state for the active network (which
  // may be unknown, if, for instance, portal detection is in process
  // for the active network).
  //
  // WARNING: don't call this method from the Observer's ctors or
  // dtors, as it implicitly calls OnPortalDetectionCompleted(), which
  // is virtual.
  // TODO (ygorshenin@): find a way to avoid this restriction.
  virtual void AddAndFireObserver(Observer* observer) = 0;

  // Removes |observer| from the observers list.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns CaptivePortalStatus for the the default network or UNKNOWN.
  virtual CaptivePortalStatus GetCaptivePortalStatus() = 0;

  // Returns true if portal detection is enabled.
  virtual bool IsEnabled() = 0;

  // Enable portal detection. This method is needed because we can't
  // check current network for portal state unless user accepts EULA.
  // If |start_detection| is true and NetworkPortalDetector was
  // disabled previously, portal detection for the active network is
  // initiated by this method.
  virtual void Enable(bool start_detection) = 0;

  // Starts or restarts portal detection for the default network. If not
  // currently in the idle state, does nothing. Returns true if a new portal
  // detection attempt was started.
  virtual void StartPortalDetection() = 0;

  // Sets current strategy according to |id|. If current detection id
  // doesn't equal to |id|, detection is restarted.
  virtual void SetStrategy(PortalDetectorStrategy::StrategyId id) = 0;

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

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::NetworkPortalDetector;
namespace network_portal_detector {
using ::chromeos::network_portal_detector::GetInstance;
using ::chromeos::network_portal_detector::InitializeForTesting;
using ::chromeos::network_portal_detector::IsInitialized;
using ::chromeos::network_portal_detector::SetForTesting;
using ::chromeos::network_portal_detector::SetNetworkPortalDetector;
using ::chromeos::network_portal_detector::Shutdown;
}  // namespace network_portal_detector
}  // namespace ash

#endif  // CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_H_
