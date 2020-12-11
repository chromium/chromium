// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_NET_SYNC_SERVER_CONNECTION_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_NET_SYNC_SERVER_CONNECTION_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "url/gurl.h"

namespace syncer {

class CancelationSignal;
class HttpPostProviderFactory;

// A ServerConnectionManager subclass that generates a POST object using an
// instance of the HttpPostProviderFactory class.
class SyncServerConnectionManager : public ServerConnectionManager {
 public:
  // |factory| and |cancelation_signal| must not be null, and the latter must
  // outlive this object.
  SyncServerConnectionManager(const GURL& sync_request_url,
                              std::unique_ptr<HttpPostProviderFactory> factory,
                              CancelationSignal* cancelation_signal);
  ~SyncServerConnectionManager() override;

  HttpResponse PostBuffer(const std::string& buffer_in,
                          const std::string& access_token,
                          std::string* buffer_out) override;

 private:
  // The full URL that requests will be made to.
  const GURL sync_request_url_;

  // A factory creating concrete HttpPostProviders for use whenever we need to
  // issue a POST to sync servers.
  std::unique_ptr<HttpPostProviderFactory> post_provider_factory_;

  // Cancelation signal is signalled when engine shuts down. Current blocking
  // operation should be aborted.
  CancelationSignal* const cancelation_signal_;

  DISALLOW_COPY_AND_ASSIGN(SyncServerConnectionManager);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_NET_SYNC_SERVER_CONNECTION_MANAGER_H_
