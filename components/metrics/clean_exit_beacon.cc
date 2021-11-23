// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/variations_switches.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace metrics {
namespace {

using ::variations::kControlGroup;
using ::variations::kDefaultGroup;
using ::variations::kExtendedSafeModeTrial;
using ::variations::kSignalAndWriteViaFileUtilGroup;
using ::variations::prefs::kVariationsCrashStreak;

// Denotes whether Chrome should perform clean shutdown steps: signaling that
// Chrome is exiting cleanly and then CHECKing that is has shutdown cleanly.
// This may be modified by SkipCleanShutdownStepsForTesting().
bool g_skip_clean_shutdown_steps = false;

// Records the the combined state of two distinct beacons' values in the given
// histogram. One beacon is stored in Local State while the other is stored
// elsewhere (e.g. in platform-specific storage, like the Windows registry, or
// in the beacon file).
void RecordBeaconConsistency(const std::string& histogram_name,
                             absl::optional<bool> other_beacon_value,
                             absl::optional<bool> local_state_beacon_value) {
  CleanExitBeaconConsistency consistency =
      CleanExitBeaconConsistency::kDirtyDirty;

  if (!other_beacon_value) {  // The non-Local-State-backed beacon is missing.
    if (!local_state_beacon_value) {  // The Local State beacon is missing.
      consistency = CleanExitBeaconConsistency::kMissingMissing;
    } else {
      consistency = local_state_beacon_value.value()
                        ? CleanExitBeaconConsistency::kMissingClean
                        : CleanExitBeaconConsistency::kMissingDirty;
    }
  } else if (!local_state_beacon_value) {
    consistency = other_beacon_value.value()
                      ? CleanExitBeaconConsistency::kCleanMissing
                      : CleanExitBeaconConsistency::kDirtyMissing;
  } else if (other_beacon_value.value()) {
    consistency = local_state_beacon_value.value()
                      ? CleanExitBeaconConsistency::kCleanClean
                      : CleanExitBeaconConsistency::kCleanDirty;
  } else {
    consistency = local_state_beacon_value.value()
                      ? CleanExitBeaconConsistency::kDirtyClean
                      : CleanExitBeaconConsistency::kDirtyDirty;
  }
  base::UmaHistogramEnumeration(histogram_name, consistency);
}

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
std::unique_ptr<base::Value> MaybeGetFileContents(
    const base::FilePath& beacon_file_path) {
  if (beacon_file_path.empty())
    return nullptr;

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

// Returns the channel to use for setting up the Extended Variations Safe Mode
// experiment.
//
// This is needed for tests in which there is a mismatch between (a) the channel
// on which the bot is running (and thus the channel plumbed through to the
// CleanExitBeacon's ctor) and (b) the channel that we wish to use for running a
// particular test. This mismatch can cause failures (crbug/1259550) when (a)
// the channel on which the bot is running is a channel to which the Extended
// Variations Safe Mode experiment does not apply and (b) a test uses a channel
// on which the experiment does apply.
//
// TODO(crbug/1241702): Clean up this function once the experiment is over.
version_info::Channel GetChannel(version_info::Channel channel) {
  const std::string forced_channel =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          variations::switches::kFakeVariationsChannel);

  if (!forced_channel.empty()) {
    if (forced_channel == "stable")
      return version_info::Channel::STABLE;
    if (forced_channel == "beta")
      return version_info::Channel::BETA;
    if (forced_channel == "dev")
      return version_info::Channel::DEV;
    if (forced_channel == "canary")
      return version_info::Channel::CANARY;
    DVLOG(1) << "Invalid channel provided: " << forced_channel;
  }
  return channel;
}

// Sets up the Extended Variations Safe Mode experiment, which is enabled on
// only some channels. If assigned to an experiment group, returns the name of
// the group name, e.g. "Control"; otherwise, returns the empty string.
std::string SetUpExtendedSafeModeTrial(version_info::Channel channel) {
  if (channel != version_info::Channel::UNKNOWN &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::BETA) {
    return std::string();
  }

  int default_group;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kExtendedSafeModeTrial, 100, kDefaultGroup,
          base::FieldTrial::ONE_TIME_RANDOMIZED, &default_group));

  trial->AppendGroup(kControlGroup, 50);
  trial->AppendGroup(kSignalAndWriteViaFileUtilGroup, 50);
  return trial->group_name();
}

}  // namespace

CleanExitBeacon::CleanExitBeacon(const std::wstring& backup_registry_key,
                                 const base::FilePath& user_data_dir,
                                 PrefService* local_state,
                                 version_info::Channel channel)
    : backup_registry_key_(backup_registry_key),
      user_data_dir_(user_data_dir),
      local_state_(local_state),
      initial_browser_last_live_timestamp_(
          local_state->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp)),
      channel_(GetChannel(channel)) {
  DCHECK_NE(PrefService::INITIALIZATION_STATUS_WAITING,
            local_state_->GetInitializationStatus());
}

