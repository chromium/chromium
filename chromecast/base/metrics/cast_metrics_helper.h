// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_
#define CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"

namespace base {
class SequencedTaskRunner;
class TickClock;
}  // namespace base

namespace chromecast {
namespace metrics {

// Helper class for tracking complex metrics. This particularly includes
// playback metrics that span events across time, such as "time from app launch
// to video being rendered."
// Currently, browser startup code should instantiate this once; it can be
// accessed thereafter through GetInstance. It's not deleted since it may be
// called during teardown of other global objects.
// TODO(halliwell): convert to mojo service, eliminate singleton pattern.
class CastMetricsHelper {
 public:
  enum BufferingType {
    kInitialBuffering,
    kBufferingAfterUnderrun,
    kAbortedBuffering,
  };

  using RecordActionCallback =
      base::RepeatingCallback<void(const std::string&)>;

  class MetricsSink {
   public:
    virtual ~MetricsSink() = default;

    virtual void OnAction(const std::string& action) = 0;
    virtual void OnEnumerationEvent(const std::string& name,
                                    int value, int num_buckets) = 0;
    virtual void OnTimeEvent(const std::string& name,
                             base::TimeDelta value,
                             base::TimeDelta min,
                             base::TimeDelta max,
                             int num_buckets) = 0;
  };

  // Decodes action_name/app_id/session_id/sdk_version from metrics name.
  // Return false if the metrics name is not generated from
  // EncodeAppInfoIntoMetricsName() with correct format.
  static bool DecodeAppInfoFromMetricsName(
      const std::string& metrics_name,
      std::string* action_name,
      std::string* app_id,
      std::string* session_id,
      std::string* sdk_version);

  static CastMetricsHelper* GetInstance();

  // This records the startup time of an app load (note: another app
  // may be running and still collecting metrics).
  virtual void DidStartLoad(const std::string& app_id);
  // This function marks the completion of a successful app load. It switches
  // metric collection to this app.
  virtual void DidCompleteLoad(const std::string& app_id,
                               const std::string& session_id);
  // This function updates the sdk version of the current active application
  virtual void UpdateSDKInfo(const std::string& sdk_version);

  // Logs UMA record for media play/pause user actions.
  virtual void LogMediaPlay();
  virtual void LogMediaPause();

  // Logs a simple UMA user action.
  // This is used as an in-place replacement of content::RecordComputedAction().
  virtual void RecordSimpleAction(const std::string& action);

  // Logs a generic event.
  virtual void RecordEventWithValue(const std::string& action, int value);

  // Logs application specific events.
  virtual void RecordApplicationEvent(const std::string& event);
  virtual void RecordApplicationEvent(const std::string& app_id,
                                      const std::string& session_id,
                                      const std::string& sdk_version,
                                      const std::string& event);
  virtual void RecordApplicationEventWithValue(const std::string& event,
                                               int value);

  // Logs UMA record of the time the app made its first paint.
  virtual void LogTimeToFirstPaint();

  // Logs UMA record of the time the app pushed its first audio frame.
  virtual void LogTimeToFirstAudio();

  // Logs UMA record of the time needed to re-buffer A/V.
  virtual void LogTimeToBufferAv(BufferingType buffering_type,
                                 base::TimeDelta time);

  // Returns metrics name with app name between prefix and suffix.
  virtual std::string GetMetricsNameWithAppName(
      const std::string& prefix,
      const std::string& suffix) const;

  // Provides a MetricsSink instance to delegate UMA event logging.
  // Once the delegate interface is set, CastMetricsHelper will not log UMA
  // events internally unless SetMetricsSink(NULL) is called.
  // CastMetricsHelper can only hold one MetricsSink instance.
  // Caller retains ownership of MetricsSink.
  virtual void SetMetricsSink(MetricsSink* delegate);

  // Sets a default callback to record user action when MetricsSink is not set.
  // This function could be called multiple times (in unittests), and
  // CastMetricsHelper only honors the last one.
  virtual void SetRecordActionCallback(RecordActionCallback callback);

  // Sets an all-0's session ID for running browser tests.
  void SetDummySessionIdForTesting();

 private:
  static std::string EncodeAppInfoIntoMetricsName(
      const std::string& action_name,
      const std::string& app_id,
      const std::string& session_id,
      const std::string& sdk_version);

  friend class base::NoDestructor<CastMetricsHelper>;
  friend class CastMetricsHelperTest;
  friend class MockCastMetricsHelper;

  // |tick_clock| just provided for unit test to construct; normally it should
  // be nullptr when accessed through GetInstance.
  CastMetricsHelper(scoped_refptr<base::SequencedTaskRunner> task_runner =
                        base::SequencedTaskRunnerHandle::Get(),
                    const base::TickClock* tick_clock = nullptr);
  virtual ~CastMetricsHelper();

  void LogEnumerationHistogramEvent(const std::string& name,
                                    int value, int num_buckets);
  void LogTimeHistogramEvent(const std::string& name,
                             base::TimeDelta value,
                             base::TimeDelta min,
                             base::TimeDelta max,
                             int num_buckets);
  void LogMediumTimeHistogramEvent(const std::string& name,
                                   base::TimeDelta value);
  base::Value CreateEventBase(const std::string& name);
  base::TimeTicks Now();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::TickClock* const tick_clock_;

  // Start times for loading the next apps.
  base::flat_map<std::string /* app_id */, base::TimeTicks>
      app_load_start_times_;

  // Start time for the currently running app.
  base::TimeTicks app_start_time_;

  // Currently running app id. Used to construct histogram name.
  std::string app_id_;
  std::string session_id_;
  std::string sdk_version_;

  MetricsSink* metrics_sink_;

  bool logged_first_audio_;

  // Default RecordAction callback when metrics_sink_ is not set.
  RecordActionCallback record_action_callback_;

  DISALLOW_COPY_AND_ASSIGN(CastMetricsHelper);
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_
