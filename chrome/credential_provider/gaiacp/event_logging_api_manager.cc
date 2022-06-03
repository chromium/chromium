// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/event_logging_api_manager.h"

#include "base/strings/string_number_conversions.h"

namespace credential_provider {

// static
EventLoggingApiManager* EventLoggingApiManager::Get() {
  return *GetInstanceStorage();
}

// static
EventLoggingApiManager** EventLoggingApiManager::GetInstanceStorage() {
  static EventLoggingApiManager instance;
  static EventLoggingApiManager* instance_storage = &instance;
  return &instance_storage;
}

EventLoggingApiManager::EventLoggingApiManager() = default;

EventLoggingApiManager::~EventLoggingApiManager() = default;

EVT_HANDLE EventLoggingApiManager::EvtQuery(EVT_HANDLE session,
                                            LPCWSTR path,
                                            LPCWSTR query,
                                            DWORD flags) {
  return ::EvtQuery(session, path, query, flags);
}

EVT_HANDLE EventLoggingApiManager::EvtOpenPublisherMetadata(
    EVT_HANDLE session,
    LPCWSTR publisher_id,
    LPCWSTR log_file_path,
    LCID locale,
    DWORD flags) {
  return ::EvtOpenPublisherMetadata(session, publisher_id, log_file_path,
                                    locale, flags);
}

EVT_HANDLE EventLoggingApiManager::EvtCreateRenderContext(
    DWORD value_paths_count,
    LPCWSTR* value_paths,
    DWORD flags) {
  return ::EvtCreateRenderContext(value_paths_count, value_paths, flags);
}

BOOL EventLoggingApiManager::EvtNext(EVT_HANDLE result_set,
                                     DWORD events_size,
                                     PEVT_HANDLE events,
                                     DWORD timeout,
                                     DWORD flags,
                                     PDWORD num_returned) {
  return ::EvtNext(result_set, events_size, events, timeout, flags,
                   num_returned);
}

BOOL EventLoggingApiManager::EvtGetQueryInfo(EVT_HANDLE query,
                                             EVT_QUERY_PROPERTY_ID property_id,
                                             DWORD value_buffer_size,
                                             PEVT_VARIANT value_buffer,
                                             PDWORD value_buffer_used) {
  return ::EvtGetQueryInfo(query, property_id, value_buffer_size, value_buffer,
                           value_buffer_used);
}

BOOL EventLoggingApiManager::EvtRender(EVT_HANDLE context,
                                       EVT_HANDLE evt_handle,
                                       DWORD flags,
                                       DWORD buffer_size,
                                       PVOID buffer,
                                       PDWORD buffer_used,
                                       PDWORD property_count) {
  return ::EvtRender(context, evt_handle, flags, buffer_size, buffer,
                     buffer_used, property_count);
}

BOOL EventLoggingApiManager::EvtFormatMessage(EVT_HANDLE publisher_metadata,
                                              EVT_HANDLE event,
                                              DWORD message_id,
                                              DWORD value_count,
                                              PEVT_VARIANT values,
                                              DWORD flags,
                                              DWORD buffer_size,
                                              LPWSTR buffer,
                                              PDWORD buffer_used) {
  return ::EvtFormatMessage(publisher_metadata, event, message_id, value_count,
                            values, flags, buffer_size, buffer, buffer_used);
}

BOOL EventLoggingApiManager::EvtClose(EVT_HANDLE handle) {
  return ::EvtClose(handle);
}

DWORD EventLoggingApiManager::GetLastError() {
  return ::GetLastError();
}

std::string EventLoggingApiManager::GetLastErrorAsString() {
  DWORD error_code = this->GetLastError();
  if (error_code == ERROR_SUCCESS)
    return std::string();

  LPSTR buffer = nullptr;
  if (FormatMessageA(
          FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr,
          error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
          (LPSTR)&buffer, 0, nullptr)) {
    std::string error_str(buffer);
    LocalFree(buffer);
    return error_str;
  }

  // If we fail return at least the error code.
  return base::NumberToString(error_code);
}

}  // namespace credential_provider
