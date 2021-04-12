// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_

namespace chromeos {
namespace diagnostics {

class NetworkHealthProvider {
 public:
  NetworkHealthProvider();

  NetworkHealthProvider(const NetworkHealthProvider&) = delete;
  NetworkHealthProvider& operator=(const NetworkHealthProvider&) = delete;

  ~NetworkHealthProvider();
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_
