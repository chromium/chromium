// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/metrics/cast_metrics_helper.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "chromecast/base/metrics/cast_histograms.h"
#include "chromecast/base/metrics/grouped_histogram.h"

namespace chromecast {
namespace metrics {

// A useful macro to make sure current member function runs on the valid thread.
#define MAKE_SURE_SEQUENCE(callback, ...)                                  \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                       \
    task_runner_->PostTask(                                                \
        FROM_HERE, base::BindOnce(&CastMetricsHelper::callback,            \
                                  base::Unretained(this), ##__VA_ARGS__)); \
    return;                                                                \
  }

namespace {

const char kMetricsNameAppInfoDelimiter = '#';

constexpr base::TimeDelta kAppLoadTimeout = base::Minutes(5);

}  // namespace

// NOTE(gfhuang): This is a hacky way to encode/decode app infos into a
// string. Mainly because it's hard to add another metrics serialization type
// into components/metrics/serialization/.
// static
bool CastMetricsHelper::DecodeAppInfoFromMetricsName(
    const std::string& metrics_name,
    std::string* action_name,
    std::string* app_id,
    std::string* session_id,
    std::string* sdk_version) {
  DCHECK(action_name);
  DCHECK(app_id);
  DCHECK(session_id);
  DCHECK(sdk_version);

  if (!base::Contains(metrics_name, kMetricsNameAppInfoDelimiter)) {
    return false;
  }

  std::vector<std::string> tokens = base::SplitString(
      metrics_name, std::string(1, kMetricsNameAppInfoDelimiter),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(tokens.size(), 4u);
  // The order of tokens should match EncodeAppInfoIntoMetricsName().
  *action_name = tokens[0];
  *app_id = tokens[1];
  *session_id = tokens[2];
  *sdk_version = tokens[3];
  return true;
}

// static
std::string CastMetricsHelper::EncodeAppInfoIntoMetricsName(
    const std::string& action_name,
    const std::string& app_id,
    const std::string& session_id,
    const std::string& sdk_version) {
  std::string result(action_name);
  result.push_back(kMetricsNameAppInfoDelimiter);
  result.append(app_id);
  result.push_back(kMetricsNameAppInfoDelimiter);
  result.append(session_id);
  result.push_back(kMetricsNameAppInfoDelimiter);
  result.append(sdk_version);
  return result;
}

// static
CastMetricsHelper* CastMetricsHelper::GetInstance() {
  static base::NoDestructor<CastMetricsHelper> instance;
  return instance.get();
}

CastMetricsHelper::CastMetricsHelper(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TickClock* tick_clock)
    : task_runner_(std::move(task_runner)),
      tick_clock_(tick_clock),
      metrics_sink_(nullptr),
      logged_first_audio_(false),
      record_action_callback_(
          base::BindRepeating(&base::RecordComputedAction)) {
  DCHECK(task_runner_);
}

CastMetricsHelper::~CastMetricsHelper() {}

void CastMetricsHelper::DidStartLoad(const std::string& app_id) {
  MAKE_SURE_SEQUENCE(DidStartLoad, app_id);
  const base::TimeTicks now = Now();

  // Remove start times for apps that never became the current app.
  for (auto it = app_load_start_times_.cbegin();
       it != app_load_start_times_.cend();) {
    if (now - it->second >= kAppLoadTimeout) {
      it = app_load_start_times_.erase(it);
    } else {
      ++it;
    }
  }

  app_load_start_times_[app_id] = now;
}

void CastMetricsHelper::DidCompleteLoad(const std::string& app_id,
                                        const std::string& session_id) {
  MAKE_SURE_SEQUENCE(DidCompleteLoad, app_id, session_id);
  auto it = app_load_start_times_.find(app_id);
  if (it == app_load_start_times_.end()) {
    LOG(ERROR) << "No start time for app: app_id=" << app_id;
    return;
  }
  app_id_ = app_id;
  session_id_ = session_id;
  app_start_time_ = it->second;
  app_load_start_times_.erase(it);
  logged_first_audio_ = false;
  TagAppStartForGroupedHistograms(app_id_);
  sdk_version_.clear();
}

void CastMetricsHelper::UpdateSDKInfo(const std::string& sdk_version) {
  MAKE_SURE_SEQUENCE(UpdateSDKInfo, sdk_version);
  sdk_version_ = sdk_version;
}

void CastMetricsHelper::LogMediaPlay() {
  MAKE_SURE_SEQUENCE(LogMediaPlay);
  RecordSimpleAction(EncodeAppInfoIntoMetricsName(
      "MediaPlay",
      app_id_,
      session_id_,
      sdk_version_));
}

void CastMetricsHelper::LogMediaPause() {
  MAKE_SURE_SEQUENCE(LogMediaPause);
  RecordSimpleAction(EncodeAppInfoIntoMetricsName(
      "MediaPause",
      app_id_,
      session_id_,
      sdk_version_));
}

void CastMetricsHelper::LogTimeToFirstPaint() {
  MAKE_SURE_SEQUENCE(LogTimeToFirstPaint);
  if (app_id_.empty())
    return;
  base::TimeDelta launch_time = Now() - app_start_time_;
  const std::string uma_name(GetMetricsNameWithAppName("Startup",
                                                       "TimeToFirstPaint"));
  LogMediumTimeHistogramEvent(uma_name, launch_time);
  LOG(INFO) << uma_name << " is " << launch_time.InSecondsF() << " seconds.";
}

void CastMetricsHelper::LogTimeToFirstAudio() {
  MAKE_SURE_SEQUENCE(LogTimeToFirstAudio);
  if (logged_first_audio_)
    return;
  if (app_id_.empty())
    return;
  base::TimeDelta time_to_first_audio = Now() - app_start_time_;
  const std::string uma_name(
      GetMetricsNameWithAppName("Startup", "TimeToFirstAudio"));
  LogMediumTimeHistogramEvent(uma_name, time_to_first_audio);
  LOG(INFO) << uma_name << " is " << time_to_first_audio.InSecondsF()
            << " seconds.";
  logged_first_audio_ = true;
}

void CastMetricsHelper::LogTimeToBufferAv(BufferingType buffering_type,
                                          base::TimeDelta time) {
  MAKE_SURE_SEQUENCE(LogTimeToBufferAv, buffering_type, time);
  if (time.is_negative()) {
    LOG(WARNING) << "Negative time";
    return;
  }

  const std::string uma_name(GetMetricsNameWithAppName(
      "Media",
      (buffering_type == kInitialBuffering ? "TimeToBufferAv" :
       buffering_type == kBufferingAfterUnderrun ?
           "TimeToBufferAvAfterUnderrun" :
       buffering_type == kAbortedBuffering ? "TimeToBufferAvAfterAbort" : "")));

  // Histogram from 250ms to 30s with 50 buckets.
  // The ratio between 2 consecutive buckets is:
  // exp( (ln(30000) - ln(250)) / 50 ) = 1.1
  LogTimeHistogramEvent(uma_name, time, base::Milliseconds(250),
                        base::Milliseconds(30000), 50);
}

std::string CastMetricsHelper::GetMetricsNameWithAppName(
    const std::string& prefix,
    const std::string& suffix) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::string metrics_name(prefix);
  if (!app_id_.empty()) {
    if (!metrics_name.empty())
      metrics_name.push_back('.');
    metrics_name.append(app_id_);
  }
  if (!suffix.empty()) {
    if (!metrics_name.empty())
      metrics_name.push_back('.');
    metrics_name.append(suffix);
  }
  return metrics_name;
}

void CastMetricsHelper::SetMetricsSink(MetricsSink* delegate) {
  MAKE_SURE_SEQUENCE(SetMetricsSink, delegate);
  metrics_sink_ = delegate;
}

void CastMetricsHelper::SetRecordActionCallback(RecordActionCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  record_action_callback_ = std::move(callback);
}

void CastMetricsHelper::SetDummySessionIdForTesting() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  session_id_ = "00000000-0000-0000-0000-000000000000";
}

void CastMetricsHelper::RecordSimpleAction(const std::string& action) {
  MAKE_SURE_SEQUENCE(RecordSimpleAction, action);

  if (metrics_sink_) {
    metrics_sink_->OnAction(action);
  } else {
    record_action_callback_.Run(action);
  }
}

void CastMetricsHelper::LogEnumerationHistogramEvent(
    const std::string& name, int value, int num_buckets) {
  MAKE_SURE_SEQUENCE(LogEnumerationHistogramEvent, name, value, num_buckets);

  if (metrics_sink_) {
    metrics_sink_->OnEnumerationEvent(name, value, num_buckets);
  } else {
    UMA_HISTOGRAM_ENUMERATION_NO_CACHE(name, value, num_buckets);
  }
}

void CastMetricsHelper::LogTimeHistogramEvent(const std::string& name,
                                              base::TimeDelta value,
                                              base::TimeDelta min,
                                              base::TimeDelta max,
                                              int num_buckets) {
  MAKE_SURE_SEQUENCE(LogTimeHistogramEvent, name, value, min, max, num_buckets);

  if (metrics_sink_) {
    metrics_sink_->OnTimeEvent(name, value, min, max, num_buckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES_NO_CACHE(name, value, min, max, num_buckets);
  }
}

void CastMetricsHelper::LogMediumTimeHistogramEvent(const std::string& name,
                                                    base::TimeDelta value) {
  // Follow UMA_HISTOGRAM_MEDIUM_TIMES definition.
  LogTimeHistogramEvent(name, value, base::Milliseconds(10), base::Minutes(3),
                        50);
}

base::Value::Dict CastMetricsHelper::CreateEventBase(const std::string& name) {
  base::Value::Dict cast_event;
  cast_event.Set("name", name);
  const double time = (Now() - base::TimeTicks()).InMicrosecondsF();
  cast_event.Set("time", time);
  return cast_event;
}

void CastMetricsHelper::RecordEventWithValue(const std::string& event,
                                             int value) {
  base::Value::Dict cast_event = CreateEventBase(event);
  cast_event.Set("value", value);
  std::string message;
  base::JSONWriter::Write(cast_event, &message);
  RecordSimpleAction(message);
}

void CastMetricsHelper::RecordApplicationEvent(const std::string& event) {
  RecordApplicationEvent(app_id_, session_id_, sdk_version_, event);
}

void CastMetricsHelper::RecordApplicationEvent(const std::string& app_id,
                                               const std::string& session_id,
                                               const std::string& sdk_version,
                                               const std::string& event) {
  base::Value::Dict cast_event = CreateEventBase(event);
  cast_event.Set("app_id", app_id);
  cast_event.Set("session_id", session_id);
  cast_event.Set("sdk_version", sdk_version);
  std::string message;
  base::JSONWriter::Write(cast_event, &message);
  RecordSimpleAction(message);
}

void CastMetricsHelper::RecordApplicationEventWithValue(
    const std::string& event,
    int value) {
  base::Value::Dict cast_event = CreateEventBase(event);
  cast_event.Set("app_id", app_id_);
  cast_event.Set("session_id", session_id_);
  cast_event.Set("sdk_version", sdk_version_);
  cast_event.Set("value", value);
  std::string message;
  base::JSONWriter::Write(cast_event, &message);
  RecordSimpleAction(message);
}

void CastMetricsHelper::RecordApplicationEventWithValue(
    const std::string& app_id,
    const std::string& session_id,
    const std::string& sdk_version,
    const std::string& event,
    int value) {
  base::Value::Dict cast_event = CreateEventBase(event);
  cast_event.Set("app_id", app_id);
  cast_event.Set("session_id", session_id);
  cast_event.Set("sdk_version", sdk_version);
  cast_event.Set("value", value);
  std::string message;
  base::JSONWriter::Write(cast_event, &message);
  RecordSimpleAction(message);
}

base::TimeTicks CastMetricsHelper::Now() {
  return tick_clock_ ? tick_clock_->NowTicks() : base::TimeTicks::Now();
}

}  // namespace metrics
}  // namespace chromecast
