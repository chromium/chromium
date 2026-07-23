// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_SOCKET_FACTORY_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_SOCKET_FACTORY_H_

#include "base/functional/callback.h"
#include "services/network/public/mojom/socket_factory.mojom.h"

namespace openscreen_platform {

namespace SocketFactoryGetter {

using Callback = base::RepeatingCallback<network::mojom::SocketFactory*()>;

void Set(Callback socket_factory_getter);
void Clear();
bool IsSet();

}  // namespace SocketFactoryGetter

// This and all subsequent SocketFactory calls made must obey the thread safety
// requirements of `socket_factory_getter`.  This must be called each time a
// mojom::SocketFactory is needed; any returned pointer should not be stored
// beyond the scope in which it is received.
network::mojom::SocketFactory* GetSocketFactory();

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_SOCKET_FACTORY_H_
