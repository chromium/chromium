// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_FACTORY_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

namespace network {
class NetworkConnectionTracker;
}

namespace syncer {

class SyncManager;

// Helper class to allow dependency injection of the SyncManager in tests.
class SyncManagerFactory {
 public:
  explicit SyncManagerFactory(
      network::NetworkConnectionTracker* network_connection_tracker);

  SyncManagerFactory(const SyncManagerFactory&) = delete;
  SyncManagerFactory& operator=(const SyncManagerFactory&) = delete;

  virtual ~SyncManagerFactory();

  virtual std::unique_ptr<SyncManager> CreateSyncManager(
      const std::string& name);

 private:
  const raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_FACTORY_H_
