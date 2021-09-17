// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_safe_mode_constants.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace metrics {
namespace {

using ::variations::kExtendedSafeModeTrial;
using ::variations::kSignalAndWriteSynchronouslyViaPrefServiceGroup;
using ::variations::kSignalAndWriteViaFileUtilGroup;
using ::variations::kWriteSynchronouslyViaPrefServiceGroup;
using ::variations::prefs::kVariationsCrashStreak;

// Denotes whether Chrome should perform clean shutdown steps: signaling that
// Chrome is exiting cleanly and then CHECKing that is has shutdown cleanly.
// This may be modified by SkipCleanShutdownStepsForTesting().
bool g_skip_clean_shutdown_steps = false;

// Increments kVariationsCrashStreak if |did_previous_session_exit_cleanly| is
// false. Also, emits the crash streak to a histogram.
//
// Either |beacon_file_contents| or |local_state| is used to retrieve the crash
// streak depending on the client's Extended Variations Safe Mode experiment
// group in the last session.
void MaybeIncrementCrashStreak(bool did_previous_session_exit_cleanly,
                               base::Value* beacon_file_contents,
                               PrefService* local_state) {
  int num_crashes =
      beacon_file_contents
          ? beacon_file_contents->FindKey(kVariationsCrashStreak)->GetInt()
          : local_state->GetInteger(kVariationsCrashStreak);

  // Increment the crash streak if the previous session crashed. Note that the
  // streak is not cleared if the previous run didn’t crash. Instead, it’s
  // incremented on each crash until Chrome is able to successfully fetch a new
  // seed. This way, a seed update that mostly destabilizes Chrome still results
  // in a fallback to safe mode.
  //
  // The crash streak is incremented here rather than in a variations-related
  // class for two reasons. First, the crash streak depends on the value of
  // kStabilityExitedCleanly. Second, if kVariationsCrashStreak were updated in
  // another function, any crash between CleanExitBeacon() and that function
  // would cause the crash streak to not be to incremented. A consequence of
  // failing to increment the crash streak is that Variations Safe Mode might
  // undercount or be completely unaware of repeated crashes early on in
  // startup.
  if (!did_previous_session_exit_cleanly) {
    ++num_crashes;
    // Schedule only a Local State write. If the client happens to be in an
    // Extended Variations Safe Mode experiment group that introduces new
    // behavior, the crash streak will be written synchronously to disk later on
    // in startup. See MaybeExtendVariationsSafeMode().
    local_state->SetInteger(kVariationsCrashStreak, num_crashes);
    local_state->CommitPendingWrite();
  }
  base::UmaHistogramSparse("Variations.SafeMode.Streak.Crashes",
                           base::clamp(num_crashes, 0, 100));
}

// Returns true if the previous session exited cleanly. Either |local_state| or
// |beacon_file_contents| is used to get this information. Which is used depends
// on the client's Extended Variations Safe Mode experiment group in the
// previous session.
bool DidPreviousSessionExitCleanly(base::Value* beacon_file_contents,
                                   PrefService* local_state) {
  if (beacon_file_contents)
    return beacon_file_contents->FindKey(prefs::kStabilityExitedCleanly)
        ->GetBool();
  return local_state->GetBoolean(prefs::kStabilityExitedCleanly);
}

// Returns the contents of the file at |beacon_file_path| if the following
// conditions are all true. Otherwise, returns nullptr.
//
// 1. The file path is non-empty.
// 2. The file exists.
// 3. The file is successfully read.
// 4. The file contents are in the expected format with the expected info.
//
// The file is not expected to exist for clients that do not belong to the
// kSignalAndWriteViaFileUtilGroup, but even among clients in that group, there
// are some edge cases. MaybeGetFileContents() is called before clients are
// assigned to an Extended Variations Safe Mode experiment group, so a client
// that is later assigned to the kSignalAndWriteViaFileUtilGroup will not have
// the file in the first session after updating. It is also possible for a user
// to delete the file or to reset their variations state with
// kResetVariationState.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
std::unique_ptr<base::Value> MaybeGetFileContents(
    const base::FilePath& beacon_file_path) {
  JSONFileValueDeserializer deserializer(beacon_file_path);
  std::unique_ptr<base::Value> beacon_file_contents = deserializer.Deserialize(
      /*error_code=*/nullptr, /*error_message=*/nullptr);

  bool got_beacon_file_contents =
      beacon_file_contents && beacon_file_contents->is_dict() &&
      beacon_file_contents->FindKeyOfType(kVariationsCrashStreak,
                                          base::Value::Type::INTEGER) &&
      beacon_file_contents->FindKeyOfType(prefs::kStabilityExitedCleanly,
                                          base::Value::Type::BOOLEAN);
  base::UmaHistogramBoolean(
      "Variations.ExtendedSafeMode.GotVariationsFileContents",
      got_beacon_file_contents);

  if (got_beacon_file_contents)
    return beacon_file_contents;
  return nullptr;
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace

CleanExitBeacon::CleanExitBeacon(const std::wstring& backup_registry_key,
                                 const base::FilePath& user_data_dir,
                                 PrefService* local_state)
    : backup_registry_key_(backup_registry_key),
      user_data_dir_(user_data_dir),
      local_state_(local_state),
      initial_browser_last_live_timestamp_(
          local_state->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp)) {
  DCHECK_NE(PrefService::INITIALIZATION_STATUS_WAITING,
            local_state_->GetInitializationStatus());
}

void CleanExitBeacon::Initialize() {
  DCHECK(!initialized_);
  if (!user_data_dir_.empty())
    beacon_file_path_ = user_data_dir_.Append(variations::kVariationsFilename);

#if defined(OS_ANDROID) || defined(OS_IOS)
  // TODO(crbug/1248239): Allow the file to be used once the Extended Variations
  // Safe Mode experiment is enabled on Clank.
  //
  // TODO(crbug/1244334): Enable this on iOS once the fix lands.
  std::unique_ptr<base::Value> beacon_file_contents = nullptr;
#else
  std::unique_ptr<base::Value> beacon_file_contents =
      MaybeGetFileContents(beacon_file_path_);
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  did_previous_session_exit_cleanly_ =
      DidPreviousSessionExitCleanly(beacon_file_contents.get(), local_state_);

#if defined(OS_WIN) || defined(OS_IOS)
  // An enumeration of all possible permutations of the the beacon state in the
  // registry (Windows) or NSUserDefaults (iOS) and in Local State.
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
  CleanExitBeaconConsistency consistency =
      CleanExitBeaconConsistency::kDirtyDirty;

  bool local_state_beacon_is_missing =
      !local_state_->HasPrefPath(prefs::kStabilityExitedCleanly);
  bool local_state_was_last_shutdown_clean = did_previous_session_exit_cleanly_;

  bool backup_beacon_was_last_shutdown_clean = true;
  bool backup_beacon_is_missing = false;
#if defined(OS_WIN)
  base::win::RegKey regkey;
  DWORD value = 0u;
  if (regkey.Open(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                  KEY_ALL_ACCESS) == ERROR_SUCCESS &&
      regkey.ReadValueDW(
          base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(), &value) ==
          ERROR_SUCCESS) {
    backup_beacon_was_last_shutdown_clean = value ? true : false;
  } else {
    backup_beacon_is_missing = true;
  }
#elif defined(OS_IOS)
  if (HasUserDefaultsBeacon()) {
    backup_beacon_was_last_shutdown_clean = GetUserDefaultsBeacon();
  } else {
    backup_beacon_is_missing = true;
  }
#endif  // defined(OS_IOS)

  if (backup_beacon_is_missing) {
    if (local_state_beacon_is_missing) {
      consistency = CleanExitBeaconConsistency::kMissingMissing;
    } else {
      consistency = local_state_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kMissingClean
                        : CleanExitBeaconConsistency::kMissingDirty;
    }
  } else {
    if (local_state_beacon_is_missing) {
      consistency = backup_beacon_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kCleanMissing
                        : CleanExitBeaconConsistency::kDirtyMissing;
    } else if (backup_beacon_was_last_shutdown_clean) {
      consistency = local_state_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kCleanClean
                        : CleanExitBeaconConsistency::kCleanDirty;
    } else {
      consistency = local_state_was_last_shutdown_clean
                        ? CleanExitBeaconConsistency::kDirtyClean
                        : CleanExitBeaconConsistency::kDirtyDirty;
    }
  }
  base::UmaHistogramEnumeration("UMA.CleanExitBeaconConsistency2", consistency);

#if defined(OS_IOS)
  // For the time being, this is a no-op to avoid interference with the Extended
  // Variations Safe Mode experiment; i.e., ShouldUseUserDefaultsBeacon() always
  // returns false.
  if (ShouldUseUserDefaultsBeacon())
    did_previous_session_exit_cleanly_ = backup_beacon_was_last_shutdown_clean;
#endif
#endif  // defined(OS_WIN) || defined(OS_IOS)

  MaybeIncrementCrashStreak(did_previous_session_exit_cleanly_,
                            beacon_file_contents.get(), local_state_);
  initialized_ = true;
}

void CleanExitBeacon::WriteBeaconValue(bool exited_cleanly,
                                       bool write_synchronously,
                                       bool update_beacon) {
  DCHECK(initialized_);
  if (g_skip_clean_shutdown_steps)
    return;

  UpdateLastLiveTimestamp();
  if (update_beacon)
    local_state_->SetBoolean(prefs::kStabilityExitedCleanly, exited_cleanly);

  const std::string group_name =
      base::FieldTrialList::FindFullName(kExtendedSafeModeTrial);

  if (write_synchronously) {
    if (group_name == kWriteSynchronouslyViaPrefServiceGroup ||
        group_name == kSignalAndWriteSynchronouslyViaPrefServiceGroup) {
      SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
          "Variations.ExtendedSafeMode.WritePrefsTime");
      local_state_->CommitPendingWriteSynchronously();
    } else if (group_name == kSignalAndWriteViaFileUtilGroup) {
      SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
          "Variations.ExtendedSafeMode.WritePrefsTime");
      WriteVariationsSafeModeFile(exited_cleanly);
    }
  } else {
    local_state_->CommitPendingWrite();
    if (group_name == kSignalAndWriteViaFileUtilGroup) {
      // Clients in this group also write to the Variations Safe Mode file. This
      // is because the file will be used in the next session, and thus, should
      // be updated whenever kStabilityExitedCleanly is.
      WriteVariationsSafeModeFile(exited_cleanly);
    }
  }

