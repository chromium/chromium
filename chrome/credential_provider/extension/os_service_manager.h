// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_OS_SERVICE_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_OS_SERVICE_MANAGER_H_

#include <windows.h>

#include "base/files/file_path.h"
#include "chrome/credential_provider/extension/scoped_handle.h"

namespace credential_provider {

namespace extension {

class OSServiceManager {
 public:
  static OSServiceManager* Get();

  virtual ~OSServiceManager();

  // Creates a service object and adds it to the specified service control
  // manager database. Uses the |service_binary_path| as the location of service
  // binary. Returns handle of the service in |sc_handle| parameter.
  virtual DWORD InstallService(const base::FilePath& service_binary_path,
                               ScopedScHandle* sc_handle);

  // Uses The QueryServiceStatus API which returns the most recent service
  // status information reported to the service control manager. The
  // |service_status| receives the latest status of the service. SERVICE_STATUS
  // is obtained via QueryServiceStatus API call.
  virtual DWORD GetServiceStatus(SERVICE_STATUS* service_status);

  // The DeleteService function marks a service for deletion from the service
  // control manager database. The database entry is not removed until all open
  // handles to the service have been closed by calls to the CloseServiceHandle
  // function, and the service is not running.
  virtual DWORD DeleteService();

  // Starts the GCPW extension using StartService Windows API.
  virtual DWORD StartGCPWService();

  // Waits until service transitions to SERVICE_STOPPED state or the wait times
  // out. Returns ERROR_SUCCESS if service is stopped successfully. Returns an
  // error code in any other case.
  virtual DWORD WaitForServiceStopped();

  // Calls the ControlService API to change the state of the service. |control|
  // needs to be one of the service controls as specified in documentation [1].
  // As a result |service_status| is returned that has the latest state of the
  // service. [1]
  // https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-controlservice
  virtual DWORD ControlService(DWORD control);

  // Updates the configuration of the service.
  virtual DWORD ChangeServiceConfig(DWORD dwServiceType,
                                    DWORD dwStartType,
                                    DWORD dwErrorControl);

  // When the service control manager starts a service process, it waits for the
  // process to call the StartServiceCtrlDispatcher function. The main thread of
  // a service process should make this call as soon as possible after it starts
  // up (within 30 seconds). If StartServiceCtrlDispatcher succeeds, it connects
  // the calling thread to the service control manager and does not return until
  // all running services in the process have entered the SERVICE_STOPPED state.
  // The lpServiceTable parameter contains an entry for each service that can
  // run in the calling process. Each entry specifies the ServiceMain function
  // for that service.
  virtual DWORD StartServiceCtrlDispatcher(
      LPSERVICE_MAIN_FUNCTION service_main);

  // Registers a function to handle service control requests.
  virtual DWORD RegisterCtrlHandler(
      LPHANDLER_FUNCTION handler_proc,
      SERVICE_STATUS_HANDLE* service_status_handle);

  // Updates the service control manager's status information for the calling
  // service.
  virtual DWORD SetServiceStatus(SERVICE_STATUS_HANDLE service_status_handle,
                                 SERVICE_STATUS service);

 protected:
  OSServiceManager() {}

  // Returns the storage used for the instance pointer.
  static OSServiceManager** GetInstanceStorage();
};

}  // namespace extension
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_OS_SERVICE_MANAGER_H_
