// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_MAC_MAC_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_MAC_MAC_SYSTEM_SIGNALS_SERVICE_H_

#include "chrome/services/system_signals/base_system_signals_service.h"

namespace system_signals {

class MacSystemSignalsService : public BaseSystemSignalsService {
 public:
  explicit MacSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver);
  ~MacSystemSignalsService() override;

  MacSystemSignalsService(const MacSystemSignalsService&) = delete;
  MacSystemSignalsService& operator=(const MacSystemSignalsService&) = delete;

 private:
  friend class MacSystemSignalsServiceTest;

  // Constructor that can be used by tests to mock out dependencies.
  MacSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver,
      std::unique_ptr<device_signals::FileSystemService> file_system_service);
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_MAC_MAC_SYSTEM_SIGNALS_SERVICE_H_
