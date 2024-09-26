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

// A delegate interface for a Windows Service hosting a COM server.
class ServiceDelegate {
 public:
  virtual ~ServiceDelegate() = default;

  // Returns the event category to use when logging to the Windows Event Log.
  virtual uint16_t GetLogEventCategory() = 0;

  // Returns the event message to use when logging to the Windows Event Log.
  virtual uint32_t GetLogEventMessageId() = 0;

  // Performs any service-specific processing prior to registering the
  // service's COM classes. Called on the service's main thread.
  virtual void PreRun() {}

  // Returns the service's class factories and corresponding CLSIDs. Called on
  // the service's main thread.
  virtual base::expected<base::HeapArray<FactoryAndClsid>, HRESULT>
  CreateClassFactories() = 0;

  // Performs any service-specific processing after deregistering the
  // service's COM classes. Called on the service's main thread.
  virtual void PostRun() {}

 protected:
  ServiceDelegate() = default;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_DELEGATE_H_
