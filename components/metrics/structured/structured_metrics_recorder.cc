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
#include "components/metrics/metrics_features.h"
#include "components/metrics/structured/enums.h"
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/metrics/structured/external_metrics.h"
#endif
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "event_validator.h"
#include "project_validator.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;
using ::metrics::SystemProfileProto;

// The delay period for the PersistentProto.
constexpr int kSaveDelayMs = 1000;

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
// The interval between chrome's collection of metrics logged from cros.
constexpr int kExternalMetricsIntervalMins = 10;

// Directory containing serialized event protos to read.
constexpr char kExternalMetricsDir[] = "/var/lib/metrics/structured/events";
#endif

}  // namespace

int StructuredMetricsRecorder::kMaxEventsPerUpload = 100;

StructuredMetricsRecorder::StructuredMetricsRecorder(
    metrics::MetricsProvider* system_profile_provider)
    : StructuredMetricsRecorder(base::Milliseconds(kSaveDelayMs),
                                system_profile_provider) {}

StructuredMetricsRecorder::StructuredMetricsRecorder(
    base::TimeDelta write_delay,
    metrics::MetricsProvider* system_profile_provider)
    : write_delay_(write_delay),
      system_profile_provider_(system_profile_provider) {
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
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (external_metrics_.get() != nullptr) {
    external_metrics_->EnableRecording();
  }
#endif
  if (recording_enabled_) {
    CacheDisallowedProjectsSet();
  }
}

void StructuredMetricsRecorder::DisableRecording() {
  DCHECK(base::CurrentUIThread::IsSet());
  recording_enabled_ = false;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (external_metrics_.get() != nullptr) {
    external_metrics_->DisableRecording();
  }
#endif
  disallowed_projects_.clear();
}

void StructuredMetricsRecorder::ProvideUmaEventMetrics(
    ChromeUserMetricsExtension& uma_proto) {
  if (!can_provide_metrics()) {
    return;
  }

  auto* structured_data = uma_proto.mutable_structured_data();
  structured_data->mutable_events()->Swap(events()->mutable_uma_events());
  events()->clear_uma_events();
  proto().StartWrite();

  LogUploadSizeBytes(structured_data->ByteSizeLong());
}

void StructuredMetricsRecorder::ProvideEventMetrics(
    ChromeUserMetricsExtension& uma_proto) {
  if (!can_provide_metrics()) {
    return;
  }

  LogNumEventsInUpload(events_.get()->get()->non_uma_events_size());

  ProvideSystemProfile(uma_proto.mutable_system_profile());

  auto* structured_data = uma_proto.mutable_structured_data();
  structured_data->mutable_events()->Swap(events()->mutable_non_uma_events());
  events()->clear_non_uma_events();
  proto().StartWrite();

  LogUploadSizeBytes(structured_data->ByteSizeLong());
  LogExternalMetricsScanInUpload(external_metrics_scans_);
  external_metrics_scans_ = 0;

  // Applies custom metadata providers.
  Recorder::GetInstance()->OnProvideIndependentMetrics(&uma_proto);
}

void StructuredMetricsRecorder::InitializeKeyDataProvider(
    std::unique_ptr<KeyDataProvider> key_data_provider) {
  key_data_provider_ = std::move(key_data_provider);

  key_data_provider_->InitializeDeviceKey(
      base::BindOnce(&StructuredMetricsRecorder::OnKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));
}

void StructuredMetricsRecorder::OnKeyDataInitialized() {
  DCHECK(base::CurrentUIThread::IsSet());

  UpdateAndCheckInitState();
}

void StructuredMetricsRecorder::OnRead(const ReadStatus status) {
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

  UpdateAndCheckInitState();
}

void StructuredMetricsRecorder::OnWrite(const WriteStatus status) {
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
    events_.get()->get()->mutable_uma_events()->MergeFrom(events.uma_events());
    events_.get()->get()->mutable_non_uma_events()->MergeFrom(
        events.non_uma_events());

    // Only increment if new events were add.
    if (events.uma_events_size() || events.non_uma_events_size()) {
      external_metrics_scans_ += 1;
    }
  }
}

