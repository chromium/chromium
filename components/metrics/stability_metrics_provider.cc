// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_provider.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "components/metrics/system_session_analyzer/system_session_analyzer_win.h"
#endif

namespace metrics {

namespace {

#if BUILDFLAG(IS_WIN)
// Runs on a blocking thread-pool sequence. SystemSessionAnalyzer reads the
// Windows System event log via EvtQuery/EvtNext, a synchronous ALPC round-trip
// to the Event Log service that can take 100+ ms, so it must not run on the UI
// thread. Log traversal can be expensive, so the analyzer is limited to
// reaching back three previous system sessions to bound the cost.
bool ComputeUncleanSystemSession(base::Time last_live_timestamp) {
  SystemSessionAnalyzer analyzer(3);
  return analyzer.IsSessionUnclean(last_live_timestamp) ==
         SystemSessionAnalyzer::UNCLEAN;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
bool HasGmsCoreVersionChanged(PrefService* local_state) {
  std::string previous_version =
      local_state->GetString(prefs::kStabilityGmsCoreVersion);
  std::string current_version = base::android::device_info::gms_version_code();

  // If the last version is empty, treat it as consistent.
  if (previous_version.empty()) {
    return false;
  }

  return previous_version != current_version;
}

void UpdateGmsCoreVersionPref(PrefService* local_state) {
  std::string current_version = base::android::device_info::gms_version_code();
  local_state->SetString(prefs::kStabilityGmsCoreVersion, current_version);
}
#endif

}  // namespace

StabilityMetricsProvider::StabilityMetricsProvider(PrefService* local_state)
    : local_state_(local_state) {
#if BUILDFLAG(IS_WIN)
  is_unclean_system_session_callback_ =
      base::BindRepeating(&ComputeUncleanSystemSession);
#endif
}

StabilityMetricsProvider::~StabilityMetricsProvider() = default;

// static
void StabilityMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityFileMetricsUnsentFilesCount,
                                0);
  registry->RegisterIntegerPref(prefs::kStabilityFileMetricsUnsentSamplesCount,
                                0);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  registry->RegisterStringPref(prefs::kStabilityGmsCoreVersion, "");
  registry->RegisterIntegerPref(prefs::kStabilityCrashCountDueToGmsCoreUpdate,
                                0);
#endif
#if BUILDFLAG(IS_WIN)
  registry->RegisterIntegerPref(prefs::kStabilitySystemCrashCount, 0);
#endif
}

void StabilityMetricsProvider::Init() {
#if BUILDFLAG(IS_ANDROID)
  // This method has to be called after HasGmsCoreVersionChanged() to avoid
  // overwriting thie result.
  UpdateGmsCoreVersionPref(local_state_);
#endif
}

void StabilityMetricsProvider::ClearSavedStabilityMetrics() {
  // The 0 is a valid value for the below prefs, clears pref instead
  // of setting to default value.
  local_state_->ClearPref(prefs::kStabilityFileMetricsUnsentFilesCount);
  local_state_->ClearPref(prefs::kStabilityFileMetricsUnsentSamplesCount);

#if BUILDFLAG(IS_ANDROID)
  local_state_->SetInteger(prefs::kStabilityLaunchCount, 0);
#endif
#if BUILDFLAG(IS_WIN)
  // Prevent pre-clear replies from restoring discarded metrics.
  weak_factory_.InvalidateWeakPtrs();
  local_state_->SetInteger(prefs::kStabilitySystemCrashCount, 0);
#endif
}

void StabilityMetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile) {
#if BUILDFLAG(IS_ANDROID)
  SystemProfileProto::Stability* stability =
      system_profile->mutable_stability();

  int pref_value = 0;
  if (GetAndClearPrefValue(prefs::kStabilityLaunchCount, &pref_value)) {
    stability->set_launch_count(pref_value);
  }
  if (GetAndClearPrefValue(prefs::kStabilityCrashCountDueToGmsCoreUpdate,
                           &pref_value)) {
    stability->set_crash_count_due_to_gms_core_update(pref_value);
  }
#endif

  if (local_state_->HasPrefPath(prefs::kStabilityFileMetricsUnsentFilesCount)) {
    UMA_STABILITY_HISTOGRAM_COUNTS_100(
        "Stability.Internals.FileMetricsProvider.BrowserMetrics."
        "UnsentFilesCount",
        local_state_->GetInteger(prefs::kStabilityFileMetricsUnsentFilesCount));
    local_state_->ClearPref(prefs::kStabilityFileMetricsUnsentFilesCount);
  }

  if (local_state_->HasPrefPath(
          prefs::kStabilityFileMetricsUnsentSamplesCount)) {
    UMA_STABILITY_HISTOGRAM_CUSTOM_COUNTS(
        "Stability.Internals.FileMetricsProvider.BrowserMetrics."
        "UnsentSamplesCount",
        local_state_->GetInteger(
            prefs::kStabilityFileMetricsUnsentSamplesCount),
        0, 1000000, 50);
    local_state_->ClearPref(prefs::kStabilityFileMetricsUnsentSamplesCount);
  }

#if BUILDFLAG(IS_WIN)
  int pref_value = 0;
  if (GetAndClearPrefValue(prefs::kStabilitySystemCrashCount, &pref_value)) {
    UMA_STABILITY_HISTOGRAM_COUNTS_100("Stability.Internals.SystemCrashCount",
                                       pref_value);
  }
#endif
}

void StabilityMetricsProvider::LogCrash(base::Time last_live_timestamp) {
#if BUILDFLAG(IS_ANDROID)
  // On Android, if there is an update for GMS Core when Chrome is running,
  // Chrome will be killed, counting as a crash. This is expected and should not
  // be counted in stability crash counts. Thus these crashes are added to a
  // specific bucket for crashes caused by GMS Core updates.
  if (HasGmsCoreVersionChanged(local_state_)) {
    IncrementPrefValue(prefs::kStabilityCrashCountDueToGmsCoreUpdate);
    return;
  }
#endif
  StabilityMetricsHelper::RecordStabilityEvent(
      StabilityEventType::kBrowserCrash);

#if BUILDFLAG(IS_WIN)
  MaybeLogSystemCrash(last_live_timestamp);
#endif
}

void StabilityMetricsProvider::LogLaunch() {
#if BUILDFLAG(IS_ANDROID)
  IncrementPrefValue(prefs::kStabilityLaunchCount);
#endif
  StabilityMetricsHelper::RecordStabilityEvent(StabilityEventType::kLaunch);
}

#if BUILDFLAG(IS_WIN)
void StabilityMetricsProvider::SetIsUncleanSystemSessionCallbackForTesting(
    IsUncleanSystemSessionCallback callback) {
  is_unclean_system_session_callback_ = std::move(callback);
}

void StabilityMetricsProvider::MaybeLogSystemCrash(
    base::Time last_live_timestamp) {
  if (last_live_timestamp == base::Time()) {
    return;
  }
  // Determining whether `last_live_timestamp` fell in an unclean Windows system
  // session reads the System event log via SystemSessionAnalyzer, a synchronous
  // operation that can take 100+ ms. MaybeLogSystemCrash() is reached from
  // LogCrash() on the UI thread during MetricsService::InitializeMetricsState()
  // at startup, so run the check on a blocking thread-pool sequence and
  // increment the (persisted) counter back on this sequence. The counter is
  // eventually-consistent across sessions, so incrementing it after this
  // session's first stability log merely defers the bump by at most one
  // session.
  //
  // The task is CONTINUE_ON_SHUTDOWN so that a slow or hung Event Log query
  // (EvtQuery() takes no timeout) never blocks shutdown. If this provider is
  // destroyed before the reply runs, the system crash is simply not counted.
  // The task only uses its own SystemSessionAnalyzer and `last_live_timestamp`,
  // so it is safe to keep running during shutdown.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(is_unclean_system_session_callback_, last_live_timestamp),
      base::BindOnce(&StabilityMetricsProvider::OnSystemSessionChecked,
                     weak_factory_.GetWeakPtr()));
}

void StabilityMetricsProvider::OnSystemSessionChecked(bool unclean) {
  if (unclean) {
    IncrementPrefValue(prefs::kStabilitySystemCrashCount);
  }
}
#endif

void StabilityMetricsProvider::IncrementPrefValue(const char* path) {
  int value = local_state_->GetInteger(path);
  local_state_->SetInteger(path, value + 1);
}

int StabilityMetricsProvider::GetAndClearPrefValue(const char* path,
                                                   int* value) {
  *value = local_state_->GetInteger(path);
  if (*value != 0)
    local_state_->SetInteger(path, 0);
  return *value;
}

}  // namespace metrics
