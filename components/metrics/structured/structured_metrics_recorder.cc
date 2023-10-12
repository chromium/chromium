// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_recorder.h"

#include "base/feature_list.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/current_thread.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/external_metrics.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;
using ::metrics::SystemProfileProto;

// The delay period for the PersistentProto.
constexpr int kSaveDelayMs = 1000;

// The interval between chrome's collection of metrics logged from cros.
constexpr int kExternalMetricsIntervalMins = 10;

// Directory containing serialized event protos to read.
constexpr char kExternalMetricsDir[] = "/var/lib/metrics/structured/events";

}  // namespace

int StructuredMetricsRecorder::kMaxEventsPerUpload = 100;

char StructuredMetricsRecorder::kLocalStateEventsPath[] =
    "/var/lib/metrics/structured/chromium/events";
char StructuredMetricsRecorder::kUnsentLogsPath[] = "structured_metrics/events";

StructuredMetricsRecorder::StructuredMetricsRecorder(
    metrics::MetricsProvider* system_profile_provider)
    : StructuredMetricsRecorder(
          base::Milliseconds(kSaveDelayMs),
          system_profile_provider,
          base::FilePath(FILE_PATH_LITERAL(kLocalStateEventsPath))) {}

StructuredMetricsRecorder::StructuredMetricsRecorder(
    base::TimeDelta write_delay,
    metrics::MetricsProvider* system_profile_provider,
    const base::FilePath& local_state_events_store_path)
    : write_delay_(write_delay),
      system_profile_provider_(system_profile_provider),
      local_state_events_store_path_(local_state_events_store_path) {
  DCHECK(system_profile_provider_);
  Recorder::GetInstance()->AddObserver(this);
}

StructuredMetricsRecorder::~StructuredMetricsRecorder() {
  Recorder::GetInstance()->RemoveObserver(this);
  DCHECK(!IsInObserverList());
}

void StructuredMetricsRecorder::EnableRecording() {
  DCHECK(base::CurrentUIThread::IsSet());
  // Enable recording only if structured metrics' feature flag is enabled.
  recording_enabled_ =
      base::FeatureList::IsEnabled(features::kStructuredMetrics);
  if (external_metrics_.get() != nullptr) {
    external_metrics_->EnableRecording();
  }
  if (recording_enabled_) {
    CacheDisallowedProjectsSet();
  }
}

void StructuredMetricsRecorder::DisableRecording() {
  DCHECK(base::CurrentUIThread::IsSet());
  recording_enabled_ = false;
  if (external_metrics_.get() != nullptr) {
    external_metrics_->DisableRecording();
  }
  disallowed_projects_.clear();
}

void StructuredMetricsRecorder::ProvideUmaEventMetrics(
    ChromeUserMetricsExtension& uma_proto) {
  auto* structured_data = uma_proto.mutable_structured_data();

  // Add local_state events if available.
  if (can_provide_local_state_metrics()) {
    structured_data->mutable_events()->Swap(
        LocalStateEvents()->mutable_uma_events());
    LocalStateEvents()->clear_uma_events();
    local_state_events_proto().StartWrite();
  }

  // Add profile events if available.
  if (can_provide_profile_metrics()) {
    structured_data->mutable_events()->MergeFrom(ProfileEvents()->uma_events());
    ProfileEvents()->clear_uma_events();
    profile_events_proto().StartWrite();
  }

  LogUploadSizeBytes(structured_data->ByteSizeLong());
  LogNumEventsInUpload(structured_data->events_size());
}

void StructuredMetricsRecorder::ProvideEventMetrics(
    ChromeUserMetricsExtension& uma_proto) {
  if (!HasIndependentMetrics()) {
    return;
  }

  ProvideSystemProfile(uma_proto.mutable_system_profile());
  auto* structured_data = uma_proto.mutable_structured_data();

  // Add local_state events if available.
  if (can_provide_local_state_metrics()) {
    structured_data->mutable_events()->Swap(
        LocalStateEvents()->mutable_non_uma_events());
    LocalStateEvents()->clear_non_uma_events();
    local_state_events_proto().StartWrite();
  }

  // Add profile events if available.
  if (can_provide_profile_metrics()) {
    structured_data->mutable_events()->MergeFrom(
        ProfileEvents()->non_uma_events());
    ProfileEvents()->clear_non_uma_events();
    profile_events_proto().StartWrite();
  }

  // Record recorder metadata only if events were provided.
  if (can_provide_local_state_metrics() || can_provide_profile_metrics()) {
    LogNumEventsInUpload(structured_data->events_size());
    LogUploadSizeBytes(structured_data->ByteSizeLong());
    LogExternalMetricsScanInUpload(external_metrics_scans_);
    external_metrics_scans_ = 0;

    // Applies custom metadata providers.
    Recorder::GetInstance()->OnProvideIndependentMetrics(&uma_proto);
  }
}

