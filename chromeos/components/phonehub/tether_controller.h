// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_H_

#include <ostream>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace phonehub {

// Exposes Instant Tethering functionality to Phone Hub.
class TetherController {
 public:
  enum class Status {
    // The device is ineligible for Instant Tethering, potentially due to the
    // flag being disabled (on Chrome OS or on the phone) or due to an
    // enterprise policy.
    kIneligibleForFeature,

    // Instant Tethering is available for use, but currently a connection is
    // unavailable. There are a variety of reasons why this may be the case:
    // the feature could have been disabled in settings, the phone may not have
    // cellular reception, or the phone may not have Google Play Services
    // notifications enabled, which are required for the feature.
    kConnectionUnavailable,

    // It is possible to connect, but no connection is active or in progress.
    // This state can occur if a previously-active connection has been
    // disconnected.
    kConnectionAvailable,

    // Initiating an Instant Tethering connection.
    kConnecting,

    // Connected via Instant Tethering.
    kConnected
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called the status has changed; use GetStatus() to get the new status.
    virtual void OnTetherStatusChanged() = 0;
  };

  TetherController(const TetherController&) = delete;
  TetherController& operator=(const TetherController&) = delete;
  virtual ~TetherController();

  virtual Status GetStatus() const = 0;

  // Attempts to find an available Instant Tethering connection. For a
  // connection to be available, the phone must be nearby, have reception, and
  // have Google Play Services notifications enabled. This function is a no-op
  // if the state is not kConnectionUnavailable.
  virtual void ScanForAvailableConnection() = 0;

  // Initiates an Instant Tethering connection. This function is a no-op if the
  // state is not one of kConnectionUnavailable or kConnectionAvailable.
  virtual void AttemptConnection() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  TetherController();

  void NotifyStatusChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

std::ostream& operator<<(std::ostream& stream, TetherController::Status status);

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_TETHER_CONTROLLER_H_
