// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_

#include "chromeos/ash/services/cros_healthd/private/mojom/cros_healthd_internal.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {

class DataCollector : public mojom::ChromiumDataCollector {
 public:
  // Delegate class to be replaced for testing.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Get the touchpad library name.
    virtual std::string GetTouchpadLibraryName() = 0;
  };

  DataCollector();
  DataCollector(Delegate* delegate);
  DataCollector(const DataCollector&) = delete;
  DataCollector& operator=(const DataCollector&) = delete;
  ~DataCollector() override;

  // Binds new pipe and returns the mojo remote.
  mojo::PendingRemote<mojom::ChromiumDataCollector> BindNewPipeAndPassRemote();

 private:
  // mojom::ChromiumDataCollector overrides.
  void GetTouchscreenDevices(GetTouchscreenDevicesCallback callback) override;
  void GetTouchpadLibraryName(GetTouchpadLibraryNameCallback callback) override;

  // Pointer to the delegate.
  Delegate* const delegate_;
  // The mojo receiver.
  mojo::Receiver<mojom::ChromiumDataCollector> receiver_{this};
};

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace cros_healthd {
namespace internal {
using ::chromeos::cros_healthd::internal::DataCollector;
}
}  // namespace cros_healthd
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_