void StructuredMetricsRecorder::InitializeKeyDataProvider(
    std::unique_ptr<KeyDataProvider> key_data_provider) {
  key_data_provider_ = std::move(key_data_provider);

  key_data_provider_->InitializeDeviceKey(
      base::BindOnce(&StructuredMetricsRecorder::OnLocalStateKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));

  local_state_events_ = std::make_unique<PersistentProto<EventsProto>>(
      base::FilePath(local_state_events_store_path_), write_delay_,
      base::BindOnce(&StructuredMetricsRecorder::OnLocalStateEventsRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&StructuredMetricsRecorder::LogWriteStatus,
                          weak_factory_.GetWeakPtr()));

  external_metrics_ = std::make_unique<ExternalMetrics>(
      base::FilePath(kExternalMetricsDir),
      base::Minutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsRecorder::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));

  if (recording_enabled_) {
    external_metrics_->EnableRecording();
  }

  // See DisableRecording for more information.
  if (purge_state_on_init_) {
    Purge();
    purge_state_on_init_ = false;
  }
}

bool StructuredMetricsRecorder::HasIndependentMetrics() {
  // If local_state metrics are unavailable, then no metrics should be available
  // since local_state metrics are the first thing initialized.
  if (!can_provide_local_state_metrics()) {
    return false;
  }

  // LocalState metrics.
  bool has_metrics = LocalStateEvents()->non_uma_events_size() != 0;
  if (can_provide_profile_metrics()) {
    has_metrics = has_metrics || (ProfileEvents()->non_uma_events_size() != 0);
  }
  return has_metrics;
}

bool StructuredMetricsRecorder::IsReadyToRecordLocalStateEvents() const {
  return init_state_.HasAll(
      InitState({InitValue::kLocalStateEventsDataLoaded,
                 InitValue::kLocalStateKeyDataInitialized}));
}

bool StructuredMetricsRecorder::IsReadyToRecordProfileEvents() const {
  return init_state_.HasAll(InitState{InitValue::kProfileEventsDataLoaded,
                                      InitValue::kProfileKeyDataInitialized});
}

EventsProto* StructuredMetricsRecorder::LocalStateEvents() {
  DCHECK(IsReadyToRecordLocalStateEvents());
  return local_state_events_->get();
}

EventsProto* StructuredMetricsRecorder::ProfileEvents() {
  DCHECK(IsReadyToRecordProfileEvents());
  return profile_events_->get();
}

void StructuredMetricsRecorder::OnLocalStateKeyDataInitialized() {
  DCHECK(base::CurrentUIThread::IsSet());
  init_state_.Put(InitValue::kLocalStateKeyDataInitialized);

  if (on_ready_callback_) {
    std::move(on_ready_callback_).Run();
  }
}

void StructuredMetricsRecorder::OnLocalStateEventsRead(ReadStatus status) {
  LogReadStatus(status);
  init_state_.Put(InitValue::kLocalStateEventsDataLoaded);
}

void StructuredMetricsRecorder::OnProfileKeyDataInitialized() {
  DCHECK(base::CurrentUIThread::IsSet());
  init_state_.Put(InitValue::kProfileKeyDataInitialized);
}

void StructuredMetricsRecorder::OnProfileEventsRead(ReadStatus status) {
  LogReadStatus(status);
  init_state_.Put(InitValue::kProfileEventsDataLoaded);
}

