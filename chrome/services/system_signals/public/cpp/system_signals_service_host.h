// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_SYSTEM_SIGNALS_SERVICE_HOST_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_SYSTEM_SIGNALS_SERVICE_HOST_H_

#include "build/build_config.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom-forward.h"

#if BUILDFLAG(IS_WIN)
#include "mojo/public/cpp/bindings/remote.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include <memory>
#endif

namespace system_signals {

// Class in charge of creating and handling the service's lifecycle. Clients of
// SystemSignalsService should always go through a common instance of this class
// to retrieve a service instance.
class SystemSignalsServiceHost {
 public:
  SystemSignalsServiceHost();
  ~SystemSignalsServiceHost();

  SystemSignalsServiceHost(const SystemSignalsServiceHost&) = delete;
  SystemSignalsServiceHost& operator=(const SystemSignalsServiceHost&) = delete;

  // Returns a pointer to the currently available SystemSignalsService instance.
  device_signals::mojom::SystemSignalsService* GetService();

 private:
#if BUILDFLAG(IS_WIN)
  mojom::Remote<device_signals::mojom::SystemSignalsService> remote_service_;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  std::unique_ptr<device_signals::mojom::SystemSignalsService> local_service_;
#endif
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_SYSTEM_SIGNALS_SERVICE_HOST_H_
