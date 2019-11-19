// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_LOOPBACK_CONNECTION_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_LOOPBACK_CONNECTION_MANAGER_H_

#include <string>

#include "components/sync/engine_impl/loopback_server/loopback_server.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"

namespace syncer {

// This class implements the ServerConnectionManager interface for a local
// in-memory virtual connection to the LoopbackServer. Since there is no network
// connection to be established we only have to handle POSTs and they will
// always succeed.
class LoopbackConnectionManager : public ServerConnectionManager {
 public:
  explicit LoopbackConnectionManager(const base::FilePath& persistent_file);
  ~LoopbackConnectionManager() override;

 private:
  // Overridden ServerConnectionManager functions.
  bool PostBufferToPath(PostBufferParams* params,
                        const std::string& path,
                        const std::string& access_token) override;

  // The loopback server that will handle the requests locally.
  LoopbackServer loopback_server_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackConnectionManager);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_LOOPBACK_CONNECTION_MANAGER_H_
