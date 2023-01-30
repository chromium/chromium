// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace safe_browsing {

// This class owns the NetworkContext that is used for requests by Safe
// Browsing.
// All methods are called on the UI thread.
class SafeBrowsingNetworkContext {
 public:
  // `user_data_dir` and `network_context_params_factory` are used to construct
  // a URLRequestContext through the network service. `trigger_migration`
  // determines whether or not to migrate the Safe Browsing network context
  // data. See the documentation for this in network_context.mojom.
  // `trigger_migration` should be set to true if the Safe Browsing network
  // context will be hosted in a sandboxed network service.
  using NetworkContextParamsFactory =
      base::RepeatingCallback<network::mojom::NetworkContextParamsPtr()>;
  SafeBrowsingNetworkContext(
      const base::FilePath& user_data_dir,
      bool trigger_migration,
      NetworkContextParamsFactory network_context_params_factory);
  ~SafeBrowsingNetworkContext();

  // Returns a SharedURLLoaderFactory.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Returns a NetworkContext.
  network::mojom::NetworkContext* GetNetworkContext();

  // Flushes NetworkContext and URLLoaderFactory pipes.
  void FlushForTesting();

  // Called at shutdown to ensure that the URLLoaderFactory is cleaned up.
  void ServiceShuttingDown();

 private:
  class SharedURLLoaderFactory;

  scoped_refptr<SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_
