// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_
#define CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"

namespace chromeos {

// UpdateEngineClient is used to communicate with the update engine.
class CHROMEOS_EXPORT UpdateEngineClient : public DBusClient {
 public:
  // Edges for state machine
  //    IDLE->CHECKING_FOR_UPDATE
  //    CHECKING_FOR_UPDATE->IDLE
  //    CHECKING_FOR_UPDATE->UPDATE_AVAILABLE
  //    CHECKING_FOR_UPDATE->NEED_PERMISSION_TO_UPDATE
  //    ...
  //    FINALIZING->UPDATE_NEED_REBOOT
  // Any state can transition to REPORTING_ERROR_EVENT and then on to IDLE.
  enum UpdateStatusOperation {
    UPDATE_STATUS_ERROR = -1,
    UPDATE_STATUS_IDLE = 0,
    UPDATE_STATUS_CHECKING_FOR_UPDATE,
    UPDATE_STATUS_UPDATE_AVAILABLE,
    // User permission is needed to download an update on a cellular connection.
    UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE,
    UPDATE_STATUS_DOWNLOADING,
    UPDATE_STATUS_VERIFYING,
    UPDATE_STATUS_FINALIZING,
    UPDATE_STATUS_UPDATED_NEED_REBOOT,
    UPDATE_STATUS_REPORTING_ERROR_EVENT,
    UPDATE_STATUS_ATTEMPTING_ROLLBACK,
  };

  // The status of the ongoing update attempt.
  struct Status {
    UpdateStatusOperation status = UPDATE_STATUS_IDLE;
    // 0.0 - 1.0
    double download_progress = 0.0;
    // As reported by std::time().
    int64_t last_checked_time = 0;
    std::string new_version;
    // Valid during DOWNLOADING, in bytes.
    int64_t new_size = 0;
    // True if the update is actually a rollback and the device will be wiped
    // when rebooted.
    bool is_rollback = false;
  };

  // The result code used for RequestUpdateCheck().
  enum UpdateCheckResult {
    UPDATE_RESULT_SUCCESS,
    UPDATE_RESULT_FAILED,
    UPDATE_RESULT_NOTIMPLEMENTED,
  };

  // Interface for observing changes from the update engine.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the status is updated.
    virtual void UpdateStatusChanged(const Status& status) {}

    // Called when the user's one time permission on update over cellular
    // connection has been granted.
    virtual void OnUpdateOverCellularOneTimePermissionGranted() {}
  };

  ~UpdateEngineClient() override;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Called once RequestUpdateCheck() is complete. Takes one parameter:
  // - UpdateCheckResult: the result of the update check.
  using UpdateCheckCallback = base::Callback<void(UpdateCheckResult)>;

  // Requests an update check and calls |callback| when completed.
  virtual void RequestUpdateCheck(const UpdateCheckCallback& callback) = 0;

  // Reboots if update has been performed.
  virtual void RebootAfterUpdate() = 0;

  // Starts Rollback.
  virtual void Rollback() = 0;

  // Called once CanRollbackCheck() is complete. Takes one parameter:
  // - bool: the result of the rollback availability check.
  using RollbackCheckCallback = base::Callback<void(bool can_rollback)>;

  // Checks if Rollback is available and calls |callback| when completed.
  virtual void CanRollbackCheck(
      const RollbackCheckCallback& callback) = 0;

  // Called once GetChannel() is complete. Takes one parameter;
  // - string: the channel name like "beta-channel".
  using GetChannelCallback =
      base::Callback<void(const std::string& channel_name)>;

  // Returns the last status the object received from the update engine.
  //
  // Ideally, the D-Bus client should be state-less, but there are clients
  // that need this information.
  virtual Status GetLastStatus() = 0;

  // Changes the current channel of the device to the target
  // channel. If the target channel is a less stable channel than the
  // current channel, then the channel change happens immediately (at
  // the next update check).  If the target channel is a more stable
  // channel, then if |is_powerwash_allowed| is set to true, then also
  // the change happens immediately but with a powerwash if
  // required. Otherwise, the change takes effect eventually (when the
  // version on the target channel goes above the version number of
  // what the device currently has). |target_channel| should look like
  // "dev-channel", "beta-channel" or "stable-channel".
  virtual void SetChannel(const std::string& target_channel,
                          bool is_powerwash_allowed) = 0;

  // If |get_current_channel| is set to true, calls |callback| with
  // the name of the channel that the device is currently
  // on. Otherwise, it calls it with the name of the channel the
  // device is supposed to be (in case of a pending channel
  // change). On error, calls |callback| with an empty string.
  virtual void GetChannel(bool get_current_channel,
                          const GetChannelCallback& callback) = 0;

  // Called once GetEolStatus() is complete. Takes one parameter;
  // - EndOfLife Status: the end of life status of the device.
  using GetEolStatusCallback =
      base::OnceCallback<void(update_engine::EndOfLifeStatus status)>;

  // Get EndOfLife status of the device and calls |callback| when completed.
  virtual void GetEolStatus(GetEolStatusCallback callback) = 0;

  // Either allow or disallow receiving updates over cellular connections.
  // Synchronous (blocking) method.
  virtual void SetUpdateOverCellularPermission(
      bool allowed,
      const base::Closure& callback) = 0;

  // Called once SetUpdateOverCellularOneTimePermission() is complete. Takes one
  // parameter;
  // - success: indicates whether the permission is set successfully.
  using UpdateOverCellularOneTimePermissionCallback =
      base::Callback<void(bool success)>;

  // Sets a one time permission on a certain update in Update Engine which then
  // performs downloading of that update after RequestUpdateCheck() is invoked
  // in the |callback|.
  // - update_version: the Chrome OS version we want to update to.
  // - update_size: the size of that Chrome OS version in bytes.
  // These two parameters are a failsafe to prevent downloading an update that
  // the user didn't agree to. They should be set using the version and size we
  // received from update engine when it broadcasts NEED_PERMISSION_TO_UPDATE.
  // They are used by update engine to double-check with update server in case
  // there's a new update available or a delta update becomes a full update with
  // a larger size.
  virtual void SetUpdateOverCellularOneTimePermission(
      const std::string& update_version,
      int64_t update_size,
      const UpdateOverCellularOneTimePermissionCallback& callback) = 0;

  // Creates the instance.
  static UpdateEngineClient* Create(DBusClientImplementationType type);

  // Returns true if |target_channel| is more stable than |current_channel|.
  static bool IsTargetChannelMoreStable(const std::string& current_channel,
                                        const std::string& target_channel);

 protected:
  // Create() should be used instead.
  UpdateEngineClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_
