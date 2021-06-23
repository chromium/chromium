// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_FACTORY_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace network {
class NetworkConnectionTracker;
}

namespace syncer {

class SyncManager;

// Helper class to allow dependency injection of the SyncManager in tests.
class SyncManagerFactory {
 public:
  SyncManagerFactory(
      network::NetworkConnectionTracker* network_connection_tracker);
  virtual ~SyncManagerFactory();

  virtual std::unique_ptr<SyncManager> CreateSyncManager(
      const std::string& name);

 private:
  network::NetworkConnectionTracker* network_connection_tracker_;

  DISALLOW_COPY_AND_ASSIGN(SyncManagerFactory);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_FACTORY_H_
