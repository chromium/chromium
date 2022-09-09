// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/public/cpp/browser/system_signals_service_host_impl.h"

#include "components/device_signals/core/common/mojom/system_signals.mojom.h"

#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"              // nogncheck
#include "content/public/browser/service_process_host.h"  // nogncheck

namespace system_signals {

SystemSignalsServiceHostImpl::SystemSignalsServiceHostImpl() = default;
SystemSignalsServiceHostImpl::~SystemSignalsServiceHostImpl() = default;

device_signals::mojom::SystemSignalsService*
SystemSignalsServiceHostImpl::GetService() {
  // To prevent any impact on Chrome's stability and memory footprint, run
  // this service in its own process on Windows (since it interacts with, e.g.,
  // WMI).
  if (!remote_service_) {
    remote_service_ = content::ServiceProcessHost::Launch<
        device_signals::mojom::SystemSignalsService>(
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_UTILITY_PROCESS_SYSTEM_SIGNALS_NAME)
            .Pass());
    remote_service_.reset_on_idle_timeout(base::Seconds(10));
  }
  return remote_service_.get();
}

}  // namespace system_signals
