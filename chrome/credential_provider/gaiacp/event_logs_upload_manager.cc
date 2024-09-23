// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/event_logs_upload_manager.h"

#include <windows.h>

#include <winevt.h>

#include <memory>
#include <unordered_map>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/credential_provider/gaiacp/event_logging_api_manager.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {
namespace {

// HTTP endpoint on the GCPW service to upload the event viewer logs.
const char kGcpwServiceUploadEventLogsPath[] = "/v1/uploadEventViewerLogs";

// Default timeout when trying to make requests to the GCPW service.
const base::TimeDelta kDefaultUploadLogsRequestTimeout =
    base::Milliseconds(12000);

// Parameter names that are used in the JSON payload of the requests.
const char kRequestSerialNumberParameterName[] = "device_serial_number";
const char kRequestMachineGuidParameterName[] = "machine_guid";
const char kRequestChunkIdParameterName[] = "chunk_id";
const char kRequestLogEntriesParameterName[] = "log_entries";
const char kEventLogEventIdParameterName[] = "event_id";
const char kEventLogTimeStampParameterName[] = "created_ts";
const char kEventLogDataParameterName[] = "data";
const char kEventLogSeverityLevelParameterName[] = "severity_level";
const char kEventLogTimeStampSecondsParameterName[] = "seconds";
const char kEventLogTimeStampNanosParameterName[] = "nanos";

// Registry key where the the last uploaded event log record id is stored.
const wchar_t kEventLogUploadLastUploadedIdRegKey[] = L"last_upload_log_id";

// Registry key where the highest severity of logs to upload is stored.
const wchar_t kEventLogUploadLevelRegKey[] = L"log_upload_level";

// Default log upload severity level (4=Information).
constexpr DWORD kDefaultUploadLogLevel = 4;

// Number of events to read from event log API at one time.
constexpr int kDefaultNumberOfEventsToRead = 10;

// Maximum number of upload requests to make per upload invocation.
constexpr int kMaxAllowedNumberOfUploadRequests = 5;

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 3;

// Maximum size of the log entries payload in bytes per HTTP request.
// TODO (crbug.com/1043195): Change this to use an experiment flag once an
// experiment framework for GCPW is available.
constexpr size_t kMaxPayloadSizeOfLogEntries = 512 * 1024;  // 512 KB.

// Name that GCPW uses to publish events into the Windows event log.
constexpr wchar_t kEventLogPublisherName[] = L"GCPW";

// XPath query template used to query for GCPW events in the event log created
// within the last 30 days (2592000000 milliseconds) with the event logging
// API.
constexpr wchar_t kEventLogQueryTemplateStr[] =
    L"<QueryList>"
    L"  <Query Id='0' Path='Application'>"
    L"    <Select Path='Application'>* [System[Provider[@Name='GCPW'] and "
    L"(EventRecordID &gt;={event_id}) and TimeCreated[timediff(@SystemTime) "
    L"&lt;= 2592000000]]]</Select>"
    L"  </Query>"
    L"</QueryList>";

// List of paths whose values are read from the event log.
constexpr const wchar_t* kEventLogValuePaths[] = {
    L"Event/System/EventRecordID", L"Event/System/TimeCreated/@SystemTime"};

// Mapping from the string values of log levels to their integral values.
const std::unordered_map<std::wstring, uint32_t>
    kEventLogLevelStrToIntValueMap = {{L"Critical", 1},
                                      {L"Error", 2},
                                      {L"Warning", 3},
                                      {L"Information", 4}};

// Helper class to read event log entries sequentially through the Windows
// event logging API.
class EventLogReader {
 public:
  EventLogReader()
      : results_handle_(nullptr),
        publisher_metadata_(nullptr),
        render_context_(nullptr),
        event_handles_(kDefaultNumberOfEventsToRead, nullptr),
        cur_result_idx_(0),
        num_results_returned_(0) {}

  // Initialize the reader to read event logs with event record ID's
  // greater than or equal to the specified value in |first_event_id|.
  // Returns true if initialization succeeds.
  bool Initialize(uint64_t first_event_id);

