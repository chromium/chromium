// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_SERVICE_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_SERVICE_H_

#include <windows.h>

#include "base/functional/callback.h"

namespace credential_provider {
namespace extension {

// Bare implementation of the GCPW extension service. Takes care the handshake
// with the service control manager and the lifetime of the service.
class Service {
 public:
  // Gets the singleton instance of the service.
  static Service* Get();

  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

  // Invoke the chosen action routine. By default service runs as a service,
  // but the action routine can support running in console for testing purposes.
  DWORD Run();

 private:
  Service();
  virtual ~Service();

  // The action routine to be executed.
  DWORD (Service::*run_routine_)();

  // This function handshakes with the service control manager and starts
  // the service.
  DWORD RunAsService();

  // Non-static function that is called as part of service main(ServiceMain).
  // Performs registering control handler callback and managing the service
  // states.
  void StartMain();

  // Service main call back which was provided earlier to service control
  // manager as part of RunAsService call.
  static VOID WINAPI ServiceMain(DWORD argc, WCHAR* argv[]);

  // The control handler of the service. Details about the control codes can be
  // found here:
  // https://docs.microsoft.com/en-us/windows/win32/services/service-control-handler-function
  static void WINAPI ServiceControlHandler(DWORD control);

  // Returns the storage used for the instance pointer.
  static Service** GetInstanceStorage();

  // Status of the running service. Must be updated accordingly before calling
  // SetServiceStatus API.
  SERVICE_STATUS service_status_;

  // The service status handle which is used with SetServiceStatus API.
  SERVICE_STATUS_HANDLE service_status_handle_;

  // Callback to end running periodic tasks.
  base::OnceClosure quit_closure_;
};

}  // namespace extension
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_SERVICE_H_
