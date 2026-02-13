// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_BRIDGE_H_
#define COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_BRIDGE_H_

#include "base/component_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace enterprise_auth {

// Exposes URLSessionURLLoader::CreateAndStart for non-Objective-C++ files.
void COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH)
    CreateURLSessionURLLoaderAndStart(
        const network::ResourceRequest& request,
        mojo::PendingReceiver<network::mojom::URLLoader> loader,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client_info);

void COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH)
    CreateURLSessionURLLoaderAndStartForTesting(
        const network::ResourceRequest& request,
        mojo::PendingReceiver<network::mojom::URLLoader> loader,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client_info);

}  // namespace enterprise_auth

#endif  // COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_BRIDGE_H_
