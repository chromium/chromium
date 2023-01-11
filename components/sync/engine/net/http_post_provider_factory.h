// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NET_HTTP_POST_PROVIDER_FACTORY_H_
#define COMPONENTS_SYNC_ENGINE_NET_HTTP_POST_PROVIDER_FACTORY_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace syncer {

class HttpPostProvider;

// A factory to create HttpPostProviders to hide details about the
// implementations and dependencies.
// A factory instance itself should be owned by whomever uses it to create
// HttpPostProviders.
class HttpPostProviderFactory {
 public:
  virtual ~HttpPostProviderFactory() = default;

  // Obtain a new HttpPostProvider instance, owned by caller.
  virtual scoped_refptr<HttpPostProvider> Create() = 0;
};

using CreateHttpPostProviderFactory =
    base::RepeatingCallback<std::unique_ptr<HttpPostProviderFactory>(
        const std::string& user_agent,
        std::unique_ptr<network::PendingSharedURLLoaderFactory>
            pending_url_loader_factory)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NET_HTTP_POST_PROVIDER_FACTORY_H_