  // Reads the next event log entry and stores it in |log_entry|. The log
  // entries are returned in an increasing order of their event record ids.
  // Returns true if |log_entry| was read successfully.
  bool GetNextEventLogEntry(EventLogsUploadManager::EventLogEntry* log_entry);

  // Close the reader and free internal resources.
  void Close();

  ~EventLogReader() { Close(); }

 private:
  bool HasValidQueryResults();
  bool GetQueryInfo(EVT_QUERY_PROPERTY_ID property_id,
                    std::vector<EVT_VARIANT>* value_buffer);
  bool ReadEventLogEntryFromEvent(
      EVT_HANDLE event_handle,
      EventLogsUploadManager::EventLogEntry* log_entry);
  bool GetFormattedMessage(EVT_HANDLE event_handle,
                           EVT_FORMAT_MESSAGE_FLAGS message_flag,
                           std::wstring* message);

  EVT_HANDLE results_handle_;
  EVT_HANDLE publisher_metadata_;
  EVT_HANDLE render_context_;
  std::vector<EVT_HANDLE> event_handles_;
  DWORD cur_result_idx_;
  DWORD num_results_returned_;
};

bool EventLogReader::Initialize(uint64_t first_event_id) {
  std::wstring query(kEventLogQueryTemplateStr);
  std::wstring pattern(L"{event_id}");
  query.replace(query.find(pattern), pattern.size(),
                base::NumberToWString(first_event_id));

  // If in an initialized state, close and re-initialize.
  if (results_handle_) {
    Close();
  }

  results_handle_ = EventLoggingApiManager::Get()->EvtQuery(
      nullptr, nullptr, query.c_str(),
      EvtQueryChannelPath | EvtQueryTolerateQueryErrors);

  if (results_handle_ == nullptr) {
    LOGFN(ERROR) << "EvtQuery failed! Error="
                 << EventLoggingApiManager::Get()->GetLastErrorAsString();
    return false;
  }

  if (!HasValidQueryResults()) {
    Close();
    LOGFN(WARNING) << "Could not find any valid logs!";
    return false;
  }

  publisher_metadata_ = EventLoggingApiManager::Get()->EvtOpenPublisherMetadata(
      nullptr, kEventLogPublisherName, nullptr, 0, 0);
  if (publisher_metadata_ == nullptr) {
    Close();
    LOGFN(ERROR) << "EvtOpenPublisherMetadata failed. Error="
                 << EventLoggingApiManager::Get()->GetLastErrorAsString();
    return false;
  }

  const DWORD paths_count =
      sizeof(kEventLogValuePaths) / sizeof(const wchar_t*);
  render_context_ = EventLoggingApiManager::Get()->EvtCreateRenderContext(
      paths_count, (LPCWSTR*)kEventLogValuePaths, EvtRenderContextValues);

  if (render_context_ == nullptr) {
    Close();
    LOGFN(ERROR) << "EvtCreateRenderContext failed. Error="
                 << EventLoggingApiManager::Get()->GetLastErrorAsString();
    return false;
  }

  return true;
}

bool EventLogReader::GetNextEventLogEntry(
    EventLogsUploadManager::EventLogEntry* log_entry) {
  DCHECK(results_handle_);
  DCHECK(log_entry);

  if (cur_result_idx_ >= num_results_returned_) {
    // Read the next block of events from the result set.
    if (!EventLoggingApiManager::Get()->EvtNext(
            results_handle_, event_handles_.size(), &event_handles_[0],
            INFINITE, 0, &num_results_returned_)) {
      DWORD last_error = EventLoggingApiManager::Get()->GetLastError();
      num_results_returned_ = 0;
      if (last_error != ERROR_NO_MORE_ITEMS) {
        LOGFN(ERROR) << "EvtNext failed with error: "
                     << EventLoggingApiManager::Get()->GetLastErrorAsString();
      }
      return false;
    }
    cur_result_idx_ = 0;
  }

  bool status =
      ReadEventLogEntryFromEvent(event_handles_[cur_result_idx_], log_entry);
  EventLoggingApiManager::Get()->EvtClose(event_handles_[cur_result_idx_]);
  event_handles_[cur_result_idx_] = nullptr;
  ++cur_result_idx_;
  return status;
}

// Returns true if the query resulted in valid results found.
bool EventLogReader::HasValidQueryResults() {
  std::vector<EVT_VARIANT> names_buffer, status_buffer;

  if (GetQueryInfo(EvtQueryNames, &names_buffer) &&
      GetQueryInfo(EvtQueryStatuses, &status_buffer)) {
    PEVT_VARIANT query_names = &names_buffer[0];
    PEVT_VARIANT query_statuses = &status_buffer[0];

    for (DWORD i = 0; i < query_names->Count; ++i) {
      if (query_statuses->UInt32Arr[i] != ERROR_SUCCESS) {
        LOGFN(ERROR) << "Query path " << query_names->StringArr[0]
                     << " has error status " << query_statuses->UInt32Arr[i];
        return false;
      }
    }
  } else {
    return false;
  }

  return true;
}

// Gets the requested property of the query and stores in the buffer.
bool EventLogReader::GetQueryInfo(EVT_QUERY_PROPERTY_ID property_id,
                                  std::vector<EVT_VARIANT>* value_buffer) {
  DCHECK(value_buffer);

  DWORD buffer_used = 0;
  if (!EventLoggingApiManager::Get()->EvtGetQueryInfo(
          results_handle_, property_id, 0, nullptr, &buffer_used)) {
    DWORD last_error = EventLoggingApiManager::Get()->GetLastError();
    if (last_error == ERROR_INSUFFICIENT_BUFFER) {
      value_buffer->resize(buffer_used / sizeof(EVT_VARIANT) + 1);
      DWORD buffer_size = value_buffer->size() * sizeof(EVT_VARIANT);
      if (EventLoggingApiManager::Get()->EvtGetQueryInfo(
              results_handle_, property_id, buffer_size, value_buffer->data(),
              &buffer_used)) {
        return true;
      }
    }
  }
  LOGFN(ERROR) << "EvtGetQueryInfo failed with error: "
               << EventLoggingApiManager::Get()->GetLastErrorAsString();
  return false;
}

// Reads the event log info from the specified handle.
bool EventLogReader::ReadEventLogEntryFromEvent(
    EVT_HANDLE event_handle,
    EventLogsUploadManager::EventLogEntry* log_entry) {
  DCHECK(log_entry);

  DWORD buffer_used = 0;
  DWORD property_count = 0;
  std::vector<EVT_VARIANT> buffer;
  bool render_done = false;

  if (!EventLoggingApiManager::Get()->EvtRender(
          render_context_, event_handle, EvtRenderEventValues, 0, nullptr,
          &buffer_used, &property_count)) {
    DWORD last_error = EventLoggingApiManager::Get()->GetLastError();
    if (last_error == ERROR_INSUFFICIENT_BUFFER) {
      buffer.resize(buffer_used / sizeof(EVT_VARIANT) + 1);
      DWORD buffer_size = buffer.size() * sizeof(EVT_VARIANT);
      if (EventLoggingApiManager::Get()->EvtRender(
              render_context_, event_handle, EvtRenderEventValues, buffer_size,
              &buffer[0], &buffer_used, &property_count)) {
        render_done = true;
      }
    }
  }

  if (render_done) {
    log_entry->event_id = buffer[0].UInt64Val;
    UINT64 ticks = buffer[1].FileTimeVal;
    // Convert from Windows epoch to Unix epoch.
    log_entry->created_ts.seconds = (INT64)((ticks / 10000000) - 11644473600LL);
    log_entry->created_ts.nanos = (INT32)((ticks % 10000000) * 100);
  } else {
    LOGFN(ERROR) << "EvtRender failed with error: "
                 << EventLoggingApiManager::Get()->GetLastErrorAsString();
    return false;
  }

  if (!GetFormattedMessage(event_handle, EvtFormatMessageEvent,
                           &log_entry->data)) {
    return false;
  }

  std::wstring level_str;
  if (!GetFormattedMessage(event_handle, EvtFormatMessageLevel, &level_str))
    return false;

  if (kEventLogLevelStrToIntValueMap.find(level_str) !=
      kEventLogLevelStrToIntValueMap.end()) {
    log_entry->severity_level = kEventLogLevelStrToIntValueMap.at(level_str);
  } else {
    log_entry->severity_level = 0;
  }

  return true;
}

// Read the requested formatted message from the event handle.
bool EventLogReader::GetFormattedMessage(EVT_HANDLE event_handle,
                                         EVT_FORMAT_MESSAGE_FLAGS message_flag,
                                         std::wstring* message) {
  DCHECK(message);

  DWORD buffer_used = 0;
  if (!EventLoggingApiManager::Get()->EvtFormatMessage(
          publisher_metadata_, event_handle, 0, 0, nullptr, message_flag, 0,
          nullptr, &buffer_used)) {
    DWORD last_error = EventLoggingApiManager::Get()->GetLastError();
    if (last_error == ERROR_INSUFFICIENT_BUFFER) {
      std::vector<WCHAR> buffer(buffer_used);
      if (EventLoggingApiManager::Get()->EvtFormatMessage(
              publisher_metadata_, event_handle, 0, 0, nullptr, message_flag,
              buffer.size(), &buffer[0], &buffer_used)) {
        message->assign(&buffer[0]);
        return true;
      }
    }
  }
  LOGFN(ERROR) << "EvtFormatMessage failed with error: "
               << EventLoggingApiManager::Get()->GetLastErrorAsString();
  return false;
}

// Close all open handles.
void EventLogReader::Close() {
  for (size_t i = 0; i < event_handles_.size(); ++i) {
    if (event_handles_[i]) {
      EventLoggingApiManager::Get()->EvtClose(event_handles_[i]);
      event_handles_[i] = nullptr;
    }
  }

  if (render_context_) {
    EventLoggingApiManager::Get()->EvtClose(render_context_);
    render_context_ = nullptr;
  }

  if (publisher_metadata_) {
    EventLoggingApiManager::Get()->EvtClose(publisher_metadata_);
    publisher_metadata_ = nullptr;
  }

  if (results_handle_) {
    EventLoggingApiManager::Get()->EvtClose(results_handle_);
    results_handle_ = nullptr;
  }
}

}  // namespace

