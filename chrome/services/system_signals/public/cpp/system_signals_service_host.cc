// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/public/cpp/system_signals_service_host.h"

#include "components/device_signals/core/common/mojom/system_signals.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/service_process_host.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/services/system_signals/mac/mac_system_signals_service.h"
#elif BUILDFLAG(IS_LINUX)
#include "chrome/services/system_signals/linux/linux_system_signals_service.h"
#endif

namespace system_signals {

SystemSignalsServiceHost::SystemSignalsServiceHost() = default;
SystemSignalsServiceHost::~SystemSignalsServiceHost() = default;

#if BUILDFLAG(IS_WIN)

device_signals::mojom::SystemSignalsService*
SystemSignalsServiceHost::GetService() {
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
  return &remote_service_;
}

#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

device_signals::mojom::SystemSignalsService*
SystemSignalsServiceHost::GetService() {
  if (!local_service_) {
#if BUILDFLAG(IS_MAC)
    local_service_ = std::make_unique<MacSystemSignalsService>();
#else
    local_service_ = std::make_unique<LinuxSystemSignalsService>();
#endif  // BUILDFLAG(IS_MAC)
  }
  return local_service_.get();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace system_signals
