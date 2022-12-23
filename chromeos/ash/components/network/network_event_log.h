// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_EVENT_LOG_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_EVENT_LOG_H_

#include <string>

#include "base/component_export.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

class NetworkState;

// Returns a consistent network identifier for logs. If |network| is null
// returns "<none>".
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string NetworkId(const NetworkState* network);

// Returns a consistent network identifier for logs. Looks up the network by
// |service_path|. If no network is found, returns service_x for /service/x.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string NetworkPathId(const std::string& service_path);

// Returns a consistent network identifier for logs. Looks up the network by
// |guid|. If no network is found, returns |guid|.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string NetworkGuidId(const std::string& guid);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_EVENT_LOG_H_
