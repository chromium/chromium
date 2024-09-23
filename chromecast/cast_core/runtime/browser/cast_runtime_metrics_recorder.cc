// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder.h"

#include <cfloat>
#include <cmath>
#include <optional>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/base/metrics/cast_histograms.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "third_party/metrics_proto/cast_logs.pb.h"

namespace chromecast {
namespace {

// The following are used by cast events created from a JSON format string.
constexpr char kEventName[] = "name";
constexpr char kEventAppId[] = "app_id";
constexpr char kEventSessionId[] = "session_id";
constexpr char kEventSdkVersion[] = "sdk_version";
constexpr char kEventTime[] = "time";
constexpr char kEventValue[] = "value";
constexpr char kEventEventsPair[] = "events";
constexpr char kEventAuditReport[] = "audit_report";

constexpr int64_t kInt64DoubleMax = 1LL << DBL_MANT_DIG;

constexpr size_t kCastEventLimit = 1000000;

void PopulateEventBuilder(CastEventBuilder* event_builder,
                          double event_time,
                          const std::string& app_id,
                          const std::string& sdk_version,
                          const std::string& session_id) {
  event_builder->SetTime(base::TimeTicks() + base::Microseconds(event_time));
  if (!app_id.empty())
    event_builder->SetAppId(app_id);
  if (!sdk_version.empty())
    event_builder->SetSdkVersion(sdk_version);
  if (!session_id.empty())
    event_builder->SetSessionId(session_id);
}

}  // namespace

CastRuntimeMetricsRecorder::EventBuilderFactory::~EventBuilderFactory() =
    default;

// NOTE: This is the same as MetricsRecorderImpl but it seems improper to mix
// MetricsRecorderImpl's static functions when this is the actual
// MetricsRecorder instance that is live.
// static
void CastRuntimeMetricsRecorder::RecordSimpleActionWithValue(
    const std::string& action,
    int64_t value) {
  MetricsRecorder* recorder = MetricsRecorder::GetInstance();
  std::unique_ptr<CastEventBuilder> event_builder(
      recorder->CreateEventBuilder(action));
  event_builder->SetExtraValue(value);
  recorder->RecordCastEvent(std::move(event_builder));
}

CastRuntimeMetricsRecorder::CastRuntimeMetricsRecorder(
    EventBuilderFactory* event_builder_factory)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      event_builder_factory_(event_builder_factory) {
  DCHECK(event_builder_factory_);

  DCHECK(!MetricsRecorder::GetInstance());
  MetricsRecorder::SetInstance(this);
  metrics::CastMetricsHelper::GetInstance()->SetRecordActionCallback(
      base::BindRepeating(
          &CastRuntimeMetricsRecorder::CastMetricsHelperRecordActionCallback,
          weak_factory_.GetWeakPtr()));
}

CastRuntimeMetricsRecorder::~CastRuntimeMetricsRecorder() {
  DCHECK(MetricsRecorder::GetInstance() == this);
  metrics::CastMetricsHelper::GetInstance()->SetRecordActionCallback(
      base::BindRepeating(&base::RecordComputedAction));
  MetricsRecorder::SetInstance(nullptr);
}

std::vector<cast::metrics::Event> CastRuntimeMetricsRecorder::TakeEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::move(events_);
}

std::unique_ptr<CastEventBuilder>
CastRuntimeMetricsRecorder::CreateEventBuilder(const std::string& name) {
  auto builder = event_builder_factory_->CreateEventBuilder();
  builder->SetName(name);
  return builder;
}

void CastRuntimeMetricsRecorder::AddActiveConnection(
    const std::string& transport_connection_id,
    const std::string& virtual_connection_id,
    const base::Value& sender_info,
    const net::IPAddressBytes& sender_ip) {
  NOTIMPLEMENTED();
}

void CastRuntimeMetricsRecorder::RemoveActiveConnection(
    const std::string& connection_id) {
  NOTIMPLEMENTED();
}

void CastRuntimeMetricsRecorder::RecordCastEvent(
    std::unique_ptr<CastEventBuilder> event_builder) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    DVLOG(1) << "RecordCastEvent direct";
    RecordCastEventOnSequence(std::move(event_builder));
  } else {
    DVLOG(1) << "RecordCastEvent bounce";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastRuntimeMetricsRecorder::RecordCastEventOnSequence,
                       weak_factory_.GetWeakPtr(), std::move(event_builder)));
  }
}

void CastRuntimeMetricsRecorder::RecordCastEventOnSequence(
    std::unique_ptr<CastEventBuilder> event_builder) {
  DVLOG(1) << "RecordCastEventOnSequence";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (events_.size() >= kCastEventLimit) {
    static bool logged_once = false;
    if (!logged_once) {
      LOG(ERROR) << "Too many events queued, dropping...";
      logged_once = true;
    }
    return;
  }
  std::unique_ptr<::metrics::CastLogsProto_CastEventProto> event_proto(
      event_builder->Build());
  std::string str;
  if (!event_proto->SerializeToString(&str)) {
    LOG(ERROR) << "Failed to serialize CastEventProto";
    return;
  }

  cast::metrics::Event e;
  e.mutable_impl_defined_event()->set_name(event_builder->GetName());
  e.mutable_impl_defined_event()->set_data(str);
  events_.push_back(std::move(e));
}

void CastRuntimeMetricsRecorder::RecordHistogramTime(const std::string& name,
                                                     int sample,
                                                     int min,
                                                     int max,
                                                     int num_buckets) {
  UMA_HISTOGRAM_CUSTOM_TIMES_NO_CACHE(name, base::Milliseconds(sample),
                                      base::Milliseconds(min),
                                      base::Milliseconds(max), num_buckets);
}

