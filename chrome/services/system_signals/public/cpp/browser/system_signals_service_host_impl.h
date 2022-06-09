// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_

#include "build/build_config.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"

#if BUILDFLAG(IS_WIN)
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include <memory>
#include "components/device_signals/core/common/mojom/system_signals.mojom-forward.h"
#endif

namespace system_signals {

class SystemSignalsServiceHostImpl
    : public device_signals::SystemSignalsServiceHost {
 public:
  SystemSignalsServiceHostImpl();
  ~SystemSignalsServiceHostImpl() override;

  SystemSignalsServiceHostImpl(const SystemSignalsServiceHostImpl&) = delete;
  SystemSignalsServiceHostImpl& operator=(const SystemSignalsServiceHostImpl&) =
      delete;

  // device_signals::SystemSignalsServiceHost:
  device_signals::mojom::SystemSignalsService* GetService() override;

 private:
#if BUILDFLAG(IS_WIN)
  mojo::Remote<device_signals::mojom::SystemSignalsService> remote_service_;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  std::unique_ptr<device_signals::mojom::SystemSignalsService> local_service_;
#endif
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_
