// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HAMMERD_HAMMERD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HAMMERD_HAMMERD_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"

namespace dbus {
class Bus;
}

namespace ash {

// Client for hammerd service - the service that manages pairing and updates for
// physically connected bases of detachable devices (hammers). The client
// listens for dbus signals related to:
//  * the connected base firmware updating state
//  * the connected base pairing events.
// The client forwards the received signals to its observers (together with any
// data extracted from the signal object).
class COMPONENT_EXPORT(HAMMERD) HammerdClient {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Base firmware requires an update.
    virtual void BaseFirmwareUpdateNeeded() = 0;

    // Base firmware has started updating - BaseFirmwareUpdateCompleted() is
    // expected to be dispatched when the update completes.
    virtual void BaseFirmwareUpdateStarted() = 0;

    // Base firmware update has completed successfully.
    virtual void BaseFirmwareUpdateSucceeded() = 0;

    // Base firmware update has failed.
    virtual void BaseFirmwareUpdateFailed() = 0;

    // A base has been attached, and it was successfully authenticated.
    // |base_id| - identifies the authenticated base.
    virtual void PairChallengeSucceeded(
        const std::vector<uint8_t>& base_id) = 0;

    // A base has been attached, but was not successfully authenticated.
    virtual void PairChallengeFailed() = 0;

    // An invalid base has been connected.
    virtual void InvalidBaseConnected() = 0;
  };

  HammerdClient();
  virtual ~HammerdClient();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static HammerdClient* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HAMMERD_HAMMERD_CLIENT_H_
