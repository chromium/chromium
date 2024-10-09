// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_switches.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace metrics {

namespace {

using ::variations::prefs::kVariationsCrashStreak;

// Denotes whether Chrome should perform clean shutdown steps: signaling that
// Chrome is exiting cleanly and then CHECKing that is has shutdown cleanly.
// This may be modified by SkipCleanShutdownStepsForTesting().
bool g_skip_clean_shutdown_steps = false;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
// Records the the combined state of two distinct beacons' values in a
// histogram.
void RecordBeaconConsistency(
    std::optional<bool> beacon_file_beacon_value,
    std::optional<bool> platform_specific_beacon_value) {
  CleanExitBeaconConsistency consistency =
      CleanExitBeaconConsistency::kDirtyDirty;

  if (!beacon_file_beacon_value) {
    if (!platform_specific_beacon_value) {
      consistency = CleanExitBeaconConsistency::kMissingMissing;
    } else {
      consistency = platform_specific_beacon_value.value()
                        ? CleanExitBeaconConsistency::kMissingClean
                        : CleanExitBeaconConsistency::kMissingDirty;
    }
  } else if (!platform_specific_beacon_value) {
    consistency = beacon_file_beacon_value.value()
                      ? CleanExitBeaconConsistency::kCleanMissing
                      : CleanExitBeaconConsistency::kDirtyMissing;
  } else if (beacon_file_beacon_value.value()) {
    consistency = platform_specific_beacon_value.value()
                      ? CleanExitBeaconConsistency::kCleanClean
                      : CleanExitBeaconConsistency::kCleanDirty;
  } else {
    consistency = platform_specific_beacon_value.value()
                      ? CleanExitBeaconConsistency::kDirtyClean
                      : CleanExitBeaconConsistency::kDirtyDirty;
  }
  base::UmaHistogramEnumeration("UMA.CleanExitBeaconConsistency3", consistency);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)

// Increments kVariationsCrashStreak if |did_previous_session_exit_cleanly| is
// false. Also, emits the crash streak to a histogram.
//
// If |beacon_file_contents| are given, then the beacon file is used to retrieve
// the crash streak. Otherwise, |local_state| is used.
void MaybeIncrementCrashStreak(bool did_previous_session_exit_cleanly,
                               base::Value* beacon_file_contents,
                               PrefService* local_state) {
  int num_crashes;
  if (beacon_file_contents) {
    std::optional<int> crash_streak =
        beacon_file_contents->GetDict().FindInt(kVariationsCrashStreak);
    // Any contents without the key should have been rejected by
    // MaybeGetFileContents().
    DCHECK(crash_streak);
    num_crashes = crash_streak.value();
  } else {
    // TODO(crbug.com/40850830): Consider not falling back to Local State for
    // clients on platforms that support the beacon file.
    num_crashes = local_state->GetInteger(kVariationsCrashStreak);
  }

  if (!did_previous_session_exit_cleanly) {
    // Increment the crash streak if the previous session crashed. Note that the
    // streak is not cleared if the previous run didn’t crash. Instead, it’s
    // incremented on each crash until Chrome is able to successfully fetch a
    // new seed. This way, a seed update that mostly destabilizes Chrome still
    // results in a fallback to Variations Safe Mode.
    //
    // The crash streak is incremented here rather than in a variations-related
    // class for two reasons. First, the crash streak depends on whether Chrome
    // exited cleanly in the last session, which is first checked via
    // CleanExitBeacon::Initialize(). Second, if the crash streak were updated
    // in another function, any crash between beacon initialization and the
    // other function might cause the crash streak to not be to incremented.
    // "Might" because the updated crash streak also needs to be persisted to
    // disk. A consequence of failing to increment the crash streak is that
    // Chrome might undercount or be completely unaware of repeated crashes
    // early on in startup.
    ++num_crashes;
    // For platforms that use the beacon file, the crash streak is written
    // synchronously to disk later on in startup via
    // MaybeExtendVariationsSafeMode() and WriteBeaconFile(). The crash streak
    // is intentionally not written to the beacon file here. If the beacon file
    // indicates that Chrome failed to exit cleanly, then Chrome got at
    // least as far as MaybeExtendVariationsSafeMode(), which is during the
    // PostEarlyInitialization stage when native code is being synchronously
    // executed. Chrome should also be able to reach that point in this session.
    //
    // For platforms that do not use the beacon file, the crash streak is
    // scheduled to be written to disk later on in startup. At the latest, this
    // is done when a Local State write is scheduled via WriteBeaconFile(). A
    // write is not scheduled here for two reasons.
    //
    // 1. It is an expensive operation.
    // 2. Android WebView (which does not use the beacon file) has its own
    //    Variations Safe Mode mechanism and does not need the crash streak.
    local_state->SetInteger(kVariationsCrashStreak, num_crashes);
  }
  base::UmaHistogramSparse("Variations.SafeMode.Streak.Crashes",
                           std::clamp(num_crashes, 0, 100));
}

// Records |file_state| in a histogram.
void RecordBeaconFileState(BeaconFileState file_state) {
  base::UmaHistogramEnumeration(
      "Variations.ExtendedSafeMode.BeaconFileStateAtStartup", file_state);
}

// Returns the contents of the file at |beacon_file_path| if the following
// conditions are all true. Otherwise, returns nullptr.
//
// 1. The file path is non-empty.
// 2. The file exists.
// 3. The file is successfully read.
// 4. The file contents are in the expected format with the expected info.
//
// The file may not exist for the below reasons:
//
// 1. The file is unsupported on the platform.
// 2. This is the first session after a client updates to or installs a Chrome
//    version that uses the beacon file. The beacon file launched on desktop
//    and iOS in M102 and on Android Chrome in M103.
// 3. Android Chrome clients with only background sessions may never write a
//    beacon file.
// 4. A user may delete the file.
std::unique_ptr<base::Value> MaybeGetFileContents(
    const base::FilePath& beacon_file_path) {
  if (beacon_file_path.empty())
    return nullptr;

  int error_code;
  JSONFileValueDeserializer deserializer(beacon_file_path);
  std::unique_ptr<base::Value> beacon_file_contents =
      deserializer.Deserialize(&error_code, /*error_message=*/nullptr);

  if (!beacon_file_contents) {
    RecordBeaconFileState(BeaconFileState::kNotDeserializable);
    base::UmaHistogramSparse(
        "Variations.ExtendedSafeMode.BeaconFileDeserializationError",
        error_code);
    return nullptr;
  }
  if (!beacon_file_contents->is_dict() ||
      beacon_file_contents->GetDict().empty()) {
    RecordBeaconFileState(BeaconFileState::kMissingDictionary);
    return nullptr;
  }
  const base::Value::Dict& beacon_dict = beacon_file_contents->GetDict();
  if (!beacon_dict.FindInt(kVariationsCrashStreak)) {
    RecordBeaconFileState(BeaconFileState::kMissingCrashStreak);
    return nullptr;
  }
  if (!beacon_dict.FindBool(prefs::kStabilityExitedCleanly)) {
    RecordBeaconFileState(BeaconFileState::kMissingBeacon);
    return nullptr;
  }
  RecordBeaconFileState(BeaconFileState::kReadable);
  return beacon_file_contents;
}

}  // namespace

