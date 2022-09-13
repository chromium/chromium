// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_STATE_MANAGER_H_
#define COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_STATE_MANAGER_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

namespace android_webview {
class AwTracingDelegateTest;
}

namespace content {
class BackgroundTracingConfig;
}

namespace tracing {

class BackgroundTracingStateManagerTest;

// Do not remove or change the order of enum fields since it is stored in
// preferences.
enum class BackgroundTracingState : int {
  // Default state when tracing is not started in previous session, or when
  // state is not found or invalid.
  NOT_ACTIVATED = 0,
  STARTED = 1,
  RAN_30_SECONDS = 2,
  FINALIZATION_STARTED = 3,
  LAST = FINALIZATION_STARTED,
};

// Manages local state prefs for background tracing, and tracks state from
// previous background tracing session(s). All the calls are expected to run on
// UI thread.
class COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS) BackgroundTracingStateManager {
 public:
  using ScenarioUploadTimestampMap = base::flat_map<std::string, base::Time>;

  static BackgroundTracingStateManager& GetInstance();

  // Initializes state from previous session and writes current state to
  // prefs, when called the first time. NOOP on any calls after that. It also
  // deletes any expired entries from prefs.
  void Initialize(PrefService* local_state);

  // Used in tests when other methods of this class need to be called before
  // Initialize().
  void SetPrefServiceForTesting(PrefService* local_state);

  // True if last session potentially crashed and it is unsafe to turn on
  // background tracing in current session.
  bool DidLastSessionEndUnexpectedly() const;

  // True if the embedder uploaded a trace for the given |config| recently, and
  // uploads should be throttled for the |config|.
  bool DidRecentlyUploadForScenario(
      const content::BackgroundTracingConfig& config) const;

  // The embedder should call this method every time background tracing starts
  // so that the state in prefs is updated. Posts a timer task to the current
  // sequence to update the state once more to denote no crashes after a
  // reasonable time (see DidLastSessionEndUnexpectedly()).
  void NotifyTracingStarted();

  // The embedder should call this method every time background tracing finishes
  // so that the state in prefs is updated.
  void NotifyFinalizationStarted();

  // Updates the state to include the upload time for |scenario_name|, and
  // saves it to prefs.
  void OnScenarioUploaded(const std::string& scenario_name);

  // Saves the given state to prefs, public for testing.
  void SaveState(const ScenarioUploadTimestampMap& upload_times,
                 BackgroundTracingState state);

 private:
  friend base::NoDestructor<BackgroundTracingStateManager>;
  friend class tracing::BackgroundTracingStateManagerTest;
  friend class android_webview::AwTracingDelegateTest;

  BackgroundTracingStateManager();
  ~BackgroundTracingStateManager();

  void SaveState();

  // Updates the current tracing state and saves it to prefs.
  void SetState(BackgroundTracingState new_state);

  // Used in tests to reset the state since a singleton instance is never
  // destroyed.
  void Reset();

  BackgroundTracingState state_ = BackgroundTracingState::NOT_ACTIVATED;

  bool initialized_ = false;

  raw_ptr<PrefService> local_state_ = nullptr;

  // Following are valid only when |initialized_| = true.
  BackgroundTracingState last_session_end_state_ =
      BackgroundTracingState::NOT_ACTIVATED;

  ScenarioUploadTimestampMap scenario_last_upload_timestamp_;
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_STATE_MANAGER_H_
