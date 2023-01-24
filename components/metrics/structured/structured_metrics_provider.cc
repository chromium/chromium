// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
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

// This is set carefully: metrics logs are stored in a queue of limited size,
// and are uploaded roughly every 30 minutes.
constexpr base::TimeDelta kMinIndependentMetricsInterval = base::Minutes(45);

// Directory containing serialized event protos to read.
constexpr char kExternalMetricsDir[] = "/var/lib/metrics/structured/events";

}  // namespace

int StructuredMetricsProvider::kMaxEventsPerUpload = 100;

char StructuredMetricsProvider::kProfileKeyDataPath[] =
    "structured_metrics/keys";

char StructuredMetricsProvider::kDeviceKeyDataPath[] =
    "/var/lib/metrics/structured/chromium/keys";

char StructuredMetricsProvider::kUnsentLogsPath[] = "structured_metrics/events";

StructuredMetricsProvider::StructuredMetricsProvider(
    base::raw_ptr<metrics::MetricsProvider> system_profile_provider)
    : StructuredMetricsProvider(base::FilePath(kDeviceKeyDataPath),
                                base::Milliseconds(kSaveDelayMs),
                                kMinIndependentMetricsInterval,
                                system_profile_provider) {}

StructuredMetricsProvider::StructuredMetricsProvider(
    const base::FilePath& device_key_path,
    base::TimeDelta min_independent_metrics_interval,
    base::TimeDelta write_delay,
    base::raw_ptr<metrics::MetricsProvider> system_profile_provider)
    : device_key_path_(device_key_path),
      write_delay_(write_delay),
      min_independent_metrics_interval_(min_independent_metrics_interval),
      system_profile_provider_(system_profile_provider) {
  DCHECK(system_profile_provider_);
  Recorder::GetInstance()->AddObserver(this);
}

StructuredMetricsProvider::~StructuredMetricsProvider() {
  Recorder::GetInstance()->RemoveObserver(this);
  DCHECK(!IsInObserverList());
}

void StructuredMetricsProvider::OnKeyDataInitialized() {
  DCHECK(base::CurrentUIThread::IsSet());

  ++init_count_;
  if (init_count_ == kTargetInitCount) {
    init_state_ = InitState::kInitialized;
    HashUnhashedEventsAndPersist();
  }
}

void StructuredMetricsProvider::OnRead(const ReadStatus status) {
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

  ++init_count_;
  if (init_count_ == kTargetInitCount) {
    init_state_ = InitState::kInitialized;
    HashUnhashedEventsAndPersist();
  }
}

void StructuredMetricsProvider::OnWrite(const WriteStatus status) {
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

void StructuredMetricsProvider::OnExternalMetricsCollected(
    const EventsProto& events) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (recording_enabled_) {
    events_.get()->get()->mutable_uma_events()->MergeFrom(events.uma_events());
    events_.get()->get()->mutable_non_uma_events()->MergeFrom(
        events.non_uma_events());
  }
}

void StructuredMetricsProvider::Purge() {
  DCHECK(events_ && profile_key_data_ && device_key_data_);
  events_->Purge();
  profile_key_data_->Purge();
  device_key_data_->Purge();
}

void StructuredMetricsProvider::OnProfileAdded(
    const base::FilePath& profile_path) {
  DCHECK(base::CurrentUIThread::IsSet());

  // We do not handle multiprofile, instead initializing with the state stored
  // in the first logged-in user's cryptohome. So if a second profile is added
  // we should ignore it. All init state beyond |InitState::kUninitialized| mean
  // a profile has already been added.
  if (init_state_ != InitState::kUninitialized) {
    return;
  }
  init_state_ = InitState::kProfileAdded;

  profile_key_data_ = std::make_unique<KeyData>(
      profile_path.Append(kProfileKeyDataPath), write_delay_,
      base::BindOnce(&StructuredMetricsProvider::OnKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));

  device_key_data_ = std::make_unique<KeyData>(
      base::FilePath(device_key_path_), write_delay_,
      base::BindOnce(&StructuredMetricsProvider::OnKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));

  events_ = std::make_unique<PersistentProto<EventsProto>>(
      profile_path.Append(kUnsentLogsPath), write_delay_,
      base::BindOnce(&StructuredMetricsProvider::OnRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&StructuredMetricsProvider::OnWrite,
                          weak_factory_.GetWeakPtr()));

  external_metrics_ = std::make_unique<ExternalMetrics>(
      base::FilePath(kExternalMetricsDir),
      base::Minutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsProvider::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));

  // See OnRecordingDisabled for more information.
  if (purge_state_on_init_) {
    Purge();
    purge_state_on_init_ = false;
  }
}