void CastRuntimeMetricsRecorder::RecordHistogramCount(const std::string& name,
                                                      int sample,
                                                      int min,
                                                      int max,
                                                      int num_buckets) {
  UMA_HISTOGRAM_CUSTOM_COUNTS_NO_CACHE(name, sample, min, max, num_buckets,
                                       /* count */ 1);
}

void CastRuntimeMetricsRecorder::RecordHistogramCountRepeated(
    const std::string& name,
    int sample,
    int min,
    int max,
    int num_buckets,
    int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS_NO_CACHE(name, sample, min, max, num_buckets,
                                       count);
}

void CastRuntimeMetricsRecorder::RecordHistogramEnum(const std::string& name,
                                                     int sample,
                                                     int boundary) {
  UMA_HISTOGRAM_ENUMERATION_NO_CACHE(name, sample, boundary);
}

void CastRuntimeMetricsRecorder::RecordHistogramSparse(const std::string& name,
                                                       int sample) {
  base::HistogramBase* counter = base::SparseHistogram::FactoryGet(
      name, base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void CastRuntimeMetricsRecorder::CastMetricsHelperRecordActionCallback(
    const std::string& action) {
  DVLOG(1) << "Record action via CastMetricsHelper: " << action;
  std::string action_name;
  std::string app_id;
  std::string session_id;
  std::string sdk_version;
  if (metrics::CastMetricsHelper::DecodeAppInfoFromMetricsName(
          action, &action_name, &app_id, &session_id, &sdk_version)) {
    if (app_id.empty() && session_id.empty()) {
      LOG(ERROR) << "Missing app and session ID";
      return;
    }
    std::unique_ptr<CastEventBuilder> event_builder(
        CreateEventBuilder(action_name));
    event_builder->SetAppId(app_id)
        .SetSessionId(session_id)
        .SetSdkVersion(sdk_version);
    RecordCastEvent(std::move(event_builder));
    return;
  }

  // Tries to parse and record event string as JSON event.
  if (RecordJsonCastEvent(action)) {
    return;
  }

  // This is normal non-JSON user action event.
  RecordCastEvent(CreateEventBuilder(action));
}

bool CastRuntimeMetricsRecorder::RecordJsonCastEvent(const std::string& event) {
  std::unique_ptr<const base::Value> value(
      JSONStringValueDeserializer(event).Deserialize(nullptr, nullptr));
  if (!value) {
    LOG(ERROR) << "This is not a JSON format event: " << event;
    return false;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "This is not a dictionary JSON format event: " << event;
    return false;
  }

  const base::Value::Dict& value_dict = value->GetDict();
  const std::string* name = value_dict.FindString(kEventName);
  if (!name) {
    LOG(ERROR) << "Missing field:" << kEventName;
    return false;
  }

  // Gets event creation time. If unavailable use now.
  std::optional<double> maybe_event_time = value_dict.FindDouble(kEventTime);
  double event_time = 0;
  if (maybe_event_time && maybe_event_time.value() > 0) {
    event_time = maybe_event_time.value();
  } else {
    event_time = (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
  }
  // Gets App Id.
  const std::string* maybe_app_id = value_dict.FindString(kEventAppId);
  std::string app_id;
  if (maybe_app_id) {
    app_id = *maybe_app_id;
  }
  // Gets session Id.
  const std::string* maybe_session_id = value_dict.FindString(kEventSessionId);
  std::string session_id;
  if (maybe_session_id) {
    session_id = *maybe_session_id;
  }
  // Gets SDK version.
  const std::string* maybe_sdk_version =
      value_dict.FindString(kEventSdkVersion);
  std::string sdk_version;
  if (maybe_sdk_version) {
    sdk_version = *maybe_sdk_version;
  }

  const base::Value::Dict* multiple_events =
      value_dict.FindDict(kEventEventsPair);
  if (!multiple_events) {
    std::unique_ptr<CastEventBuilder> event_builder(CreateEventBuilder(*name));
    PopulateEventBuilder(event_builder.get(), event_time, app_id, sdk_version,
                         session_id);
    std::optional<double> maybe_event_value =
        value_dict.FindDouble(kEventValue);
    if (maybe_event_value) {
      double event_value = maybe_event_value.value();
      DCHECK_EQ(event_value, std::nearbyint(event_value));
      DCHECK_LE(std::abs(event_value), kInt64DoubleMax);
      event_builder->SetExtraValue(static_cast<int64_t>(event_value));
    }

    const std::string* audit_report = value_dict.FindString(kEventAuditReport);
    if (audit_report) {
      event_builder->SetAuditReport(*audit_report);
    }

    RecordCastEvent(std::move(event_builder));
    return true;
  }

  for (auto kv : *multiple_events) {
    std::optional<double> maybe_event_value = kv.second.GetIfDouble();
    if (!maybe_event_value) {
      continue;
    }

    double event_value = maybe_event_value.value();
    DCHECK_EQ(event_value, std::nearbyint(event_value));
    DCHECK_LE(std::abs(event_value), kInt64DoubleMax);

    std::unique_ptr<CastEventBuilder> event_builder(
        CreateEventBuilder(*name + "." + kv.first));
    event_builder->SetExtraValue(static_cast<int64_t>(event_value));
    PopulateEventBuilder(event_builder.get(), event_time, app_id, sdk_version,
                         session_id);
    RecordCastEvent(std::move(event_builder));
  }
  return true;
}

}  // namespace chromecast