void StructuredMetricsRecorder::Purge() {
  // Only purge if the recorder has been initialized.
  if (!is_init_state(InitState::kInitialized)) {
    return;
  }
  DCHECK(IsDeviceKeyDataInitialized());
  DCHECK(IsProfileKeyDataInitialized());

  DCHECK(events_);
  events_->Purge();
  key_data_provider_->Purge();
}

void StructuredMetricsRecorder::OnProfileAdded(
    const base::FilePath& profile_path) {
  DCHECK(base::CurrentUIThread::IsSet());

  // We do not handle multiprofile, instead initializing with the state stored
  // in the first logged-in user's cryptohome. So if a second profile is added
  // we should ignore it. All init state beyond |InitState::kUninitialized|
  // mean a profile has already been added.
  if (init_state_ != InitState::kUninitialized) {
    return;
  }
  init_state_ = InitState::kProfileAdded;

  key_data_provider_->InitializeProfileKey(
      profile_path,
      base::BindOnce(&StructuredMetricsRecorder::OnKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));

  // The directory used to store unsent logs. Relative to the user's cryptohome.
  // This file is created by chromium.
  events_ = std::make_unique<PersistentProto<EventsProto>>(
      profile_path.Append(FILE_PATH_LITERAL("structured_metrics"))
          .Append(FILE_PATH_LITERAL("events")),
      write_delay_,
      base::BindOnce(&StructuredMetricsRecorder::OnRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&StructuredMetricsRecorder::OnWrite,
                          weak_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  external_metrics_ = std::make_unique<ExternalMetrics>(
      base::FilePath(kExternalMetricsDir),
      base::Minutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsRecorder::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));

  if (recording_enabled_) {
    external_metrics_->EnableRecording();
  }
#endif

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
  if (!recording_enabled_ && !CanForceRecord(event)) {
    // Events should be ignored if recording is disabled.
    LogEventRecordingState(EventRecordingState::kRecordingDisabled);
    return;
  } else if (init_state_ != InitState::kInitialized) {
    // If keys have not loaded yet, then hold the data in memory until the
    // keys have been loaded.
    LogEventRecordingState(EventRecordingState::kProviderUninitialized);
    RecordEventBeforeInitialization(event);
    return;
  }

  DCHECK(IsDeviceKeyDataInitialized());
  DCHECK(IsProfileKeyDataInitialized());

  RecordEvent(event);

  events_->QueueWrite();
  test_callback_on_record_.Run();
}

void StructuredMetricsRecorder::OnReportingStateChanged(bool enabled) {
  DCHECK(base::CurrentUIThread::IsSet());

  // When reporting is enabled, OnRecordingEnabled is also called. Let that
  // handle enabling.
  if (enabled) {
    return;
  }

  // Clean up any events that were recording during the pre-user.
  if (!recording_enabled_ && !enabled) {
    Purge();
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
  if (init_state_ == InitState::kUninitialized) {
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
  // The event proto may not be initialized yet. Check that the proto is ready
  // before attempting to write.
  if (can_provide_metrics()) {
    events_->StartWrite();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
void StructuredMetricsRecorder::SetExternalMetricsDirForTest(
    const base::FilePath& dir) {
  external_metrics_ = std::make_unique<ExternalMetrics>(
      dir, base::Minutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsRecorder::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));
}
#endif

void StructuredMetricsRecorder::SetOnReadyToRecord(base::OnceClosure callback) {
  on_ready_callback_ = std::move(callback);

  if (init_state_ == InitState::kInitialized) {
    std::move(on_ready_callback_).Run();
  }
}

void StructuredMetricsRecorder::RecordEventBeforeInitialization(
    const Event& event) {
  DCHECK_NE(init_state_, InitState::kInitialized);
  unhashed_events_.emplace_back(event.Clone());
}

void StructuredMetricsRecorder::RecordEvent(const Event& event) {
  DCHECK(IsDeviceKeyDataInitialized());
  DCHECK(IsProfileKeyDataInitialized());

  // Retrieve keys.
  KeyData* device_key_data = key_data_provider_->GetDeviceKeyData();
  KeyData* profile_key_data = key_data_provider_->GetProfileKeyData();

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

  // The |events_| persistent proto contains two repeated fields, uma_events
  // and non_uma_events. uma_events is added to the ChromeUserMetricsExtension
  // on a call to ProvideCurrentSessionData, which is the standard UMA upload
  // and contains the UMA client_id. non_uma_events is added to the proto on
  // a call to ProvideIndependentMetrics, which is a separate upload that does
  // _not_ contain the UMA client_id.
  //
  // We decide which field to add this event to based on the event's IdType.
  // kUmaId events should go in the UMA upload, and all others in the non-UMA
  // upload.
  StructuredEventProto* event_proto;
  if (project_validator->id_type() == IdType::kUmaId ||
      !IsIndependentMetricsUploadEnabled()) {
    event_proto = events_.get()->get()->add_uma_events();
  } else {
    event_proto = events_.get()->get()->add_non_uma_events();
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

    const int rotation_age =
        profile_key_data->GetKeyAgeInWeeks(project_validator->project_hash())
            .value_or(0);
    event_sequence_metadata->set_client_id_rotation_weeks(rotation_age);

    event_proto->set_device_project_id(
        device_key_data->Id(project_validator->project_hash(),
                            project_validator->key_rotation_period()));
    event_proto->set_user_project_id(
        profile_key_data->Id(project_validator->project_hash(),
                             project_validator->key_rotation_period()));
  }

  // Choose which KeyData to use for this event.
  KeyData* key_data;
  switch (project_validator->id_scope()) {
    case IdScope::kPerProfile:
      key_data = profile_key_data;
      break;
    case IdScope::kPerDevice:
      // For event sequence, use the profile key for now to hash strings.
      //
      // TODO(crbug/1399632): Event sequence is considered a structured
      // metrics project. Once the client supports device/profile split of
      // events like structured metrics, remove this.
      if (project_validator->event_type() ==
          StructuredEventProto_EventType_SEQUENCE) {
        key_data = profile_key_data;
      } else {
        key_data = device_key_data;
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

bool StructuredMetricsRecorder::IsDeviceKeyDataInitialized() {
  return key_data_provider_ && key_data_provider_->GetDeviceKeyData() &&
         key_data_provider_->GetDeviceKeyData()->is_initialized();
}

bool StructuredMetricsRecorder::IsProfileKeyDataInitialized() {
  return key_data_provider_ && key_data_provider_->GetProfileKeyData() &&
         key_data_provider_->GetProfileKeyData()->is_initialized();
}

void StructuredMetricsRecorder::UpdateAndCheckInitState() {
  ++init_count_;
  if (init_count_ == kTargetInitCount) {
    init_state_ = InitState::kInitialized;
    HashUnhashedEventsAndPersist();
    std::move(on_ready_callback_).Run();
  }
}

void StructuredMetricsRecorder::SetEventRecordCallbackForTest(
    base::RepeatingClosure callback) {
  test_callback_on_record_ = std::move(callback);
}

bool StructuredMetricsRecorder::CanForceRecord(const Event& event) const {
  const auto validators = GetEventValidators(event);
  if (!validators) {
    return false;
  }
  return validators->second->can_force_record();
}

absl::optional<std::pair<const ProjectValidator*, const EventValidator*>>
StructuredMetricsRecorder::GetEventValidators(const Event& event) const {
  auto maybe_project_validator =
      validator::Validators::Get()->GetProjectValidator(event.project_name());

  DCHECK(maybe_project_validator.has_value());
  if (!maybe_project_validator.has_value()) {
    return {};
  }
  const auto* project_validator = maybe_project_validator.value();
  const auto maybe_event_validator =
      project_validator->GetEventValidator(event.event_name());
  DCHECK(maybe_event_validator.has_value());
  if (!maybe_event_validator.has_value()) {
    return {};
  }
  const auto* event_validator = maybe_event_validator.value();
  return std::make_pair(project_validator, event_validator);
}

}  // namespace metrics::structured
