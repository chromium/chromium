// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/tile_service_factory_helper.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/prefs/pref_service.h"
#include "components/query_tiles/internal/cached_image_loader.h"
#include "components/query_tiles/internal/image_prefetcher.h"
#include "components/query_tiles/internal/init_aware_tile_service.h"
#include "components/query_tiles/internal/logger_impl.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_fetcher.h"
#include "components/query_tiles/internal/tile_manager.h"
#include "components/query_tiles/internal/tile_service_impl.h"
#include "components/query_tiles/internal/tile_service_scheduler_impl.h"
#include "components/query_tiles/switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace query_tiles {
namespace {
const base::FilePath::CharType kTileDbName[] =
    FILE_PATH_LITERAL("UpboardingQueryTileDatabase");

void BuildBackoffPolicy(net::BackoffEntry::Policy* policy) {
  policy->num_errors_to_ignore = 0;
  policy->initial_delay_ms =
      TileConfig::GetBackoffPolicyArgsInitDelayInMs();  // 30 seconds.
  policy->maximum_backoff_ms =
      TileConfig::GetBackoffPolicyArgsMaxDelayInMs();  // 1 day.
  policy->multiply_factor = 2;
  policy->jitter_factor = 0.33;
  policy->entry_lifetime_ms = -1;
  policy->always_use_initial_delay = false;
}

}  // namespace

std::unique_ptr<TileService> CreateTileService(
    image_fetcher::ImageFetcherService* image_fetcher_service,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& storage_dir,
    background_task::BackgroundTaskScheduler* scheduler,
    const std::string& accepted_language,
    const std::string& country_code,
    const std::string& api_key,
    const std::string& client_version,
    const std::string& default_server_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service) {
  // Create image loader.
  auto* cached_image_fetcher = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
  auto* reduced_mode_image_fetcher = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kReducedMode);
  auto image_loader = std::make_unique<CachedImageLoader>(
      cached_image_fetcher, reduced_mode_image_fetcher);
  auto image_prefetcher = ImagePrefetcher::Create(
      TileConfig::GetImagePrefetchMode(), std::move(image_loader));

  auto* clock = base::DefaultClock::GetInstance();
  auto logger = std::make_unique<LoggerImpl>();

  // Create tile store and manager.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  base::FilePath tile_store_dir = storage_dir.Append(kTileDbName);
  auto tile_db = db_provider->GetDB<query_tiles::proto::TileGroup, TileGroup>(
      leveldb_proto::ProtoDbType::UPBOARDING_QUERY_TILE_STORE, tile_store_dir,
      task_runner);
  auto tile_store = std::make_unique<TileStore>(std::move(tile_db));
  auto tile_manager =
      TileManager::Create(std::move(tile_store), accepted_language);

  // Create fetcher.
  auto tile_fetcher = TileFetcher::Create(
      TileConfig::GetQueryTilesServerUrl(default_server_url, false),
      country_code, accepted_language, api_key,
      TileConfig::GetExperimentTag(country_code), client_version,
      url_loader_factory);

  // Wrap background task scheduler.
  auto policy = std::make_unique<net::BackoffEntry::Policy>();
  BuildBackoffPolicy(policy.get());
  auto tile_background_task_scheduler =
      std::make_unique<TileServiceSchedulerImpl>(
          scheduler, pref_service, clock, base::DefaultTickClock::GetInstance(),
          std::move(policy), logger.get());
  logger->SetLogSource(tile_background_task_scheduler.get());

  auto tile_service_impl = std::make_unique<TileServiceImpl>(
      std::move(image_prefetcher), std::move(tile_manager),
      std::move(tile_background_task_scheduler), std::move(tile_fetcher), clock,
      std::move(logger));
  return std::make_unique<InitAwareTileService>(std::move(tile_service_impl));
}

}  // namespace query_tiles
