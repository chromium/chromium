// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"

namespace password_manager {

// Represents the on-device encryption state on a device.
// TODO(crbug.com/540854648): Refine the set of possible states.
enum class OnDeviceEncryptionState {
  // Represents situations when we don't have yet sufficient information for
  // figuring-out the actual on-device encryption state.
  kOnDeviceEncryptionStateNotAvailable,
  // On-device encryption is not enabled for the user.
  kOnDeviceEncryptionNotEnabled,
  // On-device encryption is enabled for the user, but the device is not ready
  // (locked).
  kDeviceNotReady,
  // On-device encryption is enabled for the user, and the device is ready
  // (unlocked).
  kDeviceReady,
  // User disabled syncing of passwords and passkeys.
  kPasswordAndPasskeySyncDisabled,
  // Profile is not signed in to a primary Google account.
  kProfileNotSignedIn,
  // Profile has an account, but credentials were invalidated and re-auth is
  // required (sync is paused).
  kProfileSignInPending,
};

// Base class that monitors and maintains the on-device encryption state for a
// specific data type (e.g., passkeys or passwords). Subclasses observe relevant
// underlying services and update the state via SetState(), which notifies
// registered observers.
class OnDeviceEncryptionStateTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer when the encryption state changes.
    virtual void OnDeviceEncryptionStateChanged(
        OnDeviceEncryptionStateTracker* tracker,
        OnDeviceEncryptionState previous_state,
        OnDeviceEncryptionState new_state) = 0;
    // Notifies the observer that the state tracker is shutting down. Observers
    // outliving the tracker should override this method to call
    // `tracker->RemoveObserver(this)` or clear their raw/unretained pointers.
    virtual void OnDeviceEncryptionStateTrackerShuttingDown(
        OnDeviceEncryptionStateTracker* tracker) = 0;
  };

  OnDeviceEncryptionStateTracker();
  virtual ~OnDeviceEncryptionStateTracker();
  OnDeviceEncryptionStateTracker(const OnDeviceEncryptionStateTracker&) =
      delete;
  OnDeviceEncryptionStateTracker& operator=(
      const OnDeviceEncryptionStateTracker&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the current OnDeviceEncryptionState.
  OnDeviceEncryptionState GetEncryptionState() const;

  void SetStateForTesting(OnDeviceEncryptionState new_state);

 protected:
  // Sets the current state and notifies observers if the state has changed.
  void SetState(OnDeviceEncryptionState new_state);

 private:
  OnDeviceEncryptionState state_ =
      OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable;
  base::ObserverList<Observer> observer_list_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
