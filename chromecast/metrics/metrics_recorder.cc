// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/metrics_recorder.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/metrics/cast_event_builder.h"
#include "net/base/ip_address.h"

namespace chromecast {

namespace {

bool IsLogOn(int verbose_log_level) {
  // TODO(b/135640848): Determine a better way to log metrics for
  // Fuchsia, without producing logspam during development.
  return -verbose_log_level >= logging::GetMinLogLevel() ||
         (DCHECK_IS_ON() && VLOG_IS_ON(verbose_log_level));
}

// A no-op dummy event builder, used when MetricsRecorder is nullptr.
class DummyEventBuilder : public CastEventBuilder {
 public:
  // receiver::CastEventBuilder implementation
  std::string GetName() override { return ""; }

  CastEventBuilder& SetName(const std::string& name) override { return *this; }

  CastEventBuilder& SetTime(const base::TimeTicks& time) override {
    return *this;
  }

  CastEventBuilder& SetTimezoneId(const std::string& timezone_id) override {
    return *this;
  }

  CastEventBuilder& SetAppId(const std::string& app_id) override {
    return *this;
  }

  CastEventBuilder& SetRemoteAppId(const std::string& remote_app_id) override {
    return *this;
  }

  CastEventBuilder& SetSessionId(const std::string& session_id) override {
    return *this;
  }

  CastEventBuilder& SetSdkVersion(const std::string& sdk_version) override {
    return *this;
  }

  CastEventBuilder& SetMplVersion(const std::string& mpl_version) override {
    return *this;
  }

  CastEventBuilder& SetConnectionInfo(
      const std::string& transport_connection_id,
      const std::string& virtual_connection_id) override {
    return *this;
  }

  CastEventBuilder& SetGroupUuid(const std::string& group_uuid) override {
    return *this;
  }

  CastEventBuilder& SetExtraValue(int64_t extra_value) override {
    return *this;
  }

  CastEventBuilder& SetConversationKey(
      const std::string& conversation_key) override {
    return *this;
  }

  CastEventBuilder& SetRequestId(int32_t request_id) override { return *this; }

  CastEventBuilder& SetEventId(const std::string& event_id) override {
    return *this;
  }

  CastEventBuilder& SetAoghRequestId(const std::string& request_id) override {
    return *this;
  }

  CastEventBuilder& SetAoghLocalDeviceId(int64_t local_id) override {
    return *this;
  }

  CastEventBuilder& SetAoghAgentId(const std::string& agent_id) override {
    return *this;
  }

  CastEventBuilder& SetAoghStandardAgentId(
      const std::string& agent_id) override {
    return *this;
  }

  CastEventBuilder& SetUiVersion(const std::string& ui_version) override {
    return *this;
  }

  CastEventBuilder& SetAuditReport(const std::string& audit_report) override {
    return *this;
  }

  CastEventBuilder& SetDuoCoreVersion(int64_t version) override {
    return *this;
  }

  CastEventBuilder& SetHotwordModelId(const std::string& model_id) override {
    return *this;
  }

  CastEventBuilder& SetDiscoveryAppSubtype(const std::string& app_id) override {
    return *this;
  }

  CastEventBuilder& SetDiscoveryNamespaceSubtype(
      const std::string& namespace_hash) override {
    return *this;
  }

  CastEventBuilder& SetDiscoverySender(
      const net::IPAddressBytes& sender_ip) override {
    return *this;
  }

  CastEventBuilder& SetDiscoveryUnicastFlag(bool uses_unicast) override {
    return *this;
  }

  CastEventBuilder& SetFeatureVector(
      const std::vector<float>& features) override {
    return *this;
  }

  CastEventBuilder& AddMetadata(const std::string& name,
                                int64_t value) override {
    return *this;
  }

  CastEventBuilder& SetLaunchFrom(LaunchFrom launch_from) override {
    return *this;
  }

  CastEventBuilder& MergeFrom(
      const ::metrics::CastLogsProto_CastEventProto* event_proto) override {
    return *this;
  }

