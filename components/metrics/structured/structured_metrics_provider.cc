// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/external_metrics.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {
namespace structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;

// The delay period for the PersistentProto.
constexpr int kSaveDelayMs = 1000;

// The interval between chrome's collection of metrics logged from cros.
constexpr int kExternalMetricsIntervalMins = 10;

// The minimum waiting time between successive deliveries of independent metrics
// to the metrics service via ProvideIndependentMetrics. This is set carefully:
// metrics logs are stored in a queue of limited size, and are uploaded roughly
// every 30 minutes.
constexpr base::TimeDelta kMinIndependentMetricsInterval =
    base::TimeDelta::FromMinutes(45);

// Directory containing serialized event protos to read.
constexpr char kExternalMetricsDir[] = "/var/lib/metrics/structured/events";

}  // namespace

int StructuredMetricsProvider::kMaxEventsPerUpload = 100;

char StructuredMetricsProvider::kStorageDirectory[] = "structured_metrics";

StructuredMetricsProvider::StructuredMetricsProvider() {
  Recorder::GetInstance()->AddObserver(this);
}

StructuredMetricsProvider::~StructuredMetricsProvider() {
  Recorder::GetInstance()->RemoveObserver(this);
  DCHECK(!IsInObserverList());
}

void StructuredMetricsProvider::OnExternalMetricsCollected(
    const EventsProto& events) {
  DCHECK(base::CurrentUIThread::IsSet());
  events_.get()->get()->mutable_uma_events()->MergeFrom(events.uma_events());
  events_.get()->get()->mutable_non_uma_events()->MergeFrom(
      events.non_uma_events());
}

