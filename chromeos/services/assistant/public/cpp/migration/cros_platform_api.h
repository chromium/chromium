// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_CROS_PLATFORM_API_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_CROS_PLATFORM_API_H_

#include "base/macros.h"

namespace assistant_client {
class NetworkProvider;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

// Platform API required by the Google assistant.
// Note that this no longer inherits from |assistant_client::PlatformApi|,
// because we are in the process of migrating its functionality from here to the
// Libassistant mojom service.
class CrosPlatformApi {
 public:
  CrosPlatformApi() = default;
  virtual ~CrosPlatformApi() = default;

  // Returns the network provider to be used by libassistant.
  virtual assistant_client::NetworkProvider& GetNetworkProvider() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosPlatformApi);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_CROS_PLATFORM_API_H_
