// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_CONNECTION_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_CONNECTION_MANAGER_H_

#include <string>

#include "components/sync/engine/loopback_server/loopback_server.h"
#include "components/sync/engine/net/server_connection_manager.h"

namespace syncer {

// This class implements the ServerConnectionManager interface for a local
// in-memory virtual connection to the LoopbackServer. Since there is no network
// connection to be established we only have to handle POSTs and they will
// always succeed.
class LoopbackConnectionManager : public ServerConnectionManager {
 public:
  explicit LoopbackConnectionManager(const base::FilePath& persistent_file);

  LoopbackConnectionManager(const LoopbackConnectionManager&) = delete;
  LoopbackConnectionManager& operator=(const LoopbackConnectionManager&) =
      delete;

  ~LoopbackConnectionManager() override;

 private:
  // Overridden ServerConnectionManager functions.
  HttpResponse PostBuffer(const std::string& buffer_in,
                          const std::string& access_token,
                          std::string* buffer_out) override;

  // The loopback server that will handle the requests locally.
  LoopbackServer loopback_server_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_CONNECTION_MANAGER_H_
