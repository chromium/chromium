// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_

#include "chromeos/services/cros_healthd/private/mojom/cros_healthd_internal.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {

class DataCollector {
 public:
  DataCollector(const DataCollector&) = delete;
  DataCollector& operator=(const DataCollector&) = delete;

  // Initialize a global instance.
  static void Initialize();

  // Shutdown the global instance.
  static void Shutdown();

  // Returns the global instance. Check failed if this is not initialized.
  static DataCollector* Get();

  // Binds a mojo receiver to this.
  virtual void BindReceiver(
      mojo::PendingReceiver<mojom::ChromiumDataCollector> receiver) = 0;

 protected:
  DataCollector();
  virtual ~DataCollector();
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

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_