void StructuredMetricsRecorder::LogReadStatus(const ReadStatus status) {
  DCHECK(base::CurrentUIThread::IsSet());

  switch (status) {
    case ReadStatus::kOk:
    case ReadStatus::kMissing:
      break;
    case ReadStatus::kReadError:
      LogInternalError(StructuredMetricsError::kEventReadError);
      break;
    case ReadStatus::kParseError:
      LogInternalError(StructuredMetricsError::kEventParseError);
      break;
  }
}

void StructuredMetricsRecorder::LogWriteStatus(const WriteStatus status) {
  DCHECK(base::CurrentUIThread::IsSet());

  switch (status) {
    case WriteStatus::kOk:
      break;
    case WriteStatus::kWriteError:
      LogInternalError(StructuredMetricsError::kEventWriteError);
      break;
    case WriteStatus::kSerializationError:
      LogInternalError(StructuredMetricsError::kEventSerializationError);
      break;
  }
}

void StructuredMetricsRecorder::OnExternalMetricsCollected(
    const EventsProto& events) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (recording_enabled_) {
    local_state_events_.get()->get()->mutable_uma_events()->MergeFrom(
        events.uma_events());
    local_state_events_.get()->get()->mutable_non_uma_events()->MergeFrom(
        events.non_uma_events());

    // Only increment if new events were add.
    if (events.uma_events_size() || events.non_uma_events_size()) {
      external_metrics_scans_ += 1;
    }
  }
}

void StructuredMetricsRecorder::Purge() {
  if (key_data_provider_) {
    key_data_provider_->Purge();
  }

  if (IsReadyToRecordLocalStateEvents()) {
    local_state_events_->Purge();
  }
  if (IsReadyToRecordProfileEvents()) {
    profile_events_->Purge();
  }
}

void StructuredMetricsRecorder::OnProfileAdded(
    const base::FilePath& profile_path) {
  DCHECK(base::CurrentUIThread::IsSet());

  // We do not handle multiprofile, instead initializing with the state stored
  // in the first logged-in user's cryptohome. So if a second profile is added
  // we should ignore it.
  if (init_state_.Has(InitValue::kProfileAdded)) {
    return;
  }
  init_state_.Put(InitValue::kProfileAdded);

  key_data_provider_->InitializeProfileKey(
      profile_path,
      base::BindOnce(&StructuredMetricsRecorder::OnProfileKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));

  // TODO(b/296435910): Pointer to pre-login events can be cleaned up after the
  // first flush of the local_state events has occurred.
  profile_events_ = std::make_unique<PersistentProto<EventsProto>>(
      profile_path.Append(kUnsentLogsPath), write_delay_,
      base::BindOnce(&StructuredMetricsRecorder::OnProfileEventsRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&StructuredMetricsRecorder::LogWriteStatus,
                          weak_factory_.GetWeakPtr()));

  // See DisableRecording for more information.
  if (purge_state_on_init_) {
    Purge();
    purge_state_on_init_ = false;
  }
}

void StructuredMetricsRecorder::OnEventRecord(const Event& event) {
  DCHECK(base::CurrentUIThread::IsSet());

  // One more state for the EventRecordingState exists: kMetricsProviderMissing.
  // This is recorded in Recorder::Record.
  if (!recording_enabled_) {
    // Events should be ignored if recording is disabled.
    LogEventRecordingState(EventRecordingState::kRecordingDisabled);
    return;
  }
  // Only check is we can record local_state events since checking if profile
  // are ready to be recorded should be done in RecordEvent.
  else if (!IsReadyToRecordLocalStateEvents()) {
    // If keys have not loaded yet, then hold the data in memory until the
    // keys have been loaded.
    LogEventRecordingState(EventRecordingState::kProviderUninitialized);
    RecordEventBeforeInitialization(event);
    return;
  }

  RecordEvent(event);
  if (IsReadyToRecordProfileEvents()) {
    profile_events_->QueueWrite();
  } else {
    local_state_events_->QueueWrite();
  }

  test_callback_on_record_.Run();
}

