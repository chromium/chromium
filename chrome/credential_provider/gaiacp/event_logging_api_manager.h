// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_EVENT_LOGGING_API_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_EVENT_LOGGING_API_MANAGER_H_

#include <windows.h>

#include <winevt.h>

#include <string>

namespace credential_provider {

// Class that exposes the Win32 Event logging API.
class EventLoggingApiManager {
 public:
  // Get the manager instance.
  static EventLoggingApiManager* Get();

  // Runs a query to retrieve events that match the specified query criteria.
  virtual EVT_HANDLE EvtQuery(EVT_HANDLE session,
                              LPCWSTR path,
                              LPCWSTR query,
                              DWORD flags);

  // Gets a handle that you use to read the specified provider's metadata.
  virtual EVT_HANDLE EvtOpenPublisherMetadata(EVT_HANDLE session,
                                              LPCWSTR publisher_id,
                                              LPCWSTR log_file_path,
                                              LCID locale,
                                              DWORD flags);

  // Creates a context that specifies the information in the event that
  // you want to render.
  virtual EVT_HANDLE EvtCreateRenderContext(DWORD value_paths_count,
                                            LPCWSTR* value_paths,
                                            DWORD flags);

  // Gets the next event from the query results.
  virtual BOOL EvtNext(EVT_HANDLE result_set,
                       DWORD events_size,
                       PEVT_HANDLE events,
                       DWORD timeout,
                       DWORD flags,
                       PDWORD returned);

  // Gets information about the success or failure of the query.
  virtual BOOL EvtGetQueryInfo(EVT_HANDLE query,
                               EVT_QUERY_PROPERTY_ID property_id,
                               DWORD value_buffer_size,
                               PEVT_VARIANT value_buffer,
                               PDWORD value_buffer_used);

  // Renders the information from an event based on the rendering context
  // specified.
  virtual BOOL EvtRender(EVT_HANDLE context,
                         EVT_HANDLE evt_handle,
                         DWORD flags,
                         DWORD buffer_size,
                         PVOID buffer,
                         PDWORD buffer_used,
                         PDWORD property_count);

  // Formats a message string.
  virtual BOOL EvtFormatMessage(EVT_HANDLE publisher_metadata,
                                EVT_HANDLE event,
                                DWORD message_id,
                                DWORD value_count,
                                PEVT_VARIANT values,
                                DWORD flags,
                                DWORD buffer_size,
                                LPWSTR buffer,
                                PDWORD buffer_used);

  // Closes an open handle.
  virtual BOOL EvtClose(EVT_HANDLE handle);

  // Get the error code for the last error encountered by any of
  // the above methods.
  virtual DWORD GetLastError();

  // Get the error string for the last error encountered by any of
  // the above methods.
  virtual std::string GetLastErrorAsString();

 protected:
  // Returns the storage used for the instance pointer.
  static EventLoggingApiManager** GetInstanceStorage();

  EventLoggingApiManager();
  virtual ~EventLoggingApiManager();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_EVENT_LOGGING_API_MANAGER_H_
