// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NET_HTTP_POST_PROVIDER_FACTORY_H_
#define COMPONENTS_SYNC_ENGINE_NET_HTTP_POST_PROVIDER_FACTORY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/sync/engine/net/network_time_update_callback.h"

namespace network {
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace syncer {

class HttpPostProviderInterface;

// A factory to create HttpPostProviders to hide details about the
// implementations and dependencies.
// A factory instance itself should be owned by whomever uses it to create
// HttpPostProviders.
class HttpPostProviderFactory {
 public:
  virtual ~HttpPostProviderFactory() {}

  // Obtain a new HttpPostProviderInterface instance, owned by caller.
  virtual HttpPostProviderInterface* Create() = 0;

  // When the provider is no longer needed (ready to be cleaned up), clients
  // must call Destroy().
  // This allows actual HttpPostProvider subclass implementations to be
  // reference counted, which is useful if a particular implementation uses
  // multiple threads to serve network requests.
  // TODO(crbug.com/951350): Either pass out unique_ptrs to providers, or make
  // the provider interface refcounted, to avoid this manual destruction.
  virtual void Destroy(HttpPostProviderInterface* http) = 0;
};

using CreateHttpPostProviderFactory =
    base::RepeatingCallback<std::unique_ptr<HttpPostProviderFactory>(
        const std::string& user_agent,
        std::unique_ptr<network::SharedURLLoaderFactoryInfo>
            url_loader_factory_info,
        const NetworkTimeUpdateCallback& network_time_update_callback)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NET_HTTP_POST_PROVIDER_FACTORY_H_