void StructuredMetricsProvider::OnEventRecord(const Event& event) {
  DCHECK(base::CurrentUIThread::IsSet());

  // One more state for the EventRecordingState exists: kMetricsProviderMissing.
  // This is recorded in Recorder::Record.
  if (!recording_enabled_) {
    // Events should be ignored if recording is disabled.
    LogEventRecordingState(EventRecordingState::kRecordingDisabled);
    return;
  } else if (init_state_ != InitState::kInitialized) {
    // If keys have not loaded yet, then hold the data in memory until the keys
    // have been loaded.
    LogEventRecordingState(EventRecordingState::kProviderUninitialized);
    RecordEventBeforeInitialization(event);
    return;
  } else {
    LogEventRecordingState(EventRecordingState::kRecorded);
  }

  DCHECK(profile_key_data_->is_initialized());
  DCHECK(device_key_data_->is_initialized());

  RecordEvent(event);

  events_->QueueWrite();
}

absl::optional<int> StructuredMetricsProvider::LastKeyRotation(
    const uint64_t project_name_hash) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (init_state_ != InitState::kInitialized) {
    return absl::nullopt;
  }
  DCHECK(profile_key_data_->is_initialized());
  DCHECK(device_key_data_->is_initialized());

  // |project_name_hash| could store its keys in either the profile or device
  // key data, so check both. As they cannot both contain the same name hash, at
  // most one will return a non-nullopt value.
  absl::optional<int> profile_day =
      profile_key_data_->LastKeyRotation(project_name_hash);
  absl::optional<int> device_day =
      device_key_data_->LastKeyRotation(project_name_hash);
  DCHECK(!(profile_day && device_day));
  return profile_day ? profile_day : device_day;
}

void StructuredMetricsProvider::OnRecordingEnabled() {
  DCHECK(base::CurrentUIThread::IsSet());
  // Enable recording only if structured metrics' feature flag is enabled.
  recording_enabled_ = base::FeatureList::IsEnabled(kStructuredMetrics);
}

void StructuredMetricsProvider::OnRecordingDisabled() {
  DCHECK(base::CurrentUIThread::IsSet());
  recording_enabled_ = false;
}

void StructuredMetricsProvider::OnReportingStateChanged(bool enabled) {
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
  if (init_state_ == InitState::kUninitialized) {
    purge_state_on_init_ = true;
  } else {
    Purge();
  }
}

void StructuredMetricsProvider::OnSystemProfileInitialized() {
  system_profile_initialized_ = true;
}

void StructuredMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (!recording_enabled_ || init_state_ != InitState::kInitialized) {
    return;
  }

  if (base::FeatureList::IsEnabled(kDelayUploadUntilHwid) &&
      !system_profile_initialized_) {
    return;
  }

  LogNumEventsInUpload(events_.get()->get()->uma_events_size());

  auto* structured_data = uma_proto->mutable_structured_data();
  structured_data->mutable_events()->Swap(
      events_.get()->get()->mutable_uma_events());
  events_.get()->get()->clear_uma_events();
  events_->StartWrite();
}

bool StructuredMetricsProvider::HasIndependentMetrics() {
  if (!IsIndependentMetricsUploadEnabled()) {
    return false;
  }

  if (!recording_enabled_ || init_state_ != InitState::kInitialized) {
    return false;
  }

  if (base::Time::Now() - last_provided_independent_metrics_ <
      min_independent_metrics_interval_) {
    return false;
  }

  if (base::FeatureList::IsEnabled(kDelayUploadUntilHwid) &&
      !system_profile_initialized_) {
    return false;
  }

  return events_.get()->get()->non_uma_events_size() != 0;
}

