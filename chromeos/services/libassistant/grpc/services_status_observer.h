// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_OBSERVER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_OBSERVER_H_

#include "base/observer_list_types.h"

namespace chromeos {
namespace libassistant {

// Enums representing states of Libassistant services in the order of the
// bootup progession. States are monotonic and cannot get reverted back to
// any previous states once a state is reached.
enum class ServicesStatus {
  // Connection to the libassistant's gRPC service is not available.
  OFFLINE,

  // Services are booting up. Only a few core gRPC services will be available
  // at this stage.
  ONLINE_BOOTING_UP,

  // All services are available and ready to take requests.
  ONLINE_ALL_SERVICES_AVAILABLE,
};

class ServicesStatusObserver : public base::CheckedObserver {
 public:
  virtual void OnServicesStatusChanged(ServicesStatus status) = 0;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_OBSERVER_H_
