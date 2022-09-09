// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_

#include "chrome/services/system_signals/base_system_signals_service.h"

namespace system_signals {

class LinuxSystemSignalsService : public BaseSystemSignalsService {
 public:
  explicit LinuxSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver);
  ~LinuxSystemSignalsService() override;

  LinuxSystemSignalsService(const LinuxSystemSignalsService&) = delete;
  LinuxSystemSignalsService& operator=(const LinuxSystemSignalsService&) =
      delete;

 private:
  friend class LinuxSystemSignalsServiceTest;

  // Constructor that can be used by tests to mock out dependencies.
  LinuxSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver,
      std::unique_ptr<device_signals::FileSystemService> file_system_service);
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_
