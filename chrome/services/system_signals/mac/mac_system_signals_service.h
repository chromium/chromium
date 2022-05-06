// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_MAC_MAC_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_MAC_MAC_SYSTEM_SIGNALS_SERVICE_H_

#include <vector>

#include "chrome/services/system_signals/public/mojom/system_signals.mojom.h"

namespace system_signals {

class MacSystemSignalsService : public mojom::SystemSignalsService {
 public:
  MacSystemSignalsService();
  ~MacSystemSignalsService() override;

  MacSystemSignalsService(const MacSystemSignalsService&) = delete;
  MacSystemSignalsService& operator=(const MacSystemSignalsService&) = delete;

  // mojom::SystemSignalsService:
  void GetBinarySignals(std::vector<mojom::BinarySignalsRequestPtr> requests,
                        GetBinarySignalsCallback callback) override;
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_MAC_MAC_SYSTEM_SIGNALS_SERVICE_H_
