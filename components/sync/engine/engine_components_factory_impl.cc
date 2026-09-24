// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/engine_components_factory_impl.h"

#include <map>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "components/sync/engine/backoff_delay_provider.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/engine/loopback_server/loopback_connection_manager.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/sync_server_connection_manager.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/engine/sync_scheduler_impl.h"
#include "components/sync/engine/syncer.h"

namespace syncer {
namespace {

constexpr char kSyncServerSyncPath[] = "/command/";

std::string StripTrailingSlash(const std::string& s) {
  int stripped_end_pos = s.size();
  if (s.at(stripped_end_pos - 1) == '/') {
    stripped_end_pos = stripped_end_pos - 1;
  }

  return s.substr(0, stripped_end_pos);
}

GURL MakeConnectionURL(const GURL& sync_server, const std::string& client_id) {
  CHECK_EQ(kSyncServerSyncPath[0], '/');
  std::string full_path =
      StripTrailingSlash(sync_server.GetPath()) + kSyncServerSyncPath;

  GURL::Replacements path_replacement;
  path_replacement.SetPathStr(full_path);
  return AppendSyncQueryString(sync_server.ReplaceComponents(path_replacement),
                               client_id);
}

}  // namespace

// static
std::unique_ptr<EngineComponentsFactoryImpl>
EngineComponentsFactoryImpl::CreateForServerSync(
    const Switches& switches,
    const GURL& service_url,
    HttpPostProviderFactoryGetter http_factory_getter) {
  CHECK(http_factory_getter);
  return base::WrapUnique(new EngineComponentsFactoryImpl(
      switches, service_url, std::move(http_factory_getter),
      /*local_sync_backend_folder=*/std::nullopt));
}

// static
std::unique_ptr<EngineComponentsFactoryImpl>
EngineComponentsFactoryImpl::CreateForLocalSync(
    const Switches& switches,
    const base::FilePath& local_sync_backend_folder) {
  return base::WrapUnique(new EngineComponentsFactoryImpl(
      switches, /*service_url=*/GURL(),
      /*http_factory_getter=*/HttpPostProviderFactoryGetter(),
      local_sync_backend_folder));
}

EngineComponentsFactoryImpl::EngineComponentsFactoryImpl(
    const Switches& switches,
    const GURL& service_url,
    HttpPostProviderFactoryGetter http_factory_getter,
    const std::optional<base::FilePath>& local_sync_backend_folder)
    : switches_(switches),
      service_url_(service_url),
      http_factory_getter_(std::move(http_factory_getter)),
      local_sync_backend_folder_(local_sync_backend_folder) {
  CHECK_NE(!http_factory_getter_.is_null(),
           local_sync_backend_folder_.has_value());
}

EngineComponentsFactoryImpl::~EngineComponentsFactoryImpl() = default;

std::unique_ptr<SyncScheduler> EngineComponentsFactoryImpl::BuildScheduler(
    const std::string& name,
    SyncCycleContext* context,
    CancelationSignal* cancelation_signal) {
  std::unique_ptr<BackoffDelayProvider> delay =
      (switches_.backoff_override == BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE)
          ? BackoffDelayProvider::WithShortInitialRetryOverride()
          : BackoffDelayProvider::FromDefaults();

  std::unique_ptr<SyncSchedulerImpl> scheduler =
      std::make_unique<SyncSchedulerImpl>(
          name, std::move(delay), context,
          std::make_unique<Syncer>(cancelation_signal),
          /*ignore_auth_credentials=*/local_sync_backend_folder_.has_value());
  if (switches_.force_short_nudge_delay_for_test) {
    scheduler->ForceShortNudgeDelayForTest();
  }
  return std::move(scheduler);
}

std::unique_ptr<SyncCycleContext> EngineComponentsFactoryImpl::BuildContext(
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
    SyncAccessTokenFetcher* sync_access_token_fetcher) {
  CHECK(!local_sync_backend_folder_.has_value() || !sync_access_token_fetcher);
  return std::make_unique<SyncCycleContext>(
      connection_manager, extensions_activity, listeners, debug_info_getter,
      data_type_registry, cache_guid, store_birthday, bag_of_chips,
      poll_interval, account_email, sync_access_token_fetcher);
}

std::unique_ptr<ServerConnectionManager>
EngineComponentsFactoryImpl::BuildConnectionManager(
    const std::string& cache_guid,
    CancelationSignal* cancelation_signal) {
  if (local_sync_backend_folder_.has_value()) {
    return std::make_unique<LoopbackConnectionManager>(
        *local_sync_backend_folder_);
  }

  CHECK(http_factory_getter_);
  return std::make_unique<SyncServerConnectionManager>(
      MakeConnectionURL(service_url_, cache_guid),
      std::move(http_factory_getter_).Run(), cancelation_signal);
}

}  // namespace syncer