absl::optional<int> StructuredMetricsRecorder::LastKeyRotation(
    const uint64_t project_name_hash) {
  DCHECK(base::CurrentUIThread::IsSet());
  // First check local_state key.
  if (!IsLocalStateKeyDataInitialized()) {
    return absl::nullopt;
  }
  KeyData* local_state_key_data = key_data_provider_->GetDeviceKeyData();
  absl::optional<int> local_state_day =
      local_state_key_data->LastKeyRotation(project_name_hash);
  if (local_state_day) {
    return local_state_day;
  }

  // Check profile keys if loaded.
  if (!IsProfileKeyDataInitialized()) {
    return absl::nullopt;
  }
  KeyData* profile_key_data = key_data_provider_->GetProfileKeyData();
  return profile_key_data->LastKeyRotation(project_name_hash);
}

void StructuredMetricsRecorder::OnReportingStateChanged(bool enabled) {
  DCHECK(base::CurrentUIThread::IsSet());

  // When reporting is enabled, OnRecordingEnabled is also called. Let that
  // handle enabling.
  if (enabled) {
    return;
  }

  // When reporting is disabled, OnRecordingDisabled is also called. Disabling
  // here is redundant but done for clarity.
  recording_enabled_ = false;

  // Delete keys and unsent logs. We need to handle two cases:
  //
  // 1. A profile hasn't been added yet and we can't delete the files
  //    immediately. In this case set |purge_state_on_init_| and let
  //    OnProfileAdded call Purge after initialization.
  //
  // 2. A profile has been added and so the backing PersistentProtos have been
  //    constructed. In this case just call Purge directly.
  //
  // Note that Purge will ensure the events are deleted from disk even if the
  // PersistentProto hasn't itself finished being read.
  if (init_state_.Empty()) {
    purge_state_on_init_ = true;
  } else {
    Purge();
  }
}

void StructuredMetricsRecorder::OnSystemProfileInitialized() {
  system_profile_initialized_ = true;
}

void StructuredMetricsRecorder::ProvideSystemProfile(
    SystemProfileProto* system_profile) {
  // Populate the proto if the system profile has been initialized and
  // have a system profile provider.
  // The field may be populated if ChromeOSMetricsProvider has already run.
  if (system_profile_initialized_) {
    system_profile_provider_->ProvideSystemProfileMetrics(system_profile);
  }
}

void StructuredMetricsRecorder::WriteNowForTest() {
  if (can_provide_local_state_metrics()) {
    local_state_events_->StartWrite();
  }
  if (can_provide_profile_metrics()) {
    profile_events_->StartWrite();
  }
}

void StructuredMetricsRecorder::SetExternalMetricsDirForTest(
    const base::FilePath& dir) {
  external_metrics_ = std::make_unique<ExternalMetrics>(
      dir, base::Minutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsRecorder::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));
}

void StructuredMetricsRecorder::SetOnReadyToRecord(base::OnceClosure callback) {
  on_ready_callback_ = std::move(callback);

  if (can_provide_local_state_metrics() && on_ready_callback_) {
    std::move(on_ready_callback_).Run();
  }
}

void StructuredMetricsRecorder::RecordEventBeforeInitialization(
    const Event& event) {
  DCHECK(!IsReadyToRecordLocalStateEvents());
  unhashed_events_.emplace_back(event.Clone());
}

