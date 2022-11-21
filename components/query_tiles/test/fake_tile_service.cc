// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/test/fake_tile_service.h"
#include "base/task/single_thread_task_runner.h"

#include <utility>
#include <vector>

namespace query_tiles {
namespace {

std::unique_ptr<Tile> CreateTile(const std::string& id,
                                 const std::string& display_text,
                                 const std::string& query_text,
                                 const std::string& url) {
  auto tile = std::make_unique<Tile>();
  tile->id = id;
  tile->display_text = display_text;
  tile->accessibility_text = display_text;
  tile->query_text = query_text;
  tile->image_metadatas.emplace_back(GURL(url));
  return tile;
}

void AddChild(Tile* parent, std::unique_ptr<Tile> child) {
  parent->sub_tiles.emplace_back(std::move(child));
}

std::vector<std::unique_ptr<Tile>> BuildFakeTree() {
  std::vector<std::unique_ptr<Tile>> top_tiles;
  auto tile1 = CreateTile("1", "News", "News",
                          "http://t0.gstatic.com/"
                          "images?q=tbn:ANd9GcTFlesDfqnMIxCvcotuKHBR-"
                          "U4cSOG1ceOcoitEOWuiRq9MqNn05e6agwcQHVXiQ3A");
  AddChild(tile1.get(),
           CreateTile("tile1_1", "India", "India",
                      "http://t2.gstatic.com/"
                      "images?q=tbn:ANd9GcTCr5Ene2snzAE_"
                      "tOxcZ6AlKrH8CLA4aYQYYLRepngj5oh5bwHagRF0ootjfRDlM1k"));
  auto tile2 = CreateTile("2", "Films", "Films",
                          "http://t1.gstatic.com/"
                          "images?q=tbn:"
                          "ANd9GcRuSbDebh0CCLeMEr2Wh8qmHpWSKdbqrZFWZsndsu7TMtPe"
                          "eNDYIKrQqexISQ4Bk0U");
  top_tiles.push_back(std::move(tile1));
  top_tiles.push_back(std::move(tile2));
  return top_tiles;
}

absl::optional<Tile> FindTile(std::vector<std::unique_ptr<Tile>>& tiles,
                              const std::string& id) {
  for (const auto& tile : tiles) {
    if (id == tile->id)
      return *tile.get();
  }

  for (const auto& tile : tiles) {
    for (const auto& child : tile->sub_tiles) {
      if (id == child->id)
        return *child.get();
    }
  }

  return absl::nullopt;
}

}  // namespace

FakeTileService::FakeTileService() : tiles_(BuildFakeTree()) {}

FakeTileService::~FakeTileService() = default;

void FakeTileService::GetQueryTiles(GetTilesCallback callback) {
  std::vector<Tile> tiles;
  for (auto& tile : tiles_)
    tiles.push_back(*tile.get());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(tiles)));
}

void FakeTileService::GetTile(const std::string& tile_id,
                              TileCallback callback) {
  auto tile = FindTile(tiles_, tile_id);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(tile)));
}

void FakeTileService::StartFetchForTiles(
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), false /*need_reschedule*/));
}

void FakeTileService::CancelTask() {}

void FakeTileService::PurgeDb() {}

void FakeTileService::SetServerUrl(const std::string& url) {}

void FakeTileService::OnTileClicked(const std::string& url) {}

void FakeTileService::OnQuerySelected(
    const absl::optional<std::string>& parent_tile_id,
    const std::u16string& query_text) {}

Logger* FakeTileService::GetLogger() {
  return nullptr;
}

}  // namespace query_tiles
