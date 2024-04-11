// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_UPDATE_ENGINE_UPDATE_ENGINE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_UPDATE_ENGINE_UPDATE_ENGINE_CLIENT_H_

#include <stdint.h>

#include <compare>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"

namespace ash {

class FakeUpdateEngineClient;

// UpdateEngineClient is used to communicate with the update engine.
class COMPONENT_EXPORT(ASH_DBUS_UPDATE_ENGINE) UpdateEngineClient
    : public chromeos::DBusClient {
 public:
  // The result code used for RequestUpdateCheck().
  enum UpdateCheckResult {
    UPDATE_RESULT_SUCCESS,
    UPDATE_RESULT_FAILED,
    UPDATE_RESULT_NOTIMPLEMENTED,
  };

  // Holds information related to end-of-life.
  struct EolInfo {
    auto operator<=>(const EolInfo&) const = default;

    // The End of Life date. |eol_date.is_null()| will be true to signify an
    // invalid value. More than one classes will use this UpdateEngineClient, so
    // this field is used to maintain consistency instead of converting the End
    // of Life date, that is received in days since epoch, in possibly different
    // ways and at different locations.
    base::Time eol_date;

    // The extended updates date, which should be before eol_date.
    // |extended_date.is_null()| will be true to signify an empty value.
    base::Time extended_date;

    // Whether user opt-in is required to receive extended updates.
    bool extended_opt_in_required = false;
  };

  // Interface for observing changes from the update engine.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the status is updated.
    virtual void UpdateStatusChanged(
        const update_engine::StatusResult& status) {}

    // Called when the user's one time permission on update over cellular
    // connection has been granted.
    virtual void OnUpdateOverCellularOneTimePermissionGranted() {}
  };

  // Returns the global instance if initialized. May return null.
  static UpdateEngineClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance used on Linux desktop, if
  // no instance already exists.
  static void InitializeFake();

  // Creates and initializes a fake global instance for unit tests.
  static FakeUpdateEngineClient* InitializeFakeForTest();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  UpdateEngineClient(const UpdateEngineClient&) = delete;
  UpdateEngineClient& operator=(const UpdateEngineClient&) = delete;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Called once RequestUpdateCheck() is complete. Takes one parameter:
  // - UpdateCheckResult: the result of the update check.
  using UpdateCheckCallback = base::OnceCallback<void(UpdateCheckResult)>;

  // Requests an update check and calls |callback| when completed.
  virtual void RequestUpdateCheck(UpdateCheckCallback callback) = 0;

  // Requests an update check and calls |callback| when completed.
  // Will skip applying the update if there is one and the version in
  // |update_engine::StatusResult| will be updated.
  virtual void RequestUpdateCheckWithoutApplying(
      UpdateCheckCallback callback) = 0;

  // Reboots if update has been performed.
  virtual void RebootAfterUpdate() = 0;

  // Starts Rollback.
  virtual void Rollback() = 0;

  // Called once CanRollbackCheck() is complete. Takes one parameter:
  // - bool: the result of the rollback availability check.
  using RollbackCheckCallback = base::OnceCallback<void(bool can_rollback)>;

  // Checks if Rollback is available and calls |callback| when completed.
  virtual void CanRollbackCheck(RollbackCheckCallback callback) = 0;

  // Called once GetChannel() is complete. Takes one parameter;
  // - string: the channel name like "beta-channel".
  using GetChannelCallback =
      base::OnceCallback<void(const std::string& channel_name)>;

  // Returns the last status the object received from the update engine.
  //
  // Ideally, the D-Bus client should be state-less, but there are clients
  // that need this information.
  virtual update_engine::StatusResult GetLastStatus() = 0;

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
                          GetChannelCallback callback) = 0;

  // Called once GetStatusAdvanced() is complete. Takes one parameter;
  // - EolInfo: Please look at EolInfo for param details, all params related to
  //            end-of-life will be place within this struct.
  using GetEolInfoCallback = base::OnceCallback<void(EolInfo eol_info)>;

  // Get EndOfLife info for the device and calls |callback| when completed. This
  // method should be used in place of GetEolInfo.
  virtual void GetEolInfo(GetEolInfoCallback callback) = 0;

  // Either allow or disallow receiving updates over cellular connections.
  // Synchronous (blocking) method.
  virtual void SetUpdateOverCellularPermission(bool allowed,
                                               base::OnceClosure callback) = 0;

  // Called once SetUpdateOverCellularOneTimePermission() is complete. Takes one
  // parameter;
  // - success: indicates whether the permission is set successfully.
  using UpdateOverCellularOneTimePermissionCallback =
      base::OnceCallback<void(bool success)>;

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
      UpdateOverCellularOneTimePermissionCallback callback) = 0;

  // Returns true if |target_channel| is more stable than |current_channel|.
  static bool IsTargetChannelMoreStable(const std::string& current_channel,
                                        const std::string& target_channel);

  // Enables or disables the feature value in Update Engine.
  virtual void ToggleFeature(const std::string& feature, bool enable) = 0;

  // Gets the value of a feature in Update Engine. Returns null result on error.
  using IsFeatureEnabledCallback =
      base::OnceCallback<void(std::optional<bool> result)>;
  virtual void IsFeatureEnabled(const std::string& feature,
                                IsFeatureEnabledCallback callback) = 0;

  // Apply a downloaded but deferred update. When `shutdown_after_update` is set
  // to true, shutdown after applying the update, otherwise reboot. The callback
  // will run on dbus call failure.
  virtual void ApplyDeferredUpdate(bool shutdown_after_update,
                                   base::OnceClosure failure_callback) = 0;

 protected:
  // Initialize() should be used instead.
  UpdateEngineClient();
  ~UpdateEngineClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_UPDATE_ENGINE_UPDATE_ENGINE_CLIENT_H_