const base::FilePath::CharType kCleanExitBeaconFilename[] =
    FILE_PATH_LITERAL("Variations");

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

  if (!user_data_dir_.empty()) {
    // Platforms that pass an empty path do so deliberately. They should not
    // use the beacon file.
    beacon_file_path_ = user_data_dir_.Append(kCleanExitBeaconFilename);
  }

  std::unique_ptr<base::Value> beacon_file_contents =
      MaybeGetFileContents(beacon_file_path_);

  did_previous_session_exit_cleanly_ =
      DidPreviousSessionExitCleanly(beacon_file_contents.get());

  MaybeIncrementCrashStreak(did_previous_session_exit_cleanly_,
                            beacon_file_contents.get(), local_state_);
  initialized_ = true;
}

bool CleanExitBeacon::DidPreviousSessionExitCleanly(
    base::Value* beacon_file_contents) {
  if (!IsBeaconFileSupported())
    return local_state_->GetBoolean(prefs::kStabilityExitedCleanly);

  std::optional<bool> beacon_file_beacon_value =
      beacon_file_contents ? beacon_file_contents->GetDict().FindBool(
                                 prefs::kStabilityExitedCleanly)
                           : std::nullopt;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
  std::optional<bool> backup_beacon_value = ExitedCleanly();
  RecordBeaconConsistency(beacon_file_beacon_value, backup_beacon_value);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
  // TODO(crbug.com/40190558): For the time being, this is a no-op; i.e.,
  // ShouldUseUserDefaultsBeacon() always returns false.
  if (ShouldUseUserDefaultsBeacon())
    return backup_beacon_value.value_or(true);
#endif  // BUILDFLAG(IS_IOS)

  return beacon_file_beacon_value.value_or(true);
}

bool CleanExitBeacon::IsExtendedSafeModeSupported() const {
  // All platforms that support the beacon file mechanism also happen to support
  // Extended Variations Safe Mode.
  return IsBeaconFileSupported();
}

