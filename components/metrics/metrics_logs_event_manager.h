// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOGS_EVENT_MANAGER_H_
#define COMPONENTS_METRICS_METRICS_LOGS_EVENT_MANAGER_H_

#include "base/observer_list.h"
#include "base/strings/string_piece.h"

namespace metrics {

// TODO(crbug/1363747): Add unit tests for the various calls to the notify
// functions in ReportingService and UnsentLogStore.
class MetricsLogsEventManager {
 public:
  enum class LogEvent {
    // The log was staged.
    kLogStaged,
    // The log was discarded.
    kLogDiscarded,
    // The log was trimmed.
    kLogTrimmed,
    // The log is currently being uploaded.
    kLogUploading,
    // The log was successfully uploaded.
    kLogUploaded,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogCreated(base::StringPiece log_hash,
                              base::StringPiece log_data,
                              base::StringPiece log_timestamp) = 0;
    virtual void OnLogEvent(MetricsLogsEventManager::LogEvent event,
                            base::StringPiece log_hash,
                            base::StringPiece message) = 0;

   protected:
    Observer() = default;
    ~Observer() override = default;
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
  void NotifyLogCreated(base::StringPiece log_hash,
                        base::StringPiece log_data,
                        base::StringPiece log_timestamp);

  // Notifies observers that an event |event| occurred on the log associated
  // with |log_hash|. Optionally, a |message| can be associated with the event.
  // In particular, for |kLogDiscarded|, |message| is the reason the log was
  // discarded (e.g., log is ill-formed). For |kLogTrimmed|, |message| is the
  // reason why the log was trimmed (e.g., log is too large).
  void NotifyLogEvent(LogEvent event,
                      base::StringPiece log_hash,
                      base::StringPiece message = "");

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_EVENT_MANAGER_H_
