// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TILE_SERVICE_FACTORY_HELPER_H_
#define COMPONENTS_QUERY_TILES_TILE_SERVICE_FACTORY_HELPER_H_

#include <memory>

#include "base/files/file_path.h"
#include "components/query_tiles/internal/tile_store.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace image_fetcher {
class ImageFetcherService;
}  // namespace image_fetcher

namespace background_task {
class BackgroundTaskScheduler;
}  // namespace background_task

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;

namespace query_tiles {

class TileService;

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
    PrefService* pref_service);

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_TILE_SERVICE_FACTORY_HELPER_H_