void CleanExitBeacon::Initialize() {
  DCHECK(!initialized_);

  std::string group;
  if (!user_data_dir_.empty()) {
    // Platforms that pass an empty path do so deliberately. They should not
    // participate in this experiment.
    group = SetUpExtendedSafeModeTrial(channel_);
  }

  if (group == kSignalAndWriteViaFileUtilGroup)
    beacon_file_path_ = user_data_dir_.Append(variations::kVariationsFilename);

  std::unique_ptr<base::Value> beacon_file_contents =
      MaybeGetFileContents(beacon_file_path_);

  did_previous_session_exit_cleanly_ =
      DidPreviousSessionExitCleanly(beacon_file_contents.get());

#if defined(OS_ANDROID)
  // TODO(crbug/1248239): Use the beacon file, if any, to determine the crash
  // crash once the Extended Variations Safe Mode experiment is fully enabled
  // on Android Chrome.
  beacon_file_contents.reset();
#endif  // defined(OS_ANDROID)

  MaybeIncrementCrashStreak(did_previous_session_exit_cleanly_,
                            beacon_file_contents.get(), local_state_);
  initialized_ = true;
}

bool CleanExitBeacon::DidPreviousSessionExitCleanly(
    base::Value* beacon_file_contents) {
  absl::optional<bool> local_state_beacon_value;
  if (local_state_->HasPrefPath(prefs::kStabilityExitedCleanly)) {
    local_state_beacon_value = absl::make_optional(
        local_state_->GetBoolean(prefs::kStabilityExitedCleanly));
  }

#if defined(OS_WIN) || defined(OS_IOS)
  absl::optional<bool> backup_beacon_value = ExitedCleanly();
  RecordBeaconConsistency("UMA.CleanExitBeaconConsistency2",
                          backup_beacon_value, local_state_beacon_value);
#endif  // defined(OS_WIN) || defined(OS_IOS)

  absl::optional<bool> beacon_file_beacon_value;
  bool use_beacon_file =
      base::FieldTrialList::FindFullName(kExtendedSafeModeTrial) ==
      kSignalAndWriteViaFileUtilGroup;
  if (use_beacon_file) {
    if (beacon_file_contents) {
      beacon_file_beacon_value = absl::make_optional(
          beacon_file_contents->FindKey(prefs::kStabilityExitedCleanly)
              ->GetBool());
    }
    RecordBeaconConsistency("UMA.CleanExitBeacon.BeaconFileConsistency",
                            beacon_file_beacon_value, local_state_beacon_value);
  }

#if defined(OS_ANDROID)
  // TODO(crbug/1248239): Fully enable the Extended Variations Safe Mode
  // experiment on Android Chrome by using the beacon file's beacon value for
  // clients in the SignalAndWriteViaFileUtil group.
  return local_state_beacon_value.value_or(true);
#else
#if defined(OS_IOS)
  // For the time being, this is a no-op to avoid interference with the Extended
  // Variations Safe Mode experiment; i.e., ShouldUseUserDefaultsBeacon() always
  // returns false.
  if (ShouldUseUserDefaultsBeacon())
    return backup_beacon_value.value_or(true);
#endif  // defined(OS_IOS)

  return use_beacon_file ? beacon_file_beacon_value.value_or(true)
                         : local_state_beacon_value.value_or(true);
#endif  // defined(OS_ANDROID)
}

void CleanExitBeacon::WriteBeaconValue(bool exited_cleanly,
                                       bool write_synchronously) {
  DCHECK(initialized_);
  if (g_skip_clean_shutdown_steps)
    return;

  UpdateLastLiveTimestamp();

  const std::string group_name =
      base::FieldTrialList::FindFullName(kExtendedSafeModeTrial);

  if (write_synchronously) {
    DCHECK_EQ(group_name, kSignalAndWriteViaFileUtilGroup);
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Variations.ExtendedSafeMode.WritePrefsTime");
    WriteBeaconFile(exited_cleanly);
  } else {
    local_state_->SetBoolean(prefs::kStabilityExitedCleanly, exited_cleanly);
    local_state_->CommitPendingWrite();  // Schedule a write.
    if (group_name == kSignalAndWriteViaFileUtilGroup) {
      // Clients in this group write to the Variations Safe Mode file whenever
      // |kStabilityExitedCleanly| is updated. The file is kept in sync with the
      // pref because the file is used in the next session.
      WriteBeaconFile(exited_cleanly);
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

#if defined(OS_WIN) || defined(OS_IOS)
absl::optional<bool> CleanExitBeacon::ExitedCleanly() {
#if defined(OS_WIN)
  base::win::RegKey regkey;
  DWORD value = 0u;
  if (regkey.Open(HKEY_CURRENT_USER, backup_registry_key_.c_str(),
                  KEY_ALL_ACCESS) == ERROR_SUCCESS &&
      regkey.ReadValueDW(
          base::ASCIIToWide(prefs::kStabilityExitedCleanly).c_str(), &value) ==
          ERROR_SUCCESS) {
    return value ? true : false;
  }
  return absl::nullopt;
#endif  // defined(OS_WIN)
#if defined(OS_IOS)
  if (HasUserDefaultsBeacon())
    return GetUserDefaultsBeacon();
  return absl::nullopt;
#endif  // defined(OS_IOS)
}
#endif  // #if defined(OS_WIN) || defined(OS_IOS)

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

void CleanExitBeacon::WriteBeaconFile(bool exited_cleanly) const {
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
