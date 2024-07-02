// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_recorder.h"

#include <sstream>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

StructuredMetricsRecorder::StructuredMetricsRecorder(
    std::unique_ptr<KeyDataProvider> key_data_provider,
    std::unique_ptr<EventStorage<StructuredEventProto>> event_storage)
    : RefCountedDeleteOnSequence(
          base::SequencedTaskRunner::GetCurrentDefault()),
      key_data_provider_(std::move(key_data_provider)),
      event_storage_(std::move(event_storage)) {
  CHECK(key_data_provider_);
  CHECK(event_storage_);
  Recorder::GetInstance()->SetRecorder(this);
  key_data_provider_->AddObserver(this);
}

StructuredMetricsRecorder::~StructuredMetricsRecorder() {
  Recorder::GetInstance()->UnsetRecorder(this);
  key_data_provider_->RemoveObserver(this);
}

void StructuredMetricsRecorder::EnableRecording() {
  DCHECK(base::CurrentUIThread::IsSet());
  // Enable recording only if structured metrics' feature flag is enabled.
  recording_enabled_ =
      base::FeatureList::IsEnabled(features::kStructuredMetrics);
  if (recording_enabled_) {
    CacheDisallowedProjectsSet();
  }
}

void StructuredMetricsRecorder::DisableRecording() {
  DCHECK(base::CurrentUIThread::IsSet());
  recording_enabled_ = false;
  disallowed_projects_.clear();
}
void StructuredMetricsRecorder::ProvideUmaEventMetrics(
    ChromeUserMetricsExtension& uma_proto) {
  // no-op
}

void StructuredMetricsRecorder::ProvideEventMetrics(
    ChromeUserMetricsExtension& uma_proto) {
  if (!CanProvideMetrics() || !event_storage_->HasEvents()) {
    return;
  }

  LockStorage();

  // Get the events from event storage.
  auto events = event_storage_->TakeEvents();

  ReleaseStorage();

  StructuredDataProto& structured_data = *uma_proto.mutable_structured_data();
  *structured_data.mutable_events() = std::move(events);

  LogUploadSizeBytes(structured_data.ByteSizeLong());
  LogNumEventsInUpload(structured_data.events_size());
}

void StructuredMetricsRecorder::ProvideLogMetadata(
    ChromeUserMetricsExtension& uma_proto) {
  // Applies custom metadata providers.
  Recorder::GetInstance()->OnProvideIndependentMetrics(&uma_proto);
}

bool StructuredMetricsRecorder::CanProvideMetrics() {
  // We can provide metrics once device or profile keys have been loaded.
  return recording_enabled() && (IsInitialized() || IsProfileInitialized());
}

bool StructuredMetricsRecorder::HasMetricsToProvide() {
  return event_storage()->HasEvents();
}

void StructuredMetricsRecorder::OnKeyReady() {
  DCHECK(base::CurrentUIThread::IsSet());

  // If key data has not been initialized, it is highly likely that the key data
  // is initialized.
  if (!init_state_.Has(State::kKeyDataInitialized)) {
    init_state_.Put(State::kKeyDataInitialized);
  } else {
    // If kKeyDataInitialized, then this is the second time this callback is
    // being called, which must be the profile keys.
    init_state_.Put(State::kProfileKeyDataInitialized);
  }

  // If recorder is now ready then hash events in-memory and store them in
  // persistent storage.
  if (CanProvideMetrics()) {
    HashUnhashedEventsAndPersist();
    if (!on_ready_callback_.is_null()) {
      std::move(on_ready_callback_).Run();
    }
  }
}

void StructuredMetricsRecorder::AddEventsObserver(Observer* watcher) {
  watchers_.AddObserver(watcher);
}

void StructuredMetricsRecorder::RemoveEventsObserver(Observer* watcher) {
  watchers_.RemoveObserver(watcher);
}

void StructuredMetricsRecorder::OnEventRecord(const Event& event) {
  DCHECK(base::CurrentUIThread::IsSet());

  // One more state for the EventRecordingState exists: kMetricsProviderMissing.
  // This is recorded in Recorder::Record.
  if (!recording_enabled_ && !CanForceRecord(event)) {
    // Events should be ignored if recording is disabled.
    LogEventRecordingState(EventRecordingState::kRecordingDisabled);
    return;
  }

  if (IsDeviceEvent(event) && !IsInitialized()) {
    RecordEventBeforeInitialization(event);
    return;
  }

  if (IsProfileEvent(event) && !IsProfileInitialized()) {
    RecordProfileEventBeforeInitialization(event);
    return;
  }

  RecordEvent(event);
  test_callback_on_record_.Run();
}

bool StructuredMetricsRecorder::HasState(State state) const {
  return init_state_.Has(state);
}

void StructuredMetricsRecorder::Purge() {
  CHECK(event_storage_);
  event_storage_->Purge();
  key_data_provider_->Purge();

  unhashed_events_.clear();
  unhashed_profile_events_.clear();
}