// static
EventLogsUploadManager* EventLogsUploadManager::Get() {
  return *GetInstanceStorage();
}

// static
EventLogsUploadManager** EventLogsUploadManager::GetInstanceStorage() {
  static EventLogsUploadManager instance;
  static EventLogsUploadManager* instance_storage = &instance;
  return &instance_storage;
}

EventLogsUploadManager::EventLogsUploadManager()
    : upload_status_(S_OK), num_event_logs_uploaded_(0) {}

EventLogsUploadManager::~EventLogsUploadManager() = default;

GURL EventLogsUploadManager::GetGcpwServiceUploadEventViewerLogsUrl() {
  GURL gcpw_service_url = GetGcpwServiceUrl();

  return gcpw_service_url.Resolve(kGcpwServiceUploadEventLogsPath);
}

HRESULT EventLogsUploadManager::UploadEventViewerLogs(
    const std::string& access_token) {
  LOGFN(VERBOSE);

  DWORD log_upload_level = GetGlobalFlagOrDefault(kEventLogUploadLevelRegKey,
                                                  kDefaultUploadLogLevel);

  EventLogReader event_log_reader;

  DWORD first_event_log_id_to_upload =
      1 + GetGlobalFlagOrDefault(kEventLogUploadLastUploadedIdRegKey, 0);
  if (!event_log_reader.Initialize(first_event_log_id_to_upload)) {
    LOGFN(ERROR) << "Failed to initialize event log reader!";
    upload_status_ = E_FAIL;
    return upload_status_;
  }

  uint64_t chunk_id = 0;
  size_t log_entries_payload_size = 0;
  int num_upload_requests_made = 0;
  std::unique_ptr<base::Value::List> log_entry_value_list;
  EventLogEntry log_entry;

  while (event_log_reader.GetNextEventLogEntry(&log_entry) &&
         num_upload_requests_made < kMaxAllowedNumberOfUploadRequests) {
    if (log_entry.severity_level > log_upload_level)
      continue;

    chunk_id = std::max(chunk_id, log_entry.event_id);

    base::Value::Dict log_entry_value = log_entry.ToValue();

    // Get the JSON for the log to keep track of payload size.
    std::string log_entry_json;
    if (!base::JSONWriter::Write(log_entry_value, &log_entry_json)) {
      LOGFN(ERROR) << "base::JSONWriter::Write failed";
      upload_status_ = E_FAIL;
      return upload_status_;
    }

    if (!log_entry_value_list) {
      log_entry_value_list = std::make_unique<base::Value::List>();
    }
    log_entry_value_list->Append(std::move(log_entry_value));

    log_entries_payload_size += log_entry_json.size();
    if (log_entries_payload_size >= kMaxPayloadSizeOfLogEntries) {
      upload_status_ = MakeUploadLogChunkRequest(
          access_token, chunk_id, std::move(log_entry_value_list));
      if (FAILED(upload_status_)) {
        return upload_status_;
      }
      ++num_upload_requests_made;
      chunk_id = 0;
      log_entries_payload_size = 0;
    }
  }

  if (log_entry_value_list && !log_entry_value_list->empty()) {
    upload_status_ = MakeUploadLogChunkRequest(access_token, chunk_id,
                                               std::move(log_entry_value_list));
    if (FAILED(upload_status_)) {
      return upload_status_;
    }
    ++num_upload_requests_made;
  }

  LOGFN(VERBOSE) << num_event_logs_uploaded_ << " events uploaded with "
                 << num_upload_requests_made << " requests.";
  upload_status_ = S_OK;
  return upload_status_;
}

