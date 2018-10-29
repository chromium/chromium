// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_POLICY_CONTROLLER_H_
#define CHROMEOS_DBUS_POWER_POLICY_CONTROLLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

// PowerPolicyController is responsible for sending Chrome's assorted power
// management preferences to the Chrome OS power manager.
class CHROMEOS_EXPORT PowerPolicyController
    : public PowerManagerClient::Observer {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(PowerManagerClient* power_manager_client);

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance. Initialize() must be called first.
  static PowerPolicyController* Get();

  // Reasons why a wake lock may be added.
  // TODO(derat): Remove this enum in favor of device::mojom::WakeLockReason
  // once this class has been moved to the device service:
  // https://crbug.com/702449
  enum WakeLockReason {
    REASON_AUDIO_PLAYBACK,
    REASON_VIDEO_PLAYBACK,
    REASON_OTHER,
  };

  // Note: Do not change these values; they are used by preferences.
  enum Action {
    ACTION_SUSPEND      = 0,
    ACTION_STOP_SESSION = 1,
    ACTION_SHUT_DOWN    = 2,
    ACTION_DO_NOTHING   = 3,
  };

  // Values of various power-management-related preferences.
  struct PrefValues {
    PrefValues();

    int ac_screen_dim_delay_ms;
    int ac_screen_off_delay_ms;
    int ac_screen_lock_delay_ms;
    int ac_idle_warning_delay_ms;
    int ac_idle_delay_ms;
    int battery_screen_dim_delay_ms;
    int battery_screen_off_delay_ms;
    int battery_screen_lock_delay_ms;
    int battery_idle_warning_delay_ms;
    int battery_idle_delay_ms;
    Action ac_idle_action;
    Action battery_idle_action;
    Action lid_closed_action;
    bool use_audio_activity;
    bool use_video_activity;
    double ac_brightness_percent;
    double battery_brightness_percent;
    bool allow_wake_locks;
    bool allow_screen_wake_locks;
    bool enable_auto_screen_lock;
    double presentation_screen_dim_delay_factor;
    double user_activity_screen_dim_delay_factor;
    bool wait_for_initial_user_activity;
    bool force_nonzero_brightness_for_user_activity;
    bool smart_dim_enabled;
  };

  // Returns a string describing |policy|.  Useful for tests.
  static std::string GetPolicyDebugString(
      const power_manager::PowerManagementPolicy& policy);

  // Delay in milliseconds between the screen being turned off and the screen
  // being locked. Used if the |enable_auto_screen_lock| pref is set but
  // |*_screen_lock_delay_ms| are unset or set to higher values than what this
  // constant would imply.
  static const int kScreenLockAfterOffDelayMs;

  // String added to a PowerManagementPolicy |reason| field if the policy has
  // been modified by preferences.
  static const char kPrefsReason[];

  bool honor_screen_wake_locks_for_test() const {
    return honor_screen_wake_locks_;
  }

  // Updates |prefs_policy_| with |values| and sends an updated policy.
  void ApplyPrefs(const PrefValues& values);

  // Registers a request to temporarily prevent the screen from getting dimmed
  // or turned off or the system from suspending in response to user inactivity
  // and sends an updated policy. |description| is a human-readable description
  // of the reason the lock was created. Returns a unique ID that can be passed
  // to RemoveWakeLock() later.
  // See the comment above WakeLock::Type for descriptions of the lock types.
  int AddScreenWakeLock(WakeLockReason reason, const std::string& description);
  int AddDimWakeLock(WakeLockReason reason, const std::string& description);
  int AddSystemWakeLock(WakeLockReason reason, const std::string& description);

  // Unregisters a request previously created via an Add*WakeLock() call
  // and sends an updated policy.
  void RemoveWakeLock(int id);

  // Adjusts policy while Chrome is exiting. The lid-closed action
  // is overridden to ensure that the system doesn't suspend or shut
  // down.
  void NotifyChromeIsExiting();

  // Adjusts policy when the migration of a user homedir to a new
  // encryption format starts or stops. While migration is active,
  // the lid-closed action is overridden to ensure the system
  // doesn't shut down.
  void SetEncryptionMigrationActive(bool active);

  // PowerManagerClient::Observer implementation:
  void PowerManagerRestarted() override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;

 private:
  explicit PowerPolicyController(PowerManagerClient* client);
  ~PowerPolicyController() override;

  // Details about a wake lock added via Add*WakeLock().
  // SCREEN and DIM will keep the screen on and prevent it from locking.
  // SCREEN will also prevent it from dimming. SYSTEM will prevent idle
  // suspends, but the screen will turn off and lock normally.
  struct WakeLock {
    // TODO(derat): Remove this enum in favor of device::mojom::WakeLockType
    // once this class has been moved to the device service:
    // https://crbug.com/702449
    enum Type {
      TYPE_SCREEN,
      TYPE_DIM,
      TYPE_SYSTEM,
    };

    WakeLock(Type type, WakeLockReason reason, const std::string& description);
    ~WakeLock();

    const Type type;
    const WakeLockReason reason;
    const std::string description;
  };

  using WakeLockMap = std::map<int, WakeLock>;

  // Helper method for AddScreenWakeLock() and AddSystemWakeLock().
  int AddWakeLockInternal(WakeLock::Type type,
                          WakeLockReason reason,
                          const std::string& description);

  // Sends a policy based on |prefs_policy_| to the power manager.
  void SendCurrentPolicy();

  PowerManagerClient* client_;  // weak

  // Policy derived from values passed to ApplyPrefs().
  power_manager::PowerManagementPolicy prefs_policy_;

  // Was ApplyPrefs() called?
  bool prefs_were_set_ = false;

  // Maps from an ID representing a request to prevent the screen from
  // getting dimmed or turned off or to prevent the system from suspending
  // to details about the request.
  WakeLockMap wake_locks_;

  // Should |wake_locks_| be honored?
  bool honor_wake_locks_ = true;

  // If wake locks are honored, should TYPE_SCREEN or TYPE_DIM entries in
  // |wake_locks_| be honored?
  // If false, screen wake locks are just treated as TYPE_SYSTEM instead.
  bool honor_screen_wake_locks_ = true;

  // Next ID to be used by an Add*WakeLock() request.
  int next_wake_lock_id_ = 1;

  // True if Chrome is in the process of exiting.
  bool chrome_is_exiting_ = false;

  // True if a user homedir is in the process of migrating encryption formats.
  bool encryption_migration_active_ = false;

  // Whether brightness policy value was overridden by a user adjustment in the
  // current user session.
  bool per_session_brightness_override_ = false;

  DISALLOW_COPY_AND_ASSIGN(PowerPolicyController);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_POLICY_CONTROLLER_H_
