// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_

#include <vector>

#include "components/device_signals/core/common/mojom/system_signals.mojom.h"

namespace system_signals {

class LinuxSystemSignalsService
    : public device_signals::mojom::SystemSignalsService {
 public:
  LinuxSystemSignalsService();
  ~LinuxSystemSignalsService() override;

  LinuxSystemSignalsService(const LinuxSystemSignalsService&) = delete;
  LinuxSystemSignalsService& operator=(const LinuxSystemSignalsService&) =
      delete;

  // device_signals::mojom::SystemSignalsService:
  void GetBinarySignals(
      std::vector<device_signals::mojom::BinarySignalsRequestPtr> requests,
      GetBinarySignalsCallback callback) override;
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_
