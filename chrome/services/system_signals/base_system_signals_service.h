// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SYSTEM_SIGNALS_BASE_SYSTEM_SIGNALS_SERVICE_H_
#define CHROME_SERVICES_SYSTEM_SIGNALS_BASE_SYSTEM_SIGNALS_SERVICE_H_

#include <memory>
#include <vector>

#include "components/device_signals/core/common/mojom/system_signals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace device_signals {
class FileSystemService;
}  // namespace device_signals

namespace system_signals {

class BaseSystemSignalsService
    : public device_signals::mojom::SystemSignalsService {
 public:
  // device_signals::mojom::SystemSignalsService:
  void GetFileSystemSignals(
      const std::vector<device_signals::GetFileSystemInfoOptions>& requests,
      GetFileSystemSignalsCallback callback) override;

 protected:
  explicit BaseSystemSignalsService(
      mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
          receiver,
      std::unique_ptr<device_signals::FileSystemService> file_system_service);

  ~BaseSystemSignalsService() override;

  mojo::Receiver<device_signals::mojom::SystemSignalsService> receiver_;
  std::unique_ptr<device_signals::FileSystemService> file_system_service_;
};

}  // namespace system_signals

#endif  // CHROME_SERVICES_SYSTEM_SIGNALS_BASE_SYSTEM_SIGNALS_SERVICE_H_
