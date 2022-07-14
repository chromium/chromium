// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_

#include <memory>
#include <vector>

#include "components/device_signals/core/common/mojom/system_signals.mojom.h"

namespace device_signals {
class FileSystemService;
}  // namespace device_signals

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
  void GetFileSystemSignals(
      const std::vector<device_signals::GetFileSystemInfoOptions>& requests,
      GetFileSystemSignalsCallback callback) override;

 private:
  friend class LinuxSystemSignalsServiceTest;

  // Constructor that can be used by tests to mock out dependencies.
  explicit LinuxSystemSignalsService(
      std::unique_ptr<device_signals::FileSystemService> file_system_service);

  std::unique_ptr<device_signals::FileSystemService> file_system_service_;
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_LINUX_LINUX_SYSTEM_SIGNALS_SERVICE_H_
