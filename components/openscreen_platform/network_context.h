// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_NETWORK_CONTEXT_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_NETWORK_CONTEXT_H_

#include "base/functional/bind.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace openscreen_platform {

void SetNetworkContextGetter(
    network::NetworkContextGetter network_context_getter);
void ClearNetworkContextGetter();
bool HasNetworkContextGetter();

// This and all subsequent NetworkContext calls made must obey the thread safety
// requirements of |network_context_getter|.  This must be called each time a
// mojom::NetworkContext is needed; any returned pointer should not be stored
// beyond the scope in which it is received.
//
// In Chrome, the |network_context_getter| will always return the NetworkContext
// from the SystemNetworkContextManager; therefore, GetNetworkContext must be
// called on the UI thread.
network::mojom::NetworkContext* GetNetworkContext();

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_NETWORK_CONTEXT_H_
