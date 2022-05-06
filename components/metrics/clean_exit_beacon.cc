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
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/variations_switches.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace metrics {
namespace {

using ::variations::kControlGroup;
using ::variations::kDefaultGroup;
using ::variations::kEnabledGroup;
using ::variations::kExtendedSafeModeTrial;
using ::variations::prefs::kVariationsCrashStreak;

const char kMonitoringStageKey[] = "monitoring_stage";

// Denotes whether Chrome should perform clean shutdown steps: signaling that
// Chrome is exiting cleanly and then CHECKing that is has shutdown cleanly.
// This may be modified by SkipCleanShutdownStepsForTesting().
bool g_skip_clean_shutdown_steps = false;

// Records the monitoring stage in which a previous session failed to exit
// cleanly.
void RecordMonitoringStage(base::Value* beacon_file_contents) {
  BeaconMonitoringStage stage;
  if (beacon_file_contents) {
    base::Value* beacon_file_stage = beacon_file_contents->FindKeyOfType(
        kMonitoringStageKey, base::Value::Type::INTEGER);
    if (beacon_file_stage) {
      stage = static_cast<BeaconMonitoringStage>(beacon_file_stage->GetInt());
    } else {
      // The beacon file of Extended Variations Safe Mode experiment group
      // clients may not include the monitoring stage as this info was not added
      // until M100.
      stage = BeaconMonitoringStage::kMissing;
    }
  } else {
    DCHECK_NE(base::FieldTrialList::FindFullName(kExtendedSafeModeTrial),
              kEnabledGroup);
    // Clients that are not in the experiment group always emit kStatusQuo.
    stage = BeaconMonitoringStage::kStatusQuo;
  }
  // The metric should not be emitted when Chrome exited cleanly, i.e. when
  // Chrome was not monitoring for crashes.
  DCHECK_NE(stage, BeaconMonitoringStage::kNotMonitoring);
  UMA_STABILITY_HISTOGRAM_ENUMERATION("UMA.CleanExitBeacon.MonitoringStage",
                                      stage);
}

// Records the the combined state of two distinct beacons' values in the given
// histogram.
void RecordBeaconConsistency(const std::string& histogram_name,
                             absl::optional<bool> beacon_value1,
                             absl::optional<bool> beacon_value2) {
  CleanExitBeaconConsistency consistency =
      CleanExitBeaconConsistency::kDirtyDirty;

  if (!beacon_value1) {
    if (!beacon_value2) {
      consistency = CleanExitBeaconConsistency::kMissingMissing;
    } else {
      consistency = beacon_value2.value()
                        ? CleanExitBeaconConsistency::kMissingClean
                        : CleanExitBeaconConsistency::kMissingDirty;
    }
  } else if (!beacon_value2) {
    consistency = beacon_value1.value()
                      ? CleanExitBeaconConsistency::kCleanMissing
                      : CleanExitBeaconConsistency::kDirtyMissing;
  } else if (beacon_value1.value()) {
    consistency = beacon_value2.value()
                      ? CleanExitBeaconConsistency::kCleanClean
                      : CleanExitBeaconConsistency::kCleanDirty;
  } else {
    consistency = beacon_value2.value()
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
// The file is not expected to exist for clients that have never been in the
// Extended Variations Safe Mode experiment group, kEnabledGroup. The file may
// not exist for all experiment group clients because there are some are some
// edge cases. First, MaybeGetFileContents() is called before clients are
// assigned to an Extended Variations Safe Mode group, so a client that is later
// assigned to the experiment group will not have the file in the first session
// after updating to or installing a Chrome version with the experiment. Second,
// Android Chrome experiment group clients with repeated background sessions may
// never write a beacon file. Finally, it is possible for a user to delete the
// file or to reset their variations state with kResetVariationState.
//
// Note that not all beacon files are expected to have a monitoring stage as
// this info was added in M100.
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
  if (!beacon_file_contents->is_dict() || beacon_file_contents->DictEmpty()) {
    RecordBeaconFileState(BeaconFileState::kMissingDictionary);
    return nullptr;
  }
  if (!beacon_file_contents->FindKeyOfType(kVariationsCrashStreak,
                                           base::Value::Type::INTEGER)) {
    RecordBeaconFileState(BeaconFileState::kMissingCrashStreak);
    return nullptr;
  }
  if (!beacon_file_contents->FindKeyOfType(prefs::kStabilityExitedCleanly,
                                           base::Value::Type::BOOLEAN)) {
    RecordBeaconFileState(BeaconFileState::kMissingBeacon);
    return nullptr;
  }
  RecordBeaconFileState(BeaconFileState::kReadable);
  return beacon_file_contents;
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

// Sets up the Extended Variations Safe Mode experiment, whose groups have
// channel-specific weights. Returns the name of the client's experiment group
// name, e.g. "Control".
std::string SetUpExtendedSafeModeTrial(version_info::Channel channel) {
  int default_group;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kExtendedSafeModeTrial, 100, kDefaultGroup,
          base::FieldTrial::ONE_TIME_RANDOMIZED, &default_group));

#if BUILDFLAG(IS_ANDROID)
  int group_probability = channel == version_info::Channel::STABLE ? 1 : 50;
  trial->AppendGroup(kControlGroup, group_probability);
  trial->AppendGroup(kEnabledGroup, group_probability);
#else
  // The new behavior is launched on desktop and iOS.
  trial->AppendGroup(kEnabledGroup, 100);
#endif
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

  if (group == kEnabledGroup)
    beacon_file_path_ = user_data_dir_.Append(variations::kVariationsFilename);

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
  absl::optional<bool> local_state_beacon_value;
  if (local_state_->HasPrefPath(prefs::kStabilityExitedCleanly)) {
    local_state_beacon_value = absl::make_optional(
        local_state_->GetBoolean(prefs::kStabilityExitedCleanly));
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
  absl::optional<bool> backup_beacon_value = ExitedCleanly();
  RecordBeaconConsistency("UMA.CleanExitBeaconConsistency2",
                          backup_beacon_value, local_state_beacon_value);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)

  absl::optional<bool> beacon_file_beacon_value;
  bool use_beacon_file = base::FieldTrialList::FindFullName(
                             kExtendedSafeModeTrial) == kEnabledGroup;
  if (use_beacon_file) {
    if (beacon_file_contents) {
      beacon_file_beacon_value = absl::make_optional(
          beacon_file_contents->FindKey(prefs::kStabilityExitedCleanly)
              ->GetBool());
    }
    RecordBeaconConsistency("UMA.CleanExitBeacon.BeaconFileConsistency",
                            beacon_file_beacon_value, local_state_beacon_value);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
    RecordBeaconConsistency("UMA.CleanExitBeaconConsistency3",
                            beacon_file_beacon_value, backup_beacon_value);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
  }

  bool did_previous_session_exit_cleanly =
      use_beacon_file ? beacon_file_beacon_value.value_or(true)
                      : local_state_beacon_value.value_or(true);
  if (!did_previous_session_exit_cleanly)
    RecordMonitoringStage(use_beacon_file ? beacon_file_contents : nullptr);

#if BUILDFLAG(IS_IOS)
  // For the time being, this is a no-op to avoid interference with the Extended
  // Variations Safe Mode experiment; i.e., ShouldUseUserDefaultsBeacon() always
  // returns false.
  if (ShouldUseUserDefaultsBeacon())
    return backup_beacon_value.value_or(true);
#endif  // BUILDFLAG(IS_IOS)
  return did_previous_session_exit_cleanly;
}

void CleanExitBeacon::WriteBeaconValue(bool exited_cleanly,
                                       bool is_extended_safe_mode) {
  DCHECK(initialized_);
  if (g_skip_clean_shutdown_steps)
    return;

  UpdateLastLiveTimestamp();

  const std::string group_name =
      base::FieldTrialList::FindFullName(kExtendedSafeModeTrial);

  if (is_extended_safe_mode) {
    DCHECK_EQ(group_name, kEnabledGroup);
    DCHECK(!exited_cleanly);

#if BUILDFLAG(IS_ANDROID)
    extended_monitoring_stage_start_time_ = base::TimeTicks::Now();
#endif

    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Variations.ExtendedSafeMode.WritePrefsTime");
    // The beacon value is written to disk synchronously twice during
    // startup for clients in the Extended Variations Safe Mode experiment
    // group. The first time is via
    // VariationsFieldTrialCreator::MaybeExtendVariationsSafeMode(). This is
    // when Chrome begins monitoring for crashes, i.e. |exited_cleanly| is
    // false. This is the only point at which (a) the WritePrefsTime metric is
    // emitted and (b) the kExtended monitoring stage is written.
    //
    // Later on in startup, such clients call CleanExitBeacon::WriteBeaconFile()
    // again with |exited_cleanly| and |is_extended_safe_mode| set to false via
    // MetricsService::LogNeedForCleanShutdown() for desktop and
    // MetricsService::OnAppEnterForeground() for mobile, which is the status
    // quo point at which Chrome monitors for crashes. At this point, a
    // different monitoring stage is written to the beacon file.
    //
    // For Android, note that Chrome does not monitor for crashes in background
    // sessions. See VariationsFieldTrialCreator::SetUpFieldTrials() and
    // MetricsService::InitializeMetricsState().
    WriteBeaconFile(exited_cleanly, BeaconMonitoringStage::kExtended);
  } else {
    local_state_->SetBoolean(prefs::kStabilityExitedCleanly, exited_cleanly);
    local_state_->CommitPendingWrite();  // Schedule a write.
#if BUILDFLAG(IS_ANDROID)
    if (!extended_monitoring_stage_start_time_.is_null()) {
      // The time exists, so this is the transition from the extended browser
      // crash monitoring stage to the status quo stage. Only Extended
      // Variations Safe Mode enabled-group clients have the extended monitoring
      // stage.
      // TODO(crbug/1321989): Clean up this metric and
      // |extended_monitoring_stage_start_time_| once Android Chrome
      // stakeholders have enough data on the duration.
      base::UmaHistogramLongTimes(
          "UMA.CleanExitBeacon.ExtendedMonitoringStageDuration",
          base::TimeTicks::Now() - extended_monitoring_stage_start_time_);
      extended_monitoring_stage_start_time_ = base::TimeTicks();  // Null time.
    }
#endif  // BUILDFLAG(IS_ANDROID)
    if (group_name == kEnabledGroup) {
      // Clients in this group write to the Variations Safe Mode file whenever
      // |kStabilityExitedCleanly| is updated. The file is kept in sync with the
      // pref because the file is used in the next session.
      //
      // If |exited_cleanly| is true, then Chrome is not monitoring for crashes,
      // so the kNotMonitoringStage is used. Otherwise, kStatusQuo is written
      // because startup has reached the point at which the status quo
      // Variations-Safe-Mode-related code begins watching for crashes. See the
      // comment in the above if block for more details.
      WriteBeaconFile(exited_cleanly,
                      exited_cleanly ? BeaconMonitoringStage::kNotMonitoring
                                     : BeaconMonitoringStage::kStatusQuo);
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
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
absl::optional<bool> CleanExitBeacon::ExitedCleanly() {
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
  return absl::nullopt;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_IOS)
  if (HasUserDefaultsBeacon())
    return GetUserDefaultsBeacon();
  return absl::nullopt;
#endif  // BUILDFLAG(IS_IOS)
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)

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
#if BUILDFLAG(IS_IOS)
  SetUserDefaultsBeacon(exited_cleanly);
#endif  // BUILDFLAG(IS_IOS)
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

void CleanExitBeacon::WriteBeaconFile(
    bool exited_cleanly,
    BeaconMonitoringStage monitoring_stage) const {
  DCHECK_EQ(base::FieldTrialList::FindFullName(kExtendedSafeModeTrial),
            kEnabledGroup);
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey(prefs::kStabilityExitedCleanly, exited_cleanly);
  dict.SetIntKey(kVariationsCrashStreak,
                 local_state_->GetInteger(kVariationsCrashStreak));
  dict.SetIntKey(kMonitoringStageKey, static_cast<int>(monitoring_stage));

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  bool success = serializer.Serialize(dict);
  DCHECK(success);
  int data_size = static_cast<int>(json_string.size());
  DCHECK_NE(data_size, 0);
  int bytes_written;
  {
    base::ScopedAllowBlocking allow_io;
    // WriteFile() returns -1 on error.
    bytes_written =
        base::WriteFile(beacon_file_path_, json_string.data(), data_size);
  }
  base::UmaHistogramBoolean("Variations.ExtendedSafeMode.BeaconFileWrite",
                            bytes_written != -1);
}

}  // namespace metrics