void StructuredMetricsProvider::ProvideIndependentMetrics(
    base::OnceCallback<void(bool)> done_callback,
    ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager*) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (!recording_enabled_ || init_state_ != InitState::kInitialized) {
    std::move(done_callback).Run(false);
    return;
  }

  if (base::FeatureList::IsEnabled(kDelayUploadUntilHwid) &&
      !system_profile_initialized_) {
    std::move(done_callback).Run(false);
    return;
  }

  last_provided_independent_metrics_ = base::Time::Now();

  LogNumEventsInUpload(events_.get()->get()->non_uma_events_size());

  // Independent metrics need to manually populate Chrome OS fields such as
  // full_hardware_class as ChromeOSMetricsProvider will not be called for
  // IndependentMetrics.
  ProvideSystemProfile(uma_proto->mutable_system_profile());

  auto* structured_data = uma_proto->mutable_structured_data();
  structured_data->mutable_events()->Swap(
      events_.get()->get()->mutable_non_uma_events());
  events_.get()->get()->clear_non_uma_events();
  events_->StartWrite();

  // Independent events should not be associated with the client_id, so clear
  // it.
  uma_proto->clear_client_id();
  // TODO(crbug/1052796): Remove the UMA timer code, which is currently used to
  // determine if it is worth to finalize independent logs in the background
  // by measuring the time it takes to execute the callback
  // MetricsService::PrepareProviderMetricsLogDone().
  SCOPED_UMA_HISTOGRAM_TIMER(
      "UMA.IndependentLog.StructuredMetricsProvider.FinalizeTime");
  std::move(done_callback).Run(true);
}

void StructuredMetricsProvider::ProvideSystemProfile(
    SystemProfileProto* system_profile) {
  // Populate the proto if the system profile has been intiailzed and
  // have a system profile provider.
  // The field may be populated if ChromeOSMetricsProvider has already run.
  if (system_profile_initialized_) {
    system_profile_provider_->ProvideSystemProfileMetrics(system_profile);
  }
}

void StructuredMetricsProvider::WriteNowForTest() {
  events_->StartWrite();
}

void StructuredMetricsProvider::SetExternalMetricsDirForTest(
    const base::FilePath& dir) {
  external_metrics_ = std::make_unique<ExternalMetrics>(
      dir, base::Minutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsProvider::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));
}

void StructuredMetricsProvider::RecordEventBeforeInitialization(
    const Event& event) {
  DCHECK_NE(init_state_, InitState::kInitialized);
  unhashed_events_.emplace_back(event.Clone());
}

void StructuredMetricsProvider::RecordEvent(const Event& event) {
  // Validates the event. If valid, retrieve the metadata associated
  // with the event.
  auto maybe_project_validator =
      validator::GetProjectValidator(event.project_name());

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
    event_proto->set_device_project_id(
        device_key_data_.get()->Id(project_validator->project_hash(),
                                   project_validator->key_rotation_period()));
    event_proto->set_user_project_id(
        profile_key_data_.get()->Id(project_validator->project_hash(),
                                    project_validator->key_rotation_period()));
  }

  // Choose which KeyData to use for this event.
  KeyData* key_data;
  switch (project_validator->id_scope()) {
    case IdScope::kPerProfile:
      key_data = profile_key_data_.get();
      break;
    case IdScope::kPerDevice:
      // For event sequence, use the profile key for now to hash strings.
      //
      // TODO(crbug/1399632): Event sequence is considered a structured metrics
      // project. Once the client supports device/profile split of events like
      // structured metrics, remove this.
      if (project_validator->event_type() ==
          StructuredEventProto_EventType_SEQUENCE) {
        key_data = profile_key_data_.get();
      } else {
        key_data = device_key_data_.get();
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

  // Set the event type. Do this with a switch statement to catch when the event
  // type is UNKNOWN or uninitialized.
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

    // Validate that both name and metric type are valid structured metrics. If
    // a metric is invalid, then ignore the metric so that other valid metrics
    // are added to the proto.
    absl::optional<EventValidator::MetricMetadata> metadata =
        event_validator->GetMetricMetadata(metric_name);

    // Checks that the metrics defined are valid. If not valid, then the metric
    // will be ignored.
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
}

void StructuredMetricsProvider::HashUnhashedEventsAndPersist() {
  LogNumEventsRecordedBeforeInit(unhashed_events_.size());

  while (!unhashed_events_.empty()) {
    RecordEvent(unhashed_events_.front());
    unhashed_events_.pop_front();
  }
}

}  // namespace metrics::structured