void StructuredMetricsProvider::OnKeyDataInitialized() {
  DCHECK(base::CurrentUIThread::IsSet());

  switch (init_state_) {
    case InitState::kProfileAdded:
      init_state_ = InitState::kKeysInitialized;
      break;
    case InitState::kEventsInitialized:
      init_state_ = InitState::kInitialized;
      break;
    default:
      NOTREACHED();
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

  switch (init_state_) {
    case InitState::kProfileAdded:
      init_state_ = InitState::kEventsInitialized;
      break;
    case InitState::kKeysInitialized:
      init_state_ = InitState::kInitialized;
      break;
    default:
      NOTREACHED();
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

void StructuredMetricsProvider::OnProfileAdded(
    const base::FilePath& profile_path) {
  DCHECK(base::CurrentUIThread::IsSet());

  // We do not handle multiprofile, instead initializing with the state stored
  // in the first logged-in user's cryptohome. So if a second profile is added
  // we should ignore it. All init state beyond |InitState::kUninitialized| mean
  // a profile has already been added.
  if (init_state_ != InitState::kUninitialized)
    return;
  init_state_ = InitState::kProfileAdded;

  const auto storage_directory = profile_path.Append(kStorageDirectory);
  const auto save_delay = base::TimeDelta::FromMilliseconds(kSaveDelayMs);
  key_data_ = std::make_unique<KeyData>(
      storage_directory.Append("keys"), save_delay,
      base::BindOnce(&StructuredMetricsProvider::OnKeyDataInitialized,
                     weak_factory_.GetWeakPtr()));
  events_ = std::make_unique<PersistentProto<EventsProto>>(
      storage_directory.Append("events"), save_delay,
      base::BindOnce(&StructuredMetricsProvider::OnRead,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&StructuredMetricsProvider::OnWrite,
                          weak_factory_.GetWeakPtr()));

  external_metrics_ = std::make_unique<ExternalMetrics>(
      base::FilePath(kExternalMetricsDir),
      base::TimeDelta::FromMinutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsProvider::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));

  // See OnRecordingDisabled for more information.
  if (wipe_events_on_init_) {
    events_->Wipe();
  }
}

void StructuredMetricsProvider::OnRecord(const EventBase& event) {
  DCHECK(base::CurrentUIThread::IsSet());

  // One more state for the EventRecordingState exists: kMetricsProviderMissing.
  // This is recorded in Recorder::Record.
  if (!recording_enabled_) {
    LogEventRecordingState(EventRecordingState::kRecordingDisabled);
  } else if (init_state_ != InitState::kInitialized) {
    LogEventRecordingState(EventRecordingState::kProviderUninitialized);
  } else {
    LogEventRecordingState(EventRecordingState::kRecorded);
  }

  if (!recording_enabled_ || init_state_ != InitState::kInitialized)
    return;

  DCHECK(key_data_->is_initialized());

  // TODO(crbug.com/1148168): We are transitioning to new upload behaviour for
  // non-client_id-identified metrics. See structured_metrics_features.h for
  // more information. If IsIndependentMetricsUploadEnabled below returns false,
  // this reverts to the old behaviour. The call can be removed once we are
  // confident with the change.

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
  if (event.id_type() == EventBase::IdType::kUmaId ||
      !IsIndependentMetricsUploadEnabled()) {
    event_proto = events_.get()->get()->add_uma_events();
  } else {
    event_proto = events_.get()->get()->add_non_uma_events();
  }

  event_proto->set_profile_event_id(key_data_->Id(event.project_name_hash()));
  event_proto->set_event_name_hash(event.name_hash());
  for (const auto& metric : event.metrics()) {
    auto* metric_proto = event_proto->add_metrics();
    metric_proto->set_name_hash(metric.name_hash);

    switch (metric.type) {
      case EventBase::MetricType::kInt:
        metric_proto->set_value_int64(metric.int_value);
        break;
      case EventBase::MetricType::kString:
        const int64_t hmac = key_data_->HmacMetric(
            event.project_name_hash(), metric.name_hash, metric.string_value);
        metric_proto->set_value_hmac(hmac);
        break;
    }
  }

  events_->QueueWrite();
}

void StructuredMetricsProvider::OnRecordingEnabled() {
  DCHECK(base::CurrentUIThread::IsSet());
  // Enable recording only if structured metrics' feature flag is enabled.
  recording_enabled_ = base::FeatureList::IsEnabled(kStructuredMetrics);
}

void StructuredMetricsProvider::OnRecordingDisabled() {
  DCHECK(base::CurrentUIThread::IsSet());
  recording_enabled_ = false;

  // Delete the cache of unsent logs. We need to handle two cases:
  //
  // 1. A profile has been added and so |events_| has been constructed. In this
  //    case just call Wipe.
  //
  // 2. A profile hasn't been added and |events_| is nullptr. In this case set
  //    |wipe_events_on_init_| and let OnProfileAdded call Wipe after |events_|
  //    is initialized.
  //
  // Note that Wipe will ensure the events are deleted from disk even if the
  // PersistentProto hasn't itself finished initializing.
  if (events_) {
    events_->Wipe();
  } else {
    wipe_events_on_init_ = true;
  }
}

void StructuredMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (!recording_enabled_ || init_state_ != InitState::kInitialized) {
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
      kMinIndependentMetricsInterval) {
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

  last_provided_independent_metrics_ = base::Time::Now();

  LogNumEventsInUpload(events_.get()->get()->non_uma_events_size());

  auto* structured_data = uma_proto->mutable_structured_data();
  structured_data->mutable_events()->Swap(
      events_.get()->get()->mutable_non_uma_events());
  events_.get()->get()->clear_non_uma_events();
  events_->StartWrite();

  // Independent events should not be associated with the client_id, so clear
  // it.
  uma_proto->clear_client_id();
  std::move(done_callback).Run(true);
}

void StructuredMetricsProvider::WriteNowForTest() {
  events_->StartWrite();
}

void StructuredMetricsProvider::SetExternalMetricsDirForTest(
    const base::FilePath& dir) {
  external_metrics_ = std::make_unique<ExternalMetrics>(
      dir, base::TimeDelta::FromMinutes(kExternalMetricsIntervalMins),
      base::BindRepeating(
          &StructuredMetricsProvider::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));
}

}  // namespace structured
}  // namespace metrics
