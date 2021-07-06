// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_MANAGER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"

class PrefRegistrySimple;

namespace chromeos {

namespace device_sync {

// Manages syncing and storing the user's devices that are registered with
// CryptAuth.
//
// CryptAuthDeviceManager periodically syncs the user's devices from CryptAuth
// to keep the list of unlock keys fresh. If a sync attempts fails, the manager
// will schedule the next sync more aggressively to recover.
class CryptAuthDeviceManager {
 public:
  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Respresents the success result of a sync attempt.
  enum class SyncResult { SUCCESS, FAILURE };

  // Represents whether the list of unlock keys has changed after a sync
  // attempt completes.
  enum class DeviceChangeResult {
    UNCHANGED,
    CHANGED,
  };

  class Observer {
   public:
    // Called when a sync attempt is started.
    virtual void OnSyncStarted() {}

    // Called when a sync attempt finishes with the |success| of the request.
    // |devices_changed| specifies if the sync caused the stored unlock keys to
    // change.
    virtual void OnSyncFinished(SyncResult sync_result,
                                DeviceChangeResult device_change_result) {}

    virtual ~Observer() = default;
  };

  CryptAuthDeviceManager();
  virtual ~CryptAuthDeviceManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts device manager to begin syncing devices.
  virtual void Start() = 0;

  // Skips the waiting period and forces a sync immediately. If a
  // sync attempt is already in progress, this function does nothing.
  // |invocation_reason| specifies the reason that the sync was triggered,
  // which is upload to the server.
  virtual void ForceSyncNow(cryptauth::InvocationReason invocation_reason) = 0;

  // Returns the timestamp of the last successful sync. If no sync
  // has ever been made, then returns a null base::Time object.
  virtual base::Time GetLastSyncTime() const = 0;

  // Returns the time to the next sync attempt.
  virtual base::TimeDelta GetTimeToNextAttempt() const = 0;

  // Returns true if a device sync attempt is currently in progress.
  virtual bool IsSyncInProgress() const = 0;

  // Returns true if the last device sync failed and the manager is now
  // scheduling sync attempts more aggressively to recover. If no enrollment
  // has ever been recorded, then this function will also return true.
  virtual bool IsRecoveringFromFailure() const = 0;

  // Returns a list of all remote devices that have been synced.
  virtual std::vector<cryptauth::ExternalDeviceInfo> GetSyncedDevices()
      const = 0;

  // Returns a list of remote devices that can unlock the user's other devices.
  virtual std::vector<cryptauth::ExternalDeviceInfo> GetUnlockKeys() const = 0;
  // Like GetUnlockKeys(), but only returns Pixel devices.
  virtual std::vector<cryptauth::ExternalDeviceInfo> GetPixelUnlockKeys()
      const = 0;

  // Returns a list of remote devices that can host tether hotspots.
  virtual std::vector<cryptauth::ExternalDeviceInfo> GetTetherHosts() const = 0;
  // Like GetTetherHosts(), but only returns Pixel devices.
  virtual std::vector<cryptauth::ExternalDeviceInfo> GetPixelTetherHosts()
      const = 0;

 protected:
  // Invokes OnSyncStarted() on all observers.
  void NotifySyncStarted();

  // Invokes OnSyncFinished(|sync_result|, |device_change_result|) on all
  // observers.
  void NotifySyncFinished(SyncResult sync_result,
                          DeviceChangeResult device_change_result);

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceManager);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_MANAGER_H_
