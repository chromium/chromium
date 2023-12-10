// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SYSTEM_SIGNALS_SERVICE_HOST_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SYSTEM_SIGNALS_SERVICE_HOST_H_

#include "build/build_config.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockSystemSignalsServiceHost : public SystemSignalsServiceHost {
 public:
  MockSystemSignalsServiceHost();
  ~MockSystemSignalsServiceHost() override;

  MOCK_METHOD(mojom::SystemSignalsService*, GetService, (), (override));
};

class MockSystemSignalsService : public mojom::SystemSignalsService {
 public:
  MockSystemSignalsService();
  ~MockSystemSignalsService() override;

  MOCK_METHOD(void,
              GetFileSystemSignals,
              (const std::vector<device_signals::GetFileSystemInfoOptions>&,
               GetFileSystemSignalsCallback),
              (override));

#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(void,
              GetAntiVirusSignals,
              (GetAntiVirusSignalsCallback),
              (override));
  MOCK_METHOD(void, GetHotfixSignals, (GetHotfixSignalsCallback), (override));
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_SYSTEM_SIGNALS_SERVICE_HOST_H_