HRESULT EventLogsUploadManager::MakeUploadLogChunkRequest(
    const std::string& access_token,
    uint64_t chunk_id,
    std::unique_ptr<base::Value::List> log_entries_value_list) {
  // The GCPW service uses serial number and machine GUID for identifying
  // the device entry.
  std::wstring serial_number = GetSerialNumber();
  std::wstring machine_guid;
  HRESULT hr = GetMachineGuid(&machine_guid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Could not get Machine GUID. Error:" << putHR(hr);
    return hr;
  }

  size_t num_events_to_upload = log_entries_value_list->size();

  base::Value::Dict request_dict;
  request_dict.Set(kRequestSerialNumberParameterName,
                   base::WideToUTF8(serial_number));
  request_dict.Set(kRequestMachineGuidParameterName,
                   base::WideToUTF8(machine_guid));
  request_dict.Set(kRequestChunkIdParameterName, static_cast<int>(chunk_id));
  base::Value log_entries = base::Value(std::move(*log_entries_value_list));
  request_dict.Set(kRequestLogEntriesParameterName, std::move(log_entries));
  std::optional<base::Value> request_result;

  // Make the upload HTTP request.
  hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      EventLogsUploadManager::Get()->GetGcpwServiceUploadEventViewerLogsUrl(),
      access_token, {}, request_dict, kDefaultUploadLogsRequestTimeout,
      kMaxNumHttpRetries, &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return hr;
  }

  // Store the chunk id which is the last uploaded event log id
  // in registry so we know where to start next time.
  SetGlobalFlag(kEventLogUploadLastUploadedIdRegKey, chunk_id);
  num_event_logs_uploaded_ += num_events_to_upload;
  return S_OK;
}

base::Value::Dict EventLogsUploadManager::EventLogEntry::ToValue() const {
  return base::Value::Dict()
      .Set(kEventLogDataParameterName, base::WideToUTF8(data))
      .Set(kEventLogEventIdParameterName, static_cast<int>(event_id))
      .Set(kEventLogSeverityLevelParameterName,
           static_cast<int>(severity_level))
      .Set(kEventLogTimeStampParameterName,
           base::Value::Dict()
               .Set(kEventLogTimeStampSecondsParameterName,
                    static_cast<int>(created_ts.seconds))
               .Set(kEventLogTimeStampNanosParameterName,
                    static_cast<int>(created_ts.nanos)));
}

}  // namespace credential_provider
