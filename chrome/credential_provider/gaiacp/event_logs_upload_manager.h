// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_EVENT_LOGS_UPLOAD_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_EVENT_LOGS_UPLOAD_MANAGER_H_

#include <string>

#include "base/values.h"
#include "base/win/windows_types.h"
#include "url/gurl.h"

namespace credential_provider {

// Manager used to handle requests to upload GCPW event viewer logs
// to GEM cloud storage.
class EventLogsUploadManager {
 public:
  // Get the Event log manager instance.
  static EventLogsUploadManager* Get();

  // Upload event viewer logs to GEM cloud storage using |access_token| for
  // authentication and authorization.
  HRESULT UploadEventViewerLogs(const std::string& access_token);

  // Get the URL of GCPW service for HTTP request for uploading event
  // viewer logs.
  GURL GetGcpwServiceUploadEventViewerLogsUrl();

  // Structure to hold the information read for each event log entry.
  struct EventLogEntry {
    struct TimeStamp {
      uint64_t seconds;
      uint32_t nanos;
      TimeStamp() : seconds(0), nanos(0) {}
      TimeStamp(uint64_t seconds, uint32_t nanos)
          : seconds(seconds), nanos(nanos) {}
    };

    // Event Record ID of the event log entry.
    uint64_t event_id;

    // System time-stamp of when log was written into the event log.
    TimeStamp created_ts;

    // The data portion of the event log.
    std::wstring data;

    // Severity level of the log entry.
    uint32_t severity_level;

    EventLogEntry() : event_id(0), severity_level(0) {}
    EventLogEntry(uint64_t id,
                  const TimeStamp& ts,
                  std::wstring data,
                  uint32_t level)
        : event_id(id), created_ts(ts), data(data), severity_level(level) {}

    // Converts to dictionary in a base::Value.
    base::Value::Dict ToValue() const;
  };

 protected:
  // Returns the storage used for the instance pointer.
  static EventLogsUploadManager** GetInstanceStorage();

  EventLogsUploadManager();
  virtual ~EventLogsUploadManager();

  HRESULT upload_status_;
  uint64_t num_event_logs_uploaded_;

 private:
  // Makes the upload HTTP request using the provided |access_token| for
  // the upload chunk with id |chunk_id| and the log entries specified
  // as a list in |log_entries|.
  HRESULT MakeUploadLogChunkRequest(
      const std::string& access_token,
      uint64_t chunk_id,
      std::unique_ptr<base::Value::List> log_entries);
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_EVENT_LOGS_UPLOAD_MANAGER_H_
