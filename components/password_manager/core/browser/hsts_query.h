// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HSTS_QUERY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HSTS_QUERY_H_

#include "base/functional/callback_forward.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace url {
class Origin;
}

namespace password_manager {

enum class HSTSResult { kNo, kYes, kError };

using HSTSCallback = base::OnceCallback<void(HSTSResult)>;

// Checks asynchronously whether HTTP Strict Transport Security (HSTS) is active
// for the host of the given origin for a given network context.  Notifies
// |callback| with the result on the calling thread. Should be called from the
// thread the network context lives on (in things based on content/ the UI
// thread).
void PostHSTSQueryForHostAndNetworkContext(
    const url::Origin& origin,
    network::mojom::NetworkContext* network_context,
    HSTSCallback callback);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HSTS_QUERY_H_
