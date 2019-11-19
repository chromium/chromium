// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_CONTAINER_CLIENT_ADAPTER_H_
#define COMPONENTS_ARC_SESSION_ARC_CONTAINER_CLIENT_ADAPTER_H_

#include <memory>

#include "components/arc/session/arc_client_adapter.h"

namespace arc {

// Returns an adapter for talking to session_manager via D-Bus.
std::unique_ptr<ArcClientAdapter> CreateArcContainerClientAdapter();

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_CONTAINER_CLIENT_ADAPTER_H_
