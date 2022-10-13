// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_health/in_process_instance.h"

#include "base/no_destructor.h"
#include "chromeos/services/network_health/network_health_service.h"

namespace chromeos::network_health {

NetworkHealthService* GetInProcessInstance() {
  static base::NoDestructor<NetworkHealthService> instance;
  return instance.get();
}

}  // namespace chromeos::network_health