void StructuredMetricsRecorder::RecordEventBeforeInitialization(
    const Event& event) {
  DCHECK(!IsInitialized());
  unhashed_events_.emplace_back(event.Clone());
}

void StructuredMetricsRecorder::RecordProfileEventBeforeInitialization(
    const Event& event) {
  DCHECK(!IsProfileInitialized());
  unhashed_profile_events_.emplace_back(event.Clone());
}

void StructuredMetricsRecorder::RecordEvent(const Event& event) {
  DCHECK(IsKeyDataInitialized());

  // Retrieve key for the project.
  KeyData* key_data = key_data_provider_->GetKeyData(event.project_name());
  if (!key_data) {
    return;
  }

  // Validates the event. If valid, retrieve the metadata associated
  // with the event.
  const auto validators = GetEventValidators(event);
  if (!validators) {
    return;
  }

  const auto* project_validator = validators->first;
  const auto* event_validator = validators->second;

  if (!CanUploadProject(project_validator->project_hash())) {
    LogEventRecordingState(EventRecordingState::kProjectDisallowed);
    return;
  }

  LogEventRecordingState(EventRecordingState::kRecorded);

  // Events associated with UMA are deprecated.
  if (project_validator->id_type() == IdType::kUmaId) {
    return;
  }

  StructuredEventProto event_proto;

  // Initialize event proto from validator metadata.
  InitializeEventProto(&event_proto, event, *project_validator,
                       *event_validator);

  // Sequence-related metadata.
  if (project_validator->event_type() == StructuredEventProto::SEQUENCE) {
    AddSequenceMetadata(&event_proto, event, *project_validator, *key_data);
  }

  // Populate the metrics and add to proto.
  AddMetricsToProto(&event_proto, event, *project_validator, *event_validator);

  // Log size information about the event.
  LogEventSerializedSizeBytes(event_proto.ByteSizeLong());

  Recorder::GetInstance()->OnEventRecorded(&event_proto);
  NotifyEventRecorded(event_proto);

  // Add new event to storage.
  if (storage_lock_.load()) {
    locked_events_.push_back(event_proto);
  } else {
    event_storage_->AddEvent(event_proto);
  }

  test_callback_on_record_.Run();
}

void StructuredMetricsRecorder::InitializeEventProto(
    StructuredEventProto* proto,
    const Event& event,
    const ProjectValidator& project_validator,
    const EventValidator& event_validator) {
  proto->set_project_name_hash(project_validator.project_hash());
  proto->set_event_name_hash(event_validator.event_hash());

  // Set the event type. Do this with a switch statement to catch when the
  // event type is UNKNOWN or uninitialized.
  CHECK_NE(project_validator.event_type(), StructuredEventProto::UNKNOWN);

  proto->set_event_type(project_validator.event_type());

  // Set the ID for this event, if any.
  switch (project_validator.id_type()) {
    case IdType::kProjectId: {
      std::optional<uint64_t> primary_id =
          key_data_provider_->GetId(event.project_name());
      if (primary_id.has_value()) {
        proto->set_profile_event_id(primary_id.value());
      }
    } break;
    case IdType::kUmaId:
      // TODO(crbug.com/40156926): Unimplemented.
      break;
    case IdType::kUnidentified:
      // Do nothing.
      break;
  }
}

void StructuredMetricsRecorder::AddMetricsToProto(
    StructuredEventProto* proto,
    const Event& event,
    const ProjectValidator& project_validator,
    const EventValidator& event_validator) {
  KeyData* key = key_data_provider_->GetKeyData(event.project_name());

  // Key is checked by the calling function.
  CHECK(key);

  // Set each metric's name hash and value.
  for (const auto& metric : event.metric_values()) {
    const std::string& metric_name = metric.first;
    const Event::MetricValue& metric_value = metric.second;

    // Validate that both name and metric type are valid structured metrics.
    // If a metric is invalid, then ignore the metric so that other valid
    // metrics are added to the proto.
    std::optional<EventValidator::MetricMetadata> metadata =
        event_validator.GetMetricMetadata(metric_name);

    // Checks that the metrics defined are valid. If not valid, then the
    // metric will be ignored.
    bool is_valid =
        metadata.has_value() && metadata->metric_type == metric_value.type;
    DCHECK(is_valid);
    if (!is_valid) {
      continue;
    }

    StructuredEventProto::Metric* metric_proto = proto->add_metrics();
    int64_t metric_name_hash = metadata->metric_name_hash;
    metric_proto->set_name_hash(metric_name_hash);

    const auto& value = metric_value.value;
    switch (metadata->metric_type) {
      case Event::MetricType::kHmac:
        metric_proto->set_value_hmac(key->HmacMetric(
            project_validator.project_hash(), metric_name_hash,
            value.GetString(),
            base::Days(project_validator.key_rotation_period())));
        break;
      case Event::MetricType::kLong:
        int64_t long_value;
        base::StringToInt64(value.GetString(), &long_value);
        metric_proto->set_value_int64(long_value);
        break;
      case Event::MetricType::kRawString:
        metric_proto->set_value_string(value.GetString());
        break;
      case Event::MetricType::kDouble:
        metric_proto->set_value_double(value.GetDouble());
        break;
      // Represents an enum.
      case Event::MetricType::kInt:
        metric_proto->set_value_int64(value.GetInt());
        break;
      // Not supported yet.
      case Event::MetricType::kBoolean:
        break;
    }
  }
}