void CleanExitBeacon::WriteBeaconValue(bool exited_cleanly,
                                       bool is_extended_safe_mode) {
  DCHECK(initialized_);
  if (g_skip_clean_shutdown_steps)
    return;

  UpdateLastLiveTimestamp();

  if (has_exited_cleanly_ && has_exited_cleanly_.value() == exited_cleanly) {
    // It is possible to call WriteBeaconValue() with the same value for
    // |exited_cleanly| twice during startup and shutdown on some platforms. If
    // the current beacon value matches |exited_cleanly|, then return here to
    // skip redundantly updating Local State, writing a beacon file, and on
    // Windows and iOS, writing to platform-specific locations.
    return;
  }

  if (is_extended_safe_mode) {
    // |is_extended_safe_mode| can be true for only some platforms.
    DCHECK(IsExtendedSafeModeSupported());
    // |has_exited_cleanly_| should always be unset before starting to watch for
    // browser crashes.
    DCHECK(!has_exited_cleanly_);
    // When starting to watch for browser crashes in the code covered by
    // Extended Variations Safe Mode, the only valid value for |exited_cleanly|
    // is `false`. `true` signals that Chrome should stop watching for crashes.
    DCHECK(!exited_cleanly);
    WriteBeaconFile(exited_cleanly);
  } else {
    // TODO(crbug.com/40851383): Stop updating |kStabilityExitedCleanly| on
    // platforms that support the beacon file.
    local_state_->SetBoolean(prefs::kStabilityExitedCleanly, exited_cleanly);
    if (IsBeaconFileSupported()) {
      WriteBeaconFile(exited_cleanly);
    } else {
      // Schedule a Local State write on platforms that back the beacon value
      // using Local State rather than the beacon file.
      local_state_->CommitPendingWrite();
    }
  }

#if BUILDFLAG(IS_WIN)
  base::win::RegKey regkey;
  if (regkey.Create(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                    KEY_ALL_ACCESS) == ERROR_SUCCESS) {
    regkey.WriteValue(base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(),
                      exited_cleanly ? 1u : 0u);
  }
#elif BUILDFLAG(IS_IOS)
  SetUserDefaultsBeacon(exited_cleanly);
#endif  // BUILDFLAG(IS_WIN)

  has_exited_cleanly_ = std::make_optional(exited_cleanly);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
std::optional<bool> CleanExitBeacon::ExitedCleanly() {
#if BUILDFLAG(IS_WIN)
  base::win::RegKey regkey;
  DWORD value = 0u;
  if (regkey.Open(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                  KEY_ALL_ACCESS) == ERROR_SUCCESS &&
      regkey.ReadValueDW(
          base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(), &value) ==
          ERROR_SUCCESS) {
    return value ? true : false;
  }
  return std::nullopt;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_IOS)
  if (HasUserDefaultsBeacon())
    return GetUserDefaultsBeacon();
  return std::nullopt;
#endif  // BUILDFLAG(IS_IOS)
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)

void CleanExitBeacon::UpdateLastLiveTimestamp() {
  local_state_->SetTime(prefs::kStabilityBrowserLastLiveTimeStamp,
                        base::Time::Now());
}

const base::FilePath CleanExitBeacon::GetUserDataDirForTesting() const {
  return user_data_dir_;
}

base::FilePath CleanExitBeacon::GetBeaconFilePathForTesting() const {
  return beacon_file_path_;
}

// static
void CleanExitBeacon::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);

  registry->RegisterTimePref(prefs::kStabilityBrowserLastLiveTimeStamp,
                             base::Time(), PrefRegistry::LOSSY_PREF);

  // This Variations-Safe-Mode-related pref is registered here rather than in
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
#if BUILDFLAG(IS_IOS)
  SetUserDefaultsBeacon(exited_cleanly);
#endif  // BUILDFLAG(IS_IOS)
}

// static
std::string CleanExitBeacon::CreateBeaconFileContentsForTesting(
    bool exited_cleanly,
    int crash_streak) {
  const std::string exited_cleanly_str = exited_cleanly ? "true" : "false";
  return base::StringPrintf(
      "{\n"
      "  \"user_experience_metrics.stability.exited_cleanly\":%s,\n"
      "  \"variations_crash_streak\":%s\n"
      "}",
      exited_cleanly_str.data(), base::NumberToString(crash_streak).data());
}

// static
void CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(
    PrefService* local_state) {
  local_state->ClearPref(prefs::kStabilityExitedCleanly);
#if BUILDFLAG(IS_IOS)
  ResetUserDefaultsBeacon();
#endif  // BUILDFLAG(IS_IOS)
}

// static
void CleanExitBeacon::SkipCleanShutdownStepsForTesting() {
  g_skip_clean_shutdown_steps = true;
}

bool CleanExitBeacon::IsBeaconFileSupported() const {
  return !beacon_file_path_.empty();
}

void CleanExitBeacon::WriteBeaconFile(bool exited_cleanly) const {
  base::Value::Dict dict;
  dict.Set(prefs::kStabilityExitedCleanly, exited_cleanly);
  dict.Set(kVariationsCrashStreak,
           local_state_->GetInteger(kVariationsCrashStreak));

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  bool success = serializer.Serialize(dict);
  DCHECK(success);
  DCHECK(!json_string.empty());
  {
    base::ScopedAllowBlocking allow_io;
    success = base::WriteFile(beacon_file_path_, json_string);
  }
  base::UmaHistogramBoolean("Variations.ExtendedSafeMode.BeaconFileWrite",
                            success);
}

}  // namespace metrics
