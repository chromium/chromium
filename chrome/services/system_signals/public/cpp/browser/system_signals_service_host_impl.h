// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_

#include "components/device_signals/core/browser/system_signals_service_host.h"

#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

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
  mojo::Remote<device_signals::mojom::SystemSignalsService> remote_service_;
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_
