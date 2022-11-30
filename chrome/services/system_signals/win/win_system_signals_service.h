// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_WIN_WIN_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_WIN_WIN_SYSTEM_SIGNALS_SERVICE_H_

#include "base/win/scoped_com_initializer.h"
#include "chrome/services/system_signals/base_system_signals_service.h"

namespace device_signals {
class FileSystemService;
class WmiClient;
class WscClient;
}  // namespace device_signals

namespace system_signals {

class WinSystemSignalsService : public BaseSystemSignalsService {
 public:
  explicit WinSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver);

  ~WinSystemSignalsService() override;

  WinSystemSignalsService(const WinSystemSignalsService&) = delete;
  WinSystemSignalsService& operator=(const WinSystemSignalsService&) = delete;

  // mojom::SystemSignalsService:
  void GetAntiVirusSignals(GetAntiVirusSignalsCallback callback) override;
  void GetHotfixSignals(GetHotfixSignalsCallback callback) override;

 private:
  friend class WinSystemSignalsServiceTest;

  // Constructor that can be used by tests to mock out dependencies.
  WinSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver,
      std::unique_ptr<device_signals::FileSystemService> file_system_service,
      std::unique_ptr<device_signals::WmiClient> wmi_client,
      std::unique_ptr<device_signals::WscClient> wsc_client);

  std::unique_ptr<device_signals::WmiClient> wmi_client_;
  std::unique_ptr<device_signals::WscClient> wsc_client_;
  base::win::ScopedCOMInitializer scoped_com_initializer_;
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_WIN_WIN_SYSTEM_SIGNALS_SERVICE_H_
