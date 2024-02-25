// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_H_

#include "components/device_signals/core/common/mojom/system_signals.mojom-forward.h"
#include "components/keyed_service/core/keyed_service.h"

namespace device_signals {

// Class in charge of creating and handling the service's lifecycle. Clients of
// SystemSignalsService should always go through a common instance of this class
// to retrieve a service instance.
class SystemSignalsServiceHost : public KeyedService {
 public:
  ~SystemSignalsServiceHost() override = default;

  // Returns a pointer to the currently available SystemSignalsService instance.
  virtual device_signals::mojom::SystemSignalsService* GetService() = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_H_
