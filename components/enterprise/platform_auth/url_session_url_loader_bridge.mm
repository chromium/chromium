// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/platform_auth/url_session_url_loader_bridge.h"

#include "base/check_is_test.h"
#include "components/enterprise/platform_auth/url_session_url_loader.h"

namespace enterprise_auth {

void CreateURLSessionURLLoaderAndStart(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_info) {
  URLSessionURLLoader::CreateAndStart(request, std::move(loader),
                                      std::move(client_info));
}

void CreateURLSessionURLLoaderAndStartForTesting(  // IN-TEST
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_info) {
  CHECK_IS_TEST();
  URLSessionURLLoader::CreateAndStartForTesting(request,  // IN-TEST
                                                std::move(loader),
                                                std::move(client_info));
}

}  // namespace enterprise_auth