void StructuredMetricsRecorder::RecordEvent(const Event& event) {
  DCHECK(IsReadyToRecordLocalStateEvents());
  if (!IsReadyToRecordLocalStateEvents()) {
    return;
  }

  KeyData* local_state_key_data = key_data_provider_->GetDeviceKeyData();

  // Validates the event. If valid, retrieve the metadata associated
  // with the event.
  auto maybe_project_validator =
      Recorder::GetInstance()->GetValidator()->GetProjectValidator(
          event.project_name());

  DCHECK(maybe_project_validator.has_value());
  if (!maybe_project_validator.has_value()) {
    return;
  }
  const auto* project_validator = maybe_project_validator.value();
  const auto maybe_event_validator =
      project_validator->GetEventValidator(event.event_name());
  DCHECK(maybe_event_validator.has_value());
  if (!maybe_event_validator.has_value()) {
    return;
  }
  const auto* event_validator = maybe_event_validator.value();

  if (!CanUploadProject(project_validator->project_hash())) {
    LogEventRecordingState(EventRecordingState::kProjectDisallowed);
    return;
  }

  // Trying to record a profile event before profile is available. Ignore the
  // event.
  if (project_validator->id_scope() == IdScope::kPerProfile &&
      !IsReadyToRecordProfileEvents()) {
    LogEventRecordingState(EventRecordingState::kProfileEventBeforeKeysLoaded);
    return;
  }

  // Load profile keys if available.
  absl::optional<KeyData*> profile_key_data = absl::nullopt;
  if (IsReadyToRecordProfileEvents()) {
    profile_key_data = key_data_provider_->GetProfileKeyData();
  }

  // Assign appropriate |persistent_events| to write to based on |event|. If a
  // profile has been loaded, then use that storage to store events going
  // forward.
  //
  // TODO(b/304586233): We only need a handle of |local_state_events_| until
  // events have been staged to be uploaded. Holding onto the two handles
  // forever is unnecessary.
  PersistentProto<EventsProto>* persistent_events =
      IsReadyToRecordProfileEvents() ? profile_events_.get()
                                     : local_state_events_.get();

  // The persistent proto contains two repeated fields, uma_events and
  // non_uma_events. uma_events is added to the ChromeUserMetricsExtension on a
  // call to ProvideCurrentSessionData, which is the standard UMA upload and
  // contains the UMA client_id. non_uma_events is added to the proto on a call
  // to ProvideIndependentMetrics, which is a separate upload that does _not_
  // contain the UMA client_id.
  //
  // We decide which field to add this event to based on the event's IdType.
  // kUmaId events should go in the UMA upload, and all others in the non-UMA
  // upload.
  StructuredEventProto* event_proto;
  if (project_validator->id_type() == IdType::kUmaId ||
      !IsIndependentMetricsUploadEnabled()) {
    event_proto = persistent_events->get()->add_uma_events();
  } else {
    event_proto = persistent_events->get()->add_non_uma_events();
  }

  event_proto->set_project_name_hash(project_validator->project_hash());

  // Sequence-related metadata.
  if (project_validator->event_type() ==
          StructuredEventProto_EventType_SEQUENCE &&
      base::FeatureList::IsEnabled(kEventSequenceLogging)) {
    auto* event_sequence_metadata =
        event_proto->mutable_event_sequence_metadata();

    event_sequence_metadata->set_reset_counter(
        event.event_sequence_metadata().reset_counter);
    event_sequence_metadata->set_system_uptime(
        event.recorded_time_since_boot().InMilliseconds());
    event_sequence_metadata->set_event_unique_id(
        base::HashMetricName(event.event_sequence_metadata().event_unique_id));

    int days_since_rotation =
        profile_key_data.value()
            ->LastKeyRotation(project_validator->project_hash())
            .value_or(0);
    event_sequence_metadata->set_client_id_rotation_weeks(days_since_rotation /
                                                          7);

    event_proto->set_device_project_id(
        local_state_key_data->Id(project_validator->project_hash(),
                                 project_validator->key_rotation_period()));
    if (can_provide_profile_metrics()) {
      event_proto->set_user_project_id(profile_key_data.value()->Id(
          project_validator->project_hash(),
          project_validator->key_rotation_period()));
    }
  }

  // Choose which KeyData to use for this event.
  KeyData* key_data;
  switch (project_validator->id_scope()) {
    case IdScope::kPerProfile:
      if (!profile_key_data.has_value()) {
        return;
      }
      key_data = profile_key_data.value();
      break;
    case IdScope::kPerDevice:
      // For event sequence, use the profile key for now to hash strings.
      //
      // TODO(crbug/1399632): Event sequence is considered a structured
      // metrics project. Once the client supports local_state/profile split of
      // events like structured metrics, remove this.
      if (project_validator->event_type() ==
          StructuredEventProto_EventType_SEQUENCE) {
        key_data = profile_key_data.value();
      } else {
        key_data = local_state_key_data;
      }
      break;
    default:
      // In case id_scope is uninitialized.
      NOTREACHED();
  }

  // Set the ID for this event, if any.
  switch (project_validator->id_type()) {
    case IdType::kProjectId:
      event_proto->set_profile_event_id(
          key_data->Id(project_validator->project_hash(),
                       project_validator->key_rotation_period()));
      break;
    case IdType::kUmaId:
      // TODO(crbug.com/1148168): Unimplemented.
      break;
    case IdType::kUnidentified:
      // Do nothing.
      break;
    default:
      // In case id_type is uninitialized.
      NOTREACHED();
      break;
  }

  // Set the event type. Do this with a switch statement to catch when the
  // event type is UNKNOWN or uninitialized.
  switch (project_validator->event_type()) {
    case StructuredEventProto_EventType_REGULAR:
    case StructuredEventProto_EventType_RAW_STRING:
    case StructuredEventProto_EventType_SEQUENCE:
      event_proto->set_event_type(project_validator->event_type());
      break;
    default:
      NOTREACHED();
      break;
  }
  event_proto->set_event_name_hash(event_validator->event_hash());

  // Set each metric's name hash and value.
  for (const auto& metric : event.metric_values()) {
    const std::string& metric_name = metric.first;
    const Event::MetricValue& metric_value = metric.second;

    // Validate that both name and metric type are valid structured metrics.
    // If a metric is invalid, then ignore the metric so that other valid
    // metrics are added to the proto.
    absl::optional<EventValidator::MetricMetadata> metadata =
        event_validator->GetMetricMetadata(metric_name);

    // Checks that the metrics defined are valid. If not valid, then the
    // metric will be ignored.
    bool is_valid =
        metadata.has_value() && metadata->metric_type == metric_value.type;
    DCHECK(is_valid);
    if (!is_valid) {
      continue;
    }

    StructuredEventProto::Metric* metric_proto = event_proto->add_metrics();
    int64_t metric_name_hash = metadata->metric_name_hash;
    metric_proto->set_name_hash(metric_name_hash);

    const auto& value = metric_value.value;
    switch (metadata->metric_type) {
      case Event::MetricType::kHmac:
        metric_proto->set_value_hmac(key_data->HmacMetric(
            project_validator->project_hash(), metric_name_hash,
            value.GetString(), project_validator->key_rotation_period()));
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
      // Not supported yet.
      case Event::MetricType::kInt:
      case Event::MetricType::kBoolean:
        break;
    }
  }

  // Log size information about the event.
  LogEventSerializedSizeBytes(event_proto->ByteSizeLong());

  Recorder::GetInstance()->OnEventRecorded(event_proto);
  LogEventRecordingState(EventRecordingState::kRecorded);
}

