// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_
#define COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// The name of the beacon file, which is relative to the user data directory
// and used to store the CleanExitBeacon value and the variations crash streak.
extern const base::FilePath::CharType kCleanExitBeaconFilename[];

// Captures all possible beacon value permutations for two distinct beacons.
// Exposed for testing.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CleanExitBeaconConsistency {
  kCleanClean = 0,
  kCleanDirty = 1,
  kCleanMissing = 2,
  kDirtyClean = 3,
  kDirtyDirty = 4,
  kDirtyMissing = 5,
  kMissingClean = 6,
  kMissingDirty = 7,
  kMissingMissing = 8,
  kMaxValue = kMissingMissing,
};

// Denotes the state of the beacon file. Exposed for testing.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BeaconFileState {
  kReadable = 0,
  kNotDeserializable = 1,
  kMissingDictionary = 2,
  kMissingCrashStreak = 3,
  kMissingBeacon = 4,
  kMaxValue = kMissingBeacon,
};

// Reads and updates a beacon used to detect whether the previous browser
// process exited cleanly.
class CleanExitBeacon {
 public:
  // Instantiates a CleanExitBeacon whose value is stored in
  // |has_exited_cleanly_|. The value is persisted in the beacon file on
  // platforms that support this mechanism and in Local State on platforms that
  // don't.
  //
  // On Windows, |backup_registry_key| stores a backup of the beacon to verify
  // that the pref's value corresponds to the registry's. |backup_registry_key|
  // is ignored on other platforms, but iOS has a similar verification
  // mechanism embedded inside CleanExitBeacon.
  //
  // |user_data_dir| is the path to the client's user data directory. If empty,
  // the beacon file is not used.
  CleanExitBeacon(const std::wstring& backup_registry_key,
                  const base::FilePath& user_data_dir,
                  PrefService* local_state);

  virtual ~CleanExitBeacon() = default;

  // Not copyable or movable.
  CleanExitBeacon(const CleanExitBeacon&) = delete;
  CleanExitBeacon& operator=(const CleanExitBeacon&) = delete;

  // Initializes the CleanExitBeacon. This includes the following tasks:
  // 1. Determining if the last session exited cleanly,
  // 2. Incrementing the crash streak, if necessary, and
  // 3. Emitting some metrics.
  void Initialize();

  // Returns the original value of the beacon.
  bool exited_cleanly() const { return did_previous_session_exit_cleanly_; }

  // Returns the original value of the last live timestamp.
  base::Time browser_last_live_timestamp() const {
    return initial_browser_last_live_timestamp_;
  }

  // Returns true if Extended Variations Safe Mode is supported on this
  // platform. Android WebView does not support this.
  bool IsExtendedSafeModeSupported() const;

  // Sets the beacon value to |exited_cleanly| and writes the value to disk if
  // the current value (see has_exited_cleanly_) is not already
  // |exited_cleanly|. Note that on platforms that do not support the beacon
  // file, the write is scheduled, so the value may not be persisted if the
  // browser process crashes.
  //
  // Also, updates the last live timestamp.
  //
  // |is_extended_safe_mode| denotes whether Chrome is about to start watching
  // for browser crashes early on in startup as a part of Extended Variations
  // Safe Mode, which is supported by most, but not all, platforms.
  //
  // TODO(crbug.com/40850854): Consider removing |is_extended_safe_mode|.
  void WriteBeaconValue(bool exited_cleanly,
                        bool is_extended_safe_mode = false);

  // Updates the last live timestamp.
  void UpdateLastLiveTimestamp();

  const base::FilePath GetUserDataDirForTesting() const;
  base::FilePath GetBeaconFilePathForTesting() const;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Updates both Local State and NSUserDefaults beacon values.
  static void SetStabilityExitedCleanlyForTesting(PrefService* local_state,
                                                  bool exited_cleanly);

  // Creates and returns a well-formed beacon file contents with the given
  // values.
  static std::string CreateBeaconFileContentsForTesting(bool exited_cleanly,
                                                        int crash_streak);

  // Resets both Local State and NSUserDefaults beacon values.
  static void ResetStabilityExitedCleanlyForTesting(PrefService* local_state);

  // CHECKs that Chrome exited cleanly.
  static void EnsureCleanShutdown(PrefService* local_state);

#if BUILDFLAG(IS_IOS)
  // Sets the NSUserDefaults beacon value.
  static void SetUserDefaultsBeacon(bool exited_cleanly);

  // Checks user default value of kUseUserDefaultsForExitedCleanlyBeacon.
  // Because variations are not initialized early in startup, pair a user
  // defaults value with the variations config.
  static bool ShouldUseUserDefaultsBeacon();

  // Syncs feature kUseUserDefaultsForExitedCleanlyBeacon to NSUserDefaults
  // kUserDefaultsFeatureFlagForExitedCleanlyBeacon.
  static void SyncUseUserDefaultsBeacon();
#endif  // BUILDFLAG(IS_IOS)

  // Prevents a test browser from performing two clean shutdown steps. First, it
  // prevents the beacon value from being updated after this function is called.
  // This prevents the the test browser from signaling that Chrome is shutting
  // down cleanly. Second, it makes EnsureCleanShutdown() a no-op.
  static void SkipCleanShutdownStepsForTesting();

 private:
  // Returns true if the previous session exited cleanly. Either Local State
  // or |beacon_file_contents| is used to get this information. Which is used
  // depends on the client's platform and the existence of a valid beacon file.
  // Also, records several metrics.
  //
  // Should be called only once: at startup.
  bool DidPreviousSessionExitCleanly(base::Value* beacon_file_contents);

  // Returns true if the beacon file is supported on this platform. Android
  // WebView does not support this.
  bool IsBeaconFileSupported() const;

  // Writes |exited_cleanly| and the crash streak to the file located at
  // |beacon_file_path_|.
  void WriteBeaconFile(bool exited_cleanly) const;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
  // Returns whether Chrome exited cleanly in the previous session according to
  // the platform-specific beacon (the registry for Windows or NSUserDefaults
  // for iOS). Returns std::nullopt if the platform-specific location does not
  // have beacon info.
  std::optional<bool> ExitedCleanly();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
  // Returns true if the NSUserDefaults beacon value is set.
  static bool HasUserDefaultsBeacon();

  // Returns the NSUserDefaults beacon value.
  static bool GetUserDefaultsBeacon();

  // Clears the NSUserDefaults beacon value.
  static void ResetUserDefaultsBeacon();
#endif  // BUILDFLAG(IS_IOS)

  // Indicates whether the CleanExitBeacon has been initialized.
  bool initialized_ = false;

  // Stores a backup of the beacon. Windows only.
  const std::wstring backup_registry_key_;

  // Path to the client's user data directory. May be empty.
  const base::FilePath user_data_dir_;

  const raw_ptr<PrefService> local_state_;

  // This is the value of the last live timestamp from local state at the time
  // of construction. It is a timestamp from the previous browser session when
  // the browser was known to be alive.
  const base::Time initial_browser_last_live_timestamp_;

  bool did_previous_session_exit_cleanly_ = false;

  // Denotes the current beacon value for this session, which is updated via
  // CleanExitBeacon::WriteBeaconValue(). When `false`, Chrome is watching for
  // browser crashes. When `true`, Chrome has stopped watching for crashes. When
  // unset, Chrome has neither started nor stopped watching for crashes.
  std::optional<bool> has_exited_cleanly_ = std::nullopt;

  // Where the clean exit beacon and the variations crash streak are stored on
  // platforms that support the beacon file.
  base::FilePath beacon_file_path_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CLEAN_EXIT_BEACON_H_