void StructuredMetricsRecorder::HashUnhashedEventsAndPersist() {
  if (IsInitialized()) {
    LogNumEventsRecordedBeforeInit(unhashed_events_.size());
    while (!unhashed_events_.empty()) {
      OnEventRecord(unhashed_events_.front());
      unhashed_events_.pop_front();
    }
  }
  if (IsProfileInitialized()) {
    LogNumEventsRecordedBeforeInit(unhashed_profile_events_.size());
    while (!unhashed_profile_events_.empty()) {
      OnEventRecord(unhashed_profile_events_.front());
      unhashed_profile_events_.pop_front();
    }
  }
}

bool StructuredMetricsRecorder::CanUploadProject(
    uint64_t project_name_hash) const {
  return !disallowed_projects_.contains(project_name_hash);
}

void StructuredMetricsRecorder::CacheDisallowedProjectsSet() {
  const std::string& disallowed_list = GetDisabledProjects();
  if (disallowed_list.empty()) {
    return;
  }

  for (const auto& value :
       base::SplitString(disallowed_list, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    uint64_t project_name_hash;
    // Parse the string and keep only perfect conversions.
    if (base::StringToUint64(value, &project_name_hash)) {
      disallowed_projects_.insert(project_name_hash);
    }
  }
}

bool StructuredMetricsRecorder::IsKeyDataInitialized() {
  return key_data_provider_->IsReady();
}

bool StructuredMetricsRecorder::IsInitialized() {
  return init_state_.Has(State::kKeyDataInitialized);
}

bool StructuredMetricsRecorder::IsProfileInitialized() {
  return init_state_.Has(State::kProfileKeyDataInitialized);
}

bool StructuredMetricsRecorder::CanForceRecord(const Event& event) const {
  const auto validators = GetEventValidators(event);
  if (!validators) {
    return false;
  }
  return validators->second->can_force_record();
}

bool StructuredMetricsRecorder::IsDeviceEvent(const Event& event) const {
  // Validates the event. If valid, retrieve the metadata associated
  // with the event.
  const auto validators = GetEventValidators(event);
  if (!validators) {
    return false;
  }
  const auto* project_validator = validators->first;

  // Sequence events are marked as per-device but use the profile keys.
  return !event.IsEventSequenceType() &&
         project_validator->id_scope() == IdScope::kPerDevice;
}

bool StructuredMetricsRecorder::IsProfileEvent(const Event& event) const {
  // Validates the event. If valid, retrieve the metadata associated
  // with the event.
  const auto validators = GetEventValidators(event);
  if (!validators) {
    return false;
  }
  const auto* project_validator = validators->first;

  // Sequence events are marked as per-device but use the profile keys.
  return event.IsEventSequenceType() ||
         project_validator->id_scope() == IdScope::kPerProfile;
}

std::optional<std::pair<const ProjectValidator*, const EventValidator*>>
StructuredMetricsRecorder::GetEventValidators(const Event& event) const {
  const auto* project_validator =
      validator::Validators::Get()->GetProjectValidator(event.project_name());

  if (!project_validator) {
    return std::nullopt;
  }

  const auto* event_validator =
      project_validator->GetEventValidator(event.event_name());

  if (!event_validator) {
    return std::nullopt;
  }

  return std::make_pair(project_validator, event_validator);
}

void StructuredMetricsRecorder::SetOnReadyToRecord(base::OnceClosure callback) {
  on_ready_callback_ = std::move(callback);

  if (IsInitialized()) {
    std::move(on_ready_callback_).Run();
  }
}

void StructuredMetricsRecorder::SetEventRecordCallbackForTest(
    base::RepeatingClosure callback) {
  test_callback_on_record_ = std::move(callback);
}

void StructuredMetricsRecorder::AddDisallowedProjectForTest(
    uint64_t project_name_hash) {
  disallowed_projects_.insert(project_name_hash);
}

void StructuredMetricsRecorder::NotifyEventRecorded(
    const StructuredEventProto& event) {
  for (Observer& watcher : watchers_) {
    watcher.OnEventRecorded(event);
  }
}

void StructuredMetricsRecorder::LockStorage() {
  storage_lock_.store(true);
}

void StructuredMetricsRecorder::ReleaseStorage() {
  storage_lock_.store(false);

  StoreLockedEvents();
}

void StructuredMetricsRecorder::StoreLockedEvents() {
  base::SequencedTaskRunner* task_runner =
      Recorder::GetInstance()->GetUiTaskRunner();

  if (!task_runner->RunsTasksInCurrentSequence()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&StructuredMetricsRecorder::StoreLockedEvents,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  for (const auto& event : locked_events_) {
    event_storage_->AddEvent(event);
  }

  locked_events_.clear();
}

}  // namespace metrics::structured