void StructuredMetricsRecorder::HashUnhashedEventsAndPersist() {
  LogNumEventsRecordedBeforeInit(unhashed_events_.size());

  while (!unhashed_events_.empty()) {
    RecordEvent(unhashed_events_.front());
    unhashed_events_.pop_front();
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

void StructuredMetricsRecorder::AddDisallowedProjectForTest(
    uint64_t project_name_hash) {
  disallowed_projects_.insert(project_name_hash);
}

bool StructuredMetricsRecorder::IsLocalStateKeyDataInitialized() {
  return init_state_.Has(InitValue::kLocalStateKeyDataInitialized);
}

bool StructuredMetricsRecorder::IsProfileKeyDataInitialized() {
  return init_state_.Has(InitValue::kProfileKeyDataInitialized);
}

void StructuredMetricsRecorder::SetLocalStateMetricsPathForTest(
    const base::FilePath& path) {
  local_state_events_store_path_ = path;
  init_state_.Remove(InitValue::kLocalStateEventsDataLoaded);

  local_state_events_ = std::make_unique<PersistentProto<EventsProto>>(
      base::FilePath(local_state_events_store_path_), write_delay_,
      base::BindOnce(&StructuredMetricsRecorder::OnLocalStateEventsRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&StructuredMetricsRecorder::LogWriteStatus,
                          weak_factory_.GetWeakPtr()));
}

void StructuredMetricsRecorder::SetLocalStateKeysPathForTest(
    const base::FilePath& path) {
  local_state_events_store_path_ = path;
  init_state_.Remove(InitValue::kLocalStateKeyDataInitialized);

  key_data_provider_->InitializeDeviceKey(
      base::BindOnce(&StructuredMetricsRecorder::OnLocalStateKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));
}

void StructuredMetricsRecorder::SetEventRecordCallbackForTest(
    base::RepeatingClosure callback) {
  test_callback_on_record_ = std::move(callback);
}

}  // namespace metrics::structured
