// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
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
  void AddObserver(
      device_signals::SystemSignalsServiceHost::Observer* observer) override;
  void RemoveObserver(
      device_signals::SystemSignalsServiceHost::Observer* observer) override;
  void NotifyServiceDisconnect() override;

  void BindRemoteForTesting(
      mojo::PendingRemote<device_signals::mojom::SystemSignalsService> remote);

 private:
  mojo::Remote<device_signals::mojom::SystemSignalsService> remote_service_;
  base::ObserverList<device_signals::SystemSignalsServiceHost::Observer>
      observers_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SystemSignalsServiceHostImpl> weak_factory_{this};
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_PUBLIC_CPP_BROWSER_SYSTEM_SIGNALS_SERVICE_HOST_IMPL_H_