#if defined(OS_WIN)
  base::win::RegKey regkey;
  if (regkey.Create(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                    KEY_ALL_ACCESS) == ERROR_SUCCESS) {
    regkey.WriteValue(base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(),
                      exited_cleanly ? 1u : 0u);
  }
#elif defined(OS_IOS)
  SetUserDefaultsBeacon(exited_cleanly);
#endif  // defined(OS_WIN)
}

void CleanExitBeacon::UpdateLastLiveTimestamp() {
  local_state_->SetTime(prefs::kStabilityBrowserLastLiveTimeStamp,
                        base::Time::Now());
}

// static
void CleanExitBeacon::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);

  registry->RegisterTimePref(prefs::kStabilityBrowserLastLiveTimeStamp,
                             base::Time(), PrefRegistry::LOSSY_PREF);

  // This variations-safe-mode-related pref is registered here rather than in
  // SafeSeedManager::RegisterPrefs() because the CleanExitBeacon is
  // responsible for incrementing this value. (See the comments in
  // MaybeIncrementCrashStreak() for more details.)
  registry->RegisterIntegerPref(kVariationsCrashStreak, 0);
}

// static
void CleanExitBeacon::EnsureCleanShutdown(PrefService* local_state) {
  if (!g_skip_clean_shutdown_steps)
    CHECK(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
}

// static
void CleanExitBeacon::SetStabilityExitedCleanlyForTesting(
    PrefService* local_state,
    bool exited_cleanly) {
  local_state->SetBoolean(prefs::kStabilityExitedCleanly, exited_cleanly);
#if defined(OS_IOS)
  SetUserDefaultsBeacon(exited_cleanly);
#endif  // defined(OS_IOS)
}

// static
void CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(
    PrefService* local_state) {
  local_state->ClearPref(prefs::kStabilityExitedCleanly);
#if defined(OS_IOS)
  ResetUserDefaultsBeacon();
#endif  // defined(OS_IOS)
}

// static
void CleanExitBeacon::SkipCleanShutdownStepsForTesting() {
  g_skip_clean_shutdown_steps = true;
}

void CleanExitBeacon::WriteVariationsSafeModeFile(bool exited_cleanly) const {
  DCHECK_EQ(base::FieldTrialList::FindFullName(kExtendedSafeModeTrial),
            kSignalAndWriteViaFileUtilGroup);
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey(prefs::kStabilityExitedCleanly, exited_cleanly);
  dict.SetIntKey(kVariationsCrashStreak,
                 local_state_->GetInteger(kVariationsCrashStreak));
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  bool success = serializer.Serialize(dict);
  DCHECK(success);
  int data_size = static_cast<int>(json_string.size());
  DCHECK_NE(data_size, 0);
  {
    base::ScopedAllowBlocking allow_io;
    base::WriteFile(beacon_file_path_, json_string.data(), data_size);
  }
}

}  // namespace metrics
