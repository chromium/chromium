// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_IN_PROCESS_INSTANCE_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_IN_PROCESS_INSTANCE_H_

#include "base/component_export.h"

namespace ash::network_health {

class NetworkHealthService;

COMPONENT_EXPORT(IN_PROCESS_NETWORK_HEALTH)
NetworkHealthService* GetInProcessInstance();

}  // namespace ash::network_health

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_IN_PROCESS_INSTANCE_H_
