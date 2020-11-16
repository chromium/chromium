// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/writeable_pref_store.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {
namespace structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;
using PrefReadError = ::PersistentPrefStore::PrefReadError;

constexpr char kAssociatedEventsKey[] = "associated";
constexpr char kIndependentEventsKey[] = "independent";

}  // namespace

int StructuredMetricsProvider::kMaxEventsPerUpload = 100;

char StructuredMetricsProvider::kStorageFileName[] = "structured_metrics.json";

StructuredMetricsProvider::StructuredMetricsProvider() = default;

StructuredMetricsProvider::~StructuredMetricsProvider() {
  if (storage_)
    storage_->RemoveObserver(this);
  if (recording_enabled_)
    Recorder::GetInstance()->RemoveObserver(this);
  DCHECK(!IsInObserverList());
}

StructuredMetricsProvider::PrefStoreErrorDelegate::PrefStoreErrorDelegate() =
    default;

StructuredMetricsProvider::PrefStoreErrorDelegate::~PrefStoreErrorDelegate() =
    default;

void StructuredMetricsProvider::PrefStoreErrorDelegate::OnError(
    PrefReadError error) {
  LogPrefReadError(error);
}

base::Value* StructuredMetricsProvider::GetEventsList(
    EventBase::IdentifierType type) {
  // Ensure the events key exists. The "events" key was a list of event objects,
  // and is now a dict of lists. Migrate to the new layout if needed.
  base::Value* events = nullptr;
  if (!storage_->GetMutableValue("events", &events)) {
    storage_->SetValue(
        "events", std::make_unique<base::Value>(base::Value::Type::DICTIONARY),
        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    storage_->GetMutableValue("events", &events);
  } else if (events->is_list()) {
    auto new_events =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    // All existing events have previously been associated with the UMA
    // client_id, so move them to the associated events list.
    new_events->SetKey(kAssociatedEventsKey, events->Clone());
    storage_->SetValue("events", std::move(new_events),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    storage_->GetMutableValue("events", &events);
  }

  DCHECK(events->is_dict());

  // Choose the key for |type|, ensure the list Value actually exists, and
  // return it.
  base::StringPiece list_key;
  switch (type) {
    case EventBase::IdentifierType::kUmaId:
      list_key = base::StringPiece(kAssociatedEventsKey);
      break;
    case EventBase::IdentifierType::kProjectId:
    case EventBase::IdentifierType::kUnidentified:
      list_key = base::StringPiece(kIndependentEventsKey);
      break;
  }

  base::Value* events_list = events->FindKey(list_key);
  if (events_list) {
    return events_list;
  } else {
    base::Value events_list(base::Value::Type::LIST);
    return events->SetKey(list_key, std::move(events_list));
  }
}

void StructuredMetricsProvider::OnRecord(const EventBase& event) {
  // Records the information in |event|, to be logged to UMA on the next call to
  // ProvideCurrentSessionData. Should only be called from the browser UI
  // sequence.

  // One more state for the EventRecordingState exists: kMetricsProviderMissing.
  // This is recorded in Recorder::Record.
  if (!recording_enabled_) {
    LogEventRecordingState(EventRecordingState::kRecordingDisabled);
  } else if (!initialized_) {
    LogEventRecordingState(EventRecordingState::kProviderUninitialized);
  } else {
    LogEventRecordingState(EventRecordingState::kRecorded);
  }

  if (!recording_enabled_ || !initialized_)
    return;

  // Make a list of metrics.
  base::Value metrics(base::Value::Type::LIST);
  for (const auto& metric : event.metrics()) {
    base::Value name_value(base::Value::Type::DICTIONARY);
    name_value.SetStringKey("name", base::NumberToString(metric.name_hash));

    if (metric.type == EventBase::MetricType::kString) {
      // Store hashed values as strings, because the JSON parser only retains 53
      // bits of precision for ints. This would corrupt the hashes.
      name_value.SetStringKey(
          "value", base::NumberToString(key_data_->HashForEventMetric(
                       event.project_name_hash(), metric.name_hash,
                       metric.string_value)));
    } else if (metric.type == EventBase::MetricType::kInt) {
      name_value.SetIntKey("value", metric.int_value);
    }

    metrics.Append(std::move(name_value));
  }

  // Create an event value containing the metrics, the event name hash, and the
  // ID that will eventually be used as the profile_event_id of this event.
  base::Value event_value(base::Value::Type::DICTIONARY);
  event_value.SetStringKey("name", base::NumberToString(event.name_hash()));
  event_value.SetStringKey(
      "id",
      base::NumberToString(key_data_->UserEventId(event.project_name_hash())));
  event_value.SetKey("metrics", std::move(metrics));

  // Add the event to |storage_|.
  // TODO(crbug.com/1016655): Choose the event list based on the identifier type
  // of the event subclass.
  GetEventsList(EventBase::IdentifierType::kUmaId)
      ->Append(std::move(event_value));
}

void StructuredMetricsProvider::OnProfileAdded(
    const base::FilePath& profile_path) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (initialized_)
    return;

  storage_ = new JsonPrefStore(
      profile_path.Append(StructuredMetricsProvider::kStorageFileName));
  storage_->AddObserver(this);

  // |storage_| takes ownership of the error delegate.
  storage_->ReadPrefsAsync(new PrefStoreErrorDelegate());
}

void StructuredMetricsProvider::OnInitializationCompleted(const bool success) {
  if (!success)
    return;
  DCHECK(!storage_->ReadOnly());
  key_data_ = std::make_unique<internal::KeyData>(storage_.get());
  initialized_ = true;
}

void StructuredMetricsProvider::OnRecordingEnabled() {
  DCHECK(base::CurrentUIThread::IsSet());
  if (!recording_enabled_)
    Recorder::GetInstance()->AddObserver(this);
  recording_enabled_ = true;
}

void StructuredMetricsProvider::OnRecordingDisabled() {
  DCHECK(base::CurrentUIThread::IsSet());
  if (recording_enabled_)
    Recorder::GetInstance()->RemoveObserver(this);
  recording_enabled_ = false;

  // Clear the cache of unsent logs. Either |storage_| or its "events" key can
  // be nullptr if OnRecordingDisabled is called before initialization is
  // complete. In that case, there are no cached events to clear. See the class
  // comment in the header for more details on the initialization process.
  //
  // "events" was migrated from a list to a dict of lists. Handle both cases in
  // case recording is disabled before initialization can complete the
  // migration.
  base::Value* events = nullptr;
  if (storage_ && storage_->GetMutableValue("events", &events)) {
    if (events->is_list()) {
      events->ClearList();
    } else if (events->is_dict()) {
      events->DictClear();
    }
  }
}

void StructuredMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK(base::CurrentUIThread::IsSet());
  if (!recording_enabled_ || !initialized_)
    return;

  // TODO(crbug.com/1016655): Memory usage UMA metrics for unsent logs would be
  // useful as a canary for performance issues. base::Value::EstimateMemoryUsage
  // perhaps.

  base::Value* events = GetEventsList(EventBase::IdentifierType::kUmaId);
  for (const auto& event : events->GetList()) {
    auto* event_proto = uma_proto->add_structured_event();

    uint64_t event_name_hash;
    if (!base::StringToUint64(event.FindKey("name")->GetString(),
                              &event_name_hash)) {
      LogInternalError(StructuredMetricsError::kFailedUintConversion);
      continue;
    }
    event_proto->set_event_name_hash(event_name_hash);

    uint64_t user_event_id;
    if (!base::StringToUint64(event.FindKey("id")->GetString(),
                              &user_event_id)) {
      LogInternalError(StructuredMetricsError::kFailedUintConversion);
      continue;
    }
    event_proto->set_profile_event_id(user_event_id);

    for (const auto& metric : event.FindKey("metrics")->GetList()) {
      auto* metric_proto = event_proto->add_metrics();
      uint64_t name_hash;
      if (!base::StringToUint64(metric.FindKey("name")->GetString(),
                                &name_hash)) {
        LogInternalError(StructuredMetricsError::kFailedUintConversion);
        continue;
      }
      metric_proto->set_name_hash(name_hash);

      const auto* value = metric.FindKey("value");
      if (value->is_string()) {
        uint64_t hmac;
        if (!base::StringToUint64(value->GetString(), &hmac)) {
          LogInternalError(StructuredMetricsError::kFailedUintConversion);
          continue;
        }
        metric_proto->set_value_hmac(hmac);
      } else if (value->is_int()) {
        metric_proto->set_value_int64(value->GetInt());
      }
    }
  }

  LogNumEventsInUpload(events->GetList().size());
  events->ClearList();
}

void StructuredMetricsProvider::CommitPendingWriteForTest() {
  storage_->CommitPendingWrite();
}

}  // namespace structured
}  // namespace metrics
