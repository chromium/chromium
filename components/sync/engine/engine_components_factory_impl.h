// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/sync/engine/engine_components_factory.h"
#include "url/gurl.h"

namespace syncer {

class HttpPostProviderFactory;

// An EngineComponentsFactory implementation designed for real production /
// normal use.
class EngineComponentsFactoryImpl : public EngineComponentsFactory {
 public:
  using HttpPostProviderFactoryGetter =
      base::OnceCallback<std::unique_ptr<HttpPostProviderFactory>()>;

  // Factory method for standard HTTP-based communication with the Sync server.
  static std::unique_ptr<EngineComponentsFactoryImpl> CreateForServerSync(
      const Switches& switches,
      const GURL& service_url,
      HttpPostProviderFactoryGetter http_factory_getter);

  // Factory method for local sync (LoopbackServer).
  static std::unique_ptr<EngineComponentsFactoryImpl> CreateForLocalSync(
      const Switches& switches,
      const base::FilePath& local_sync_backend_folder);

  EngineComponentsFactoryImpl(const EngineComponentsFactoryImpl&) = delete;
  EngineComponentsFactoryImpl& operator=(const EngineComponentsFactoryImpl&) =
      delete;
  ~EngineComponentsFactoryImpl() override;

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* cancelation_signal) override;

  std::unique_ptr<SyncCycleContext> BuildContext(
      ServerConnectionManager* connection_manager,
      ExtensionsActivity* extensions_activity,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      DataTypeRegistry* data_type_registry,
      const std::string& cache_guid,
      const std::string& store_birthday,
      const std::string& bag_of_chips,
      base::TimeDelta poll_interval,
      const std::string& account_email,
      SyncAccessTokenFetcher* sync_access_token_fetcher) override;

  std::unique_ptr<ServerConnectionManager> BuildConnectionManager(
      const std::string& cache_guid,
      CancelationSignal* cancelation_signal) override;

 private:
  EngineComponentsFactoryImpl(
      const Switches& switches,
      const GURL& service_url,
      HttpPostProviderFactoryGetter http_factory_getter,
      const std::optional<base::FilePath>& local_sync_backend_folder);

  const Switches switches_;
  const GURL service_url_;
  HttpPostProviderFactoryGetter http_factory_getter_;
  const std::optional<base::FilePath> local_sync_backend_folder_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_IMPL_H_
