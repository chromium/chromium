// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
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
  using NetworkContextParamsFactory =
      base::RepeatingCallback<network::mojom::NetworkContextParamsPtr()>;

  // Creates and returns a `SafeBrowsingNetworkContext` (never `nullptr`).
  // When `is_off_the_record` is `true`, constructs an in-memory context with no
  // on-disk persistence (ignoring `user_data_dir` and `trigger_migration`).
  // When `is_off_the_record` is `false`, constructs a context backed by on-disk
  // storage under `user_data_dir` (which must not be empty).
  // `trigger_migration` determines whether or not to migrate the Safe Browsing
  // network context data when `is_off_the_record` is `false`; see the
  // documentation for this in `network_context.mojom`, and set it to `true` if
  // the Safe Browsing network context will be hosted in a sandboxed network
  // service. `network_context_params_factory` is used to construct a
  // `URLRequestContext` through the network service and must not be null.
  static std::unique_ptr<SafeBrowsingNetworkContext> Create(
      bool is_off_the_record,
      const base::FilePath& user_data_dir,
      bool trigger_migration,
      NetworkContextParamsFactory network_context_params_factory);

  ~SafeBrowsingNetworkContext();

  // Returns a SharedURLLoaderFactory.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Returns a `NetworkContext`, or `nullptr` after `ServiceShuttingDown()` has
  // been called.
  network::mojom::NetworkContext* GetNetworkContext();

  // Flushes NetworkContext and URLLoaderFactory pipes.
  void FlushForTesting();

  // Returns the `NetworkContextParams` constructed for this context.
  network::mojom::NetworkContextParamsPtr
  CreateNetworkContextParamsForTesting();

  // Called at shutdown to tear down the `NetworkContext` and `URLLoaderFactory`
  // pipes and prevent further `NetworkContext` creation.
  void ServiceShuttingDown();

 private:
  class SharedURLLoaderFactory;

  // Constructs an in-memory `SafeBrowsingNetworkContext` with no on-disk
  // persistence. `network_context_params_factory` must not be null.
  explicit SafeBrowsingNetworkContext(
      NetworkContextParamsFactory network_context_params_factory);

  // Constructs a `SafeBrowsingNetworkContext` backed by on-disk storage under
  // `user_data_dir` (which must not be empty). `trigger_migration` determines
  // whether or not to migrate the Safe Browsing network context data.
  // `network_context_params_factory` must not be null.
  SafeBrowsingNetworkContext(
      const base::FilePath& user_data_dir,
      bool trigger_migration,
      NetworkContextParamsFactory network_context_params_factory);

  scoped_refptr<SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_H_
