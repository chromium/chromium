// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_MSBB_SESSION_DURATIONS_METRICS_RECORDER_H_
#define COMPONENTS_UNIFIED_CONSENT_MSBB_SESSION_DURATIONS_METRICS_RECORDER_H_

#include <memory>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

class PrefService;

namespace unified_consent {

// Tracks the active browsing time that the user spends with URL-keyed
// anonymized data collection, aka "Make Searches and Browsing Better",
// enabled/disabled as a fraction of their total browsing time.
class MsbbSessionDurationsMetricsRecorder
    : public UrlKeyedDataCollectionConsentHelper::Observer {
 public:
  // Callers must ensure that the parameters outlive this object.
  explicit MsbbSessionDurationsMetricsRecorder(PrefService* pref_service);

  MsbbSessionDurationsMetricsRecorder(
      const MsbbSessionDurationsMetricsRecorder&) = delete;
  MsbbSessionDurationsMetricsRecorder& operator=(
      const MsbbSessionDurationsMetricsRecorder&) = delete;

  virtual ~MsbbSessionDurationsMetricsRecorder();

  // Informs this service that a session started at |session_start| time.
  void OnSessionStarted(base::TimeTicks session_start);
  void OnSessionEnded(base::TimeDelta session_length);

  // UrlKeyedDataCollectionConsentHelper::Observer:
  void OnUrlKeyedDataCollectionConsentStateChanged(
      UrlKeyedDataCollectionConsentHelper* consent_helper) override;

 private:
  static void LogMsbbDuration(bool msbb_enabled,
                              base::TimeDelta session_length);

  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper_;

  // The state of the MSBB setting as of the start of the current recording
  // session (see timers below). Used to detect when the state changes.
  bool last_msbb_enabled_ = false;

  // Tracks the elapsed active session time while the browser is open. The timer
  // is absent if there's no active session.
  std::unique_ptr<base::ElapsedTimer> total_session_timer_;
  // Tracks the elapsed active session time in the current MSBB state. Absent
  // if there's no active session.
  std::unique_ptr<base::ElapsedTimer> msbb_state_timer_;
};

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_MSBB_SESSION_DURATIONS_METRICS_RECORDER_H_
