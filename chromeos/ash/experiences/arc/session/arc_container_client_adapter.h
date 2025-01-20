// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_SESSION_ARC_CONTAINER_CLIENT_ADAPTER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_SESSION_ARC_CONTAINER_CLIENT_ADAPTER_H_

#include <memory>

#include "chromeos/ash/experiences/arc/session/arc_client_adapter.h"

namespace arc {

// Returns an adapter for talking to session_manager via D-Bus.
std::unique_ptr<ArcClientAdapter> CreateArcContainerClientAdapter();

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_SESSION_ARC_CONTAINER_CLIENT_ADAPTER_H_