  ::metrics::CastLogsProto_CastEventProto* Build() override { NOTREACHED(); }
};

MetricsRecorder* g_instance = nullptr;

}  // namespace

void RecordEventWithLogPrefix(const std::string& action,
                              std::unique_ptr<CastEventBuilder> event_builder,
                              int verbose_log_level,
                              const std::string& log_prefix) {
  MetricsRecorder* recorder = MetricsRecorder::GetInstance();
  if (recorder && event_builder) {
    recorder->RecordCastEvent(std::move(event_builder));
  }

  if (IsLogOn(verbose_log_level)) {
    VLOG_STREAM(verbose_log_level) << log_prefix << action;
  }
}

std::unique_ptr<CastEventBuilder> CreateCastEvent(const std::string& name) {
  MetricsRecorder* recorder = MetricsRecorder::GetInstance();
  if (recorder) {
    return recorder->CreateEventBuilder(name);
  }
  return std::make_unique<DummyEventBuilder>();
}

void RecordCastEvent(const std::string& log_name,
                     std::unique_ptr<CastEventBuilder> event_builder,
                     int verbose_log_level) {
  RecordEventWithLogPrefix(log_name, std::move(event_builder),
                           verbose_log_level, "cast event: ");
}

void RecordAction(const std::string& action, int verbose_log_level) {
  RecordEventWithLogPrefix(action, CreateCastEvent(action), verbose_log_level,
                           "Record action: ");
}

void LogAction(const std::string& action, int verbose_log_level) {
  RecordEventWithLogPrefix(action, std::unique_ptr<CastEventBuilder>(),
                           verbose_log_level, "Log action: ");
}

void RecordHistogramTime(const std::string& name,
                         int sample,
                         int min,
                         int max,
                         int num_buckets,
                         int verbose_log_level) {
  MetricsRecorder* recorder = MetricsRecorder::GetInstance();
  if (recorder) {
    recorder->RecordHistogramTime(name, sample, min, max, num_buckets);
  }

  if (IsLogOn(verbose_log_level)) {
    VLOG_STREAM(verbose_log_level)
        << "Time histogram: " << name << ", sample=" << sample
        << ", max=" << max << ", min=" << min
        << ", num_buckets=" << num_buckets;
  }
}

void RecordHistogramCount(const std::string& name,
                          int sample,
                          int min,
                          int max,
                          int num_buckets,
                          int verbose_log_level) {
  MetricsRecorder* recorder = MetricsRecorder::GetInstance();
  if (recorder) {
    recorder->RecordHistogramCount(name, sample, min, max, num_buckets);
  }

  if (IsLogOn(verbose_log_level)) {
    VLOG_STREAM(verbose_log_level)
        << "Count histogram: " << name << ", sample=" << sample
        << ", max=" << max << ", min=" << min
        << ", num_buckets=" << num_buckets;
  }
}

void RecordHistogramEnum(const std::string& name,
                         int sample,
                         int boundary,
                         int verbose_log_level) {
  MetricsRecorder* recorder = MetricsRecorder::GetInstance();
  if (recorder) {
    recorder->RecordHistogramEnum(name, sample, boundary);
  }

  if (IsLogOn(verbose_log_level)) {
    VLOG_STREAM(verbose_log_level)
        << "Count histogram: " << name << ", sample=" << sample
        << ", boundary=" << boundary;
  }
}

struct MetricsRecorder::ObserverList {
  base::ObserverList<Observer>::Unchecked list;
};

// static
void MetricsRecorder::SetInstance(MetricsRecorder* recorder) {
  g_instance = recorder;
}

// static
MetricsRecorder* MetricsRecorder::GetInstance() {
  return g_instance;
}

MetricsRecorder::MetricsRecorder()
    : observer_list_(std::make_unique<ObserverList>()) {}

MetricsRecorder::~MetricsRecorder() = default;

void MetricsRecorder::NotifyOnPreUpload() {
  for (auto& o : observer_list_->list)
    o.OnPreUpload();
}

void MetricsRecorder::AddObserver(Observer* o) {
  DCHECK(o);
  observer_list_->list.AddObserver(o);
}
void MetricsRecorder::RemoveObserver(Observer* o) {
  DCHECK(o);
  observer_list_->list.RemoveObserver(o);
}

void RecordCastEvent(const std::string& event,
                     bool has_extra_value,
                     int64_t value,
                     MetricsRecorder* metrics_recorder) {
  DCHECK(metrics_recorder);
  auto event_builder = metrics_recorder->CreateEventBuilder(event);
  if (has_extra_value) {
    event_builder->SetExtraValue(value);
  }
  metrics_recorder->RecordCastEvent(std::move(event_builder));
}

void RecordCastEventWithMetadata(
    const std::string& event,
    const base::flat_map<std::string, int64_t>& settings_map,
    MetricsRecorder* metrics_recorder) {
  DCHECK(metrics_recorder);
  auto event_builder = metrics_recorder->CreateEventBuilder(event);
  for (const auto& kv_pair : settings_map) {
    event_builder->AddMetadata(kv_pair.first, kv_pair.second);
  }
  metrics_recorder->RecordCastEvent(std::move(event_builder));
}

}  // namespace chromecast
