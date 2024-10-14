// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_DELEGATE_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_DELEGATE_H_

#include <stdint.h>
#include <wrl/client.h>

#include "base/containers/heap_array.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"
#include "chrome/windows_services/service_program/factory_and_clsid.h"

namespace base {

class CommandLine;

}  // namespace base

// A delegate interface for a Windows Service hosting a COM server.
class ServiceDelegate {
 public:
  virtual ~ServiceDelegate() = default;

  // This method is only called if `ServiceProgramMain` is used.
  // Returns the event category to use when logging to the Windows Event Log.
  virtual uint16_t GetLogEventCategory();

  // This method is only called if `ServiceProgramMain` is used.
  // Returns the event message to use when logging to the Windows Event Log.
  virtual uint32_t GetLogEventMessageId();

  // Performs any service-specific processing prior to registering the service's
  // COM classes. Called on the service's main thread. Implementations should:
  // * return `false` to rely on the `Service` to register/unregister COM
  //   classes (including calling `CreateClassFactories` and calling `PostRun`),
  //   and run a `RunLoop` on behalf of the `ServiceDelegate`.
  // * return `true` to have the `Service` delegate these responsibilities to
  //   this instance's `Run` method. The expectation is that `Run` does all the
  //   logic of registering/unregistering classes and running the COM server.
  virtual bool PreRun();

  // Only invoked by the `Service` if `PreRun` returns `true`. This method is
  // called by the `Service` to delegate all the logic of
  // registering/unregistering classes and running the COM server.
  virtual HRESULT Run(const base::CommandLine& command_line);

  // This method is only called if `PreRun` returns `false`, see `PreRun` above
  // for more details. Returns the service's class factories and corresponding
  // CLSIDs. Called on the service's main thread.
  virtual base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
  CreateClassFactories();

  // Allows the delegate to handle a `SERVICE_CONTROL_STOP` message. This method
  // is called from the service control dispatcher thread.
  virtual void OnServiceControlStop() {}

  // Called on the service's main thread, and can be used to perform any
  // service-specific processing after the service is done running, including
  // done with deregistering the service's COM classes.
  virtual void PostRun() {}

 protected:
  ServiceDelegate() = default;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_DELEGATE_H_
