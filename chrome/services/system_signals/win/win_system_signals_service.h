// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_WIN_WIN_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_WIN_WIN_SYSTEM_SIGNALS_SERVICE_H_

#include <vector>

#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace system_signals {

class WinSystemSignalsService
    : public device_signals::mojom::SystemSignalsService {
 public:
  explicit WinSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver);
  ~WinSystemSignalsService() override;

  WinSystemSignalsService(const WinSystemSignalsService&) = delete;
  WinSystemSignalsService& operator=(const WinSystemSignalsService&) = delete;

  // mojom::SystemSignalsService:
  void GetBinarySignals(
      std::vector<device_signals::mojom::BinarySignalsRequestPtr> requests,
      GetBinarySignalsCallback callback) override;
  void GetAntiVirusSignals(GetAntiVirusSignalsCallback callback) override;
  void GetHotfixSignals(GetHotfixSignalsCallback callback) override;

 private:
  mojo::Receiver<device_signals::mojom::SystemSignalsService> receiver_;
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_WIN_WIN_SYSTEM_SIGNALS_SERVICE_H_
