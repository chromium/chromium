// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/os_service_manager.h"

#include "chrome/credential_provider/extension/extension_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {
namespace extension {

OSServiceManager** OSServiceManager::GetInstanceStorage() {
  static OSServiceManager* instance = new OSServiceManager();

  return &instance;
}

// static
OSServiceManager* OSServiceManager::Get() {
  return *GetInstanceStorage();
}

OSServiceManager::~OSServiceManager() {}

DWORD OSServiceManager::InstallService(
    const base::FilePath& service_binary_path,
    ScopedScHandle* sc_handle) {
  ScopedScHandle scm_handle(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  *sc_handle = ScopedScHandle(::CreateService(
      scm_handle.Get(),                     // SCM database
      kGCPWExtensionServiceName,            // name of service
      kGCPWExtensionServiceDisplayName,     // service name to display
      SERVICE_ALL_ACCESS,                   // desired access
      SERVICE_WIN32_OWN_PROCESS,            // service type
      SERVICE_AUTO_START,                   // start type
      SERVICE_ERROR_NORMAL,                 // error control type
      service_binary_path.value().c_str(),  // path to service's binary
      nullptr,                              // no load ordering group
      nullptr,                              // no tag identifier
      nullptr,                              // no dependencies
      nullptr,                              // LocalSystem account
      nullptr));

  if (!sc_handle->IsValid())
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD OSServiceManager::GetServiceStatus(SERVICE_STATUS* service_status) {
  DCHECK(service_status);

  ScopedScHandle scm_handle(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  ScopedScHandle sc_handle(::OpenService(
      scm_handle.Get(), kGCPWExtensionServiceName, SERVICE_QUERY_STATUS));
  if (!sc_handle.IsValid())
    return ::GetLastError();

  if (!::QueryServiceStatus(sc_handle.Get(), service_status)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

DWORD OSServiceManager::DeleteService() {
  ScopedScHandle scm_handle(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  ScopedScHandle sc_handle(
      ::OpenService(scm_handle.Get(), kGCPWExtensionServiceName, DELETE));
  if (!sc_handle.IsValid())
    return ::GetLastError();

  // The DeleteService function marks a service for deletion from the service
  // control manager database. The database entry is not removed until all open
  // handles to the service have been closed by calls to the CloseServiceHandle
  // function, and the service is not running.
  if (!::DeleteService(sc_handle.Get()))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD OSServiceManager::StartGCPWService() {
  ScopedScHandle scm_handle(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  ScopedScHandle sc_handle(::OpenService(
      scm_handle.Get(), kGCPWExtensionServiceName, SERVICE_START));
  if (!sc_handle.IsValid())
    return ::GetLastError();

  if (!::StartService(sc_handle.Get(), 0, nullptr))
    return ::GetLastError();

  LOGFN(INFO) << "GCPW extension started successfully.";

  return ERROR_SUCCESS;
}

DWORD OSServiceManager::ControlService(DWORD control,
                                       SERVICE_STATUS* service_status) {
  DCHECK(service_status);

  ScopedScHandle scm_handle(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  // TODO(crbug.com/1108932): More granular access rights corresponding to the
  // controls can be specified.
  ScopedScHandle s_handle(::OpenService(
      scm_handle.Get(), kGCPWExtensionServiceName, SERVICE_ALL_ACCESS));
  if (!s_handle.IsValid())
    return ::GetLastError();

  if (!::ControlService(s_handle.Get(), control,
                        (LPSERVICE_STATUS)&service_status))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD OSServiceManager::StartServiceCtrlDispatcher(
    LPSERVICE_MAIN_FUNCTION service_main) {
  SERVICE_TABLE_ENTRY dispatch_table[] = {
      {(LPWSTR)kGCPWExtensionServiceName, service_main}, {nullptr, nullptr}};

  if (!::StartServiceCtrlDispatcher(dispatch_table))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD OSServiceManager::RegisterCtrlHandler(
    LPHANDLER_FUNCTION handler_proc,
    SERVICE_STATUS_HANDLE* service_status_handle) {
  DCHECK(handler_proc);
  DCHECK(service_status_handle);

  SERVICE_STATUS_HANDLE sc_status_handle =
      ::RegisterServiceCtrlHandler(kGCPWExtensionServiceName, handler_proc);
  if (!sc_status_handle)
    return ::GetLastError();

  *service_status_handle = sc_status_handle;
  return ERROR_SUCCESS;
}

DWORD OSServiceManager::SetServiceStatus(
    SERVICE_STATUS_HANDLE service_status_handle,
    SERVICE_STATUS service) {
  if (!::SetServiceStatus(service_status_handle, &service))
    return ::GetLastError();
  return ERROR_SUCCESS;
}

}  // namespace extension
}  // namespace credential_provider
