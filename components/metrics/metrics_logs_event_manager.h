// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOGS_EVENT_MANAGER_H_
#define COMPONENTS_METRICS_METRICS_LOGS_EVENT_MANAGER_H_

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/metrics/metrics_log.h"

namespace metrics {

// TODO(crbug.com/40238818): Add unit tests for the various calls to the notify
// functions in ReportingService and UnsentLogStore.
class MetricsLogsEventManager {
 public:
  enum class LogEvent {
    // The log was staged (queued to be uploaded).
    kLogStaged,
    // The log was discarded.
    kLogDiscarded,
    // The log was trimmed.
    kLogTrimmed,
    // The log has been sent out and is currently being uploaded.
    kLogUploading,
    // The log was successfully uploaded.
    kLogUploaded,
    // The log was created.
    kLogCreated,
  };

  enum class CreateReason {
    kUnknown,
    // The log is a periodic log, which are created at regular intervals.
    kPeriodic,
    // The log was created due to the UMA/UKM service shutting down.
    kServiceShutdown,
    // The log was loaded from a previous session.
    kLoadFromPreviousSession,
    // The log was created due to the browser being backgrounded.
    kBackgrounded,
    // The log was created due to the browser being foregrounded.
    kForegrounded,
    // The log was created due to a new alternate ongoing log store being set.
    kAlternateOngoingLogStoreSet,
    // The log was created due to the alternate ongoing log store being unset.
    kAlternateOngoingLogStoreUnset,
    // The log was created due to the previous session having stability metrics
    // to report.
    kStability,
    // The log was fully created and provided by a metrics provider.
    kIndependent,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogCreated(std::string_view log_hash,
                              std::string_view log_data,
                              std::string_view log_timestamp,
                              CreateReason reason) = 0;
    virtual void OnLogEvent(MetricsLogsEventManager::LogEvent event,
                            std::string_view log_hash,
                            std::string_view message) = 0;
    virtual void OnLogType(std::optional<MetricsLog::LogType> log_type) {}

   protected:
    Observer() = default;
    ~Observer() override = default;
  };

  // Helper class used to indicate that UMA logs created while an instance of
  // this class is in scope are of a certain type. Only one instance of this
  // class should exist at a time.
  class ScopedNotifyLogType {
   public:
    ScopedNotifyLogType(MetricsLogsEventManager* logs_event_manager,
                        MetricsLog::LogType log_type);

    ScopedNotifyLogType(const ScopedNotifyLogType& other) = delete;
    ScopedNotifyLogType& operator=(const ScopedNotifyLogType& other) = delete;

    ~ScopedNotifyLogType();

   private:
    const raw_ptr<MetricsLogsEventManager> logs_event_manager_;

    // Used to ensure that only one instance of this class exists at a time.
    static bool instance_exists_;
  };

  MetricsLogsEventManager();

  MetricsLogsEventManager(const MetricsLogsEventManager&) = delete;
  MetricsLogsEventManager& operator=(const MetricsLogsEventManager&) = delete;

  ~MetricsLogsEventManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies observers that a log was newly created and is now known by the
  // metrics service. This may occur when closing a log, or when loading a log
  // from persistent storage. |log_hash| is the SHA1 hash of the log data, used
  // to uniquely identify the log. This hash may be re-used to notify that an
  // event occurred on the log (e.g., the log was trimmed, uploaded, etc.). See
  // NotifyLogEvent(). |log_data| is the compressed serialized log protobuf
  // (see UnsentLogStore::LogInfo for more details on the compression).
  // |log_timestamp| is the time at which the log was closed.
  void NotifyLogCreated(std::string_view log_hash,
                        std::string_view log_data,
                        std::string_view log_timestamp,
                        CreateReason reason);

  // Notifies observers that an event |event| occurred on the log associated
  // with |log_hash|. Optionally, a |message| can be associated with the event.
  // In particular, for |kLogDiscarded|, |message| is the reason the log was
  // discarded (e.g., log is ill-formed). For |kLogTrimmed|, |message| is the
  // reason why the log was trimmed (e.g., log is too large).
  void NotifyLogEvent(LogEvent event,
                      std::string_view log_hash,
                      std::string_view message = "");

  // Notifies observers that logs that are created after this function is called
  // are of the type |log_type|. This should only be used in UMA. This info is
  // not passed through NotifyLogCreated() because the concept of a log type
  // only exists in UMA, and this class is intended to be re-used across
  // different metrics collection services (e.g., UKM).
  // Note: Typically, this should not be called directly. Consider using
  // ScopedNotifyLogType.
  void NotifyLogType(std::optional<MetricsLog::LogType> log_type);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOGS_EVENT_MANAGER_H_
