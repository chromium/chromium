// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/query_tiles/internal/stats.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_iterator.h"
#include "components/query_tiles/internal/tile_manager.h"
#include "components/query_tiles/internal/tile_utils.h"
#include "components/query_tiles/internal/trending_tile_handler.h"
#include "components/query_tiles/switches.h"

namespace query_tiles {
namespace {

// A special tile group for tile stats.
constexpr char kTileStatsGroup[] = "tile_stats";

class TileManagerImpl : public TileManager {
 public:
  TileManagerImpl(std::unique_ptr<TileStore> store,
                  const std::string& accept_languages)
      : initialized_(false),
        store_(std::move(store)),
        accept_languages_(accept_languages) {}

 private:
  // TileManager implementation.
  void Init(TileGroupStatusCallback callback) override {
    store_->InitAndLoad(base::BindOnce(&TileManagerImpl::OnTileStoreInitialized,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
  }

  void SaveTiles(std::unique_ptr<TileGroup> group,
                 TileGroupStatusCallback callback) override {
    if (!initialized_) {
      std::move(callback).Run(TileGroupStatus::kUninitialized);
      return;
    }

    auto group_copy = *group;
    store_->Update(group_copy.id, group_copy,
                   base::BindOnce(&TileManagerImpl::OnGroupSaved,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(group), std::move(callback)));
  }

  void GetTiles(bool shuffle_tiles, GetTilesCallback callback) override {
    if (!tile_group_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::vector<Tile>()));
      return;
    }

    // First remove the inactive trending tiles.
    RemoveIdleTrendingTiles();
    // Now build the tiles to return. Don't filter the subtiles, as they are
    // only used for UMA purpose now.
    // TODO(qinmin): remove all subtiles before returning the result, as they
    // are not used.
    std::vector<Tile> tiles =
        trending_tile_handler_.FilterExtraTrendingTiles(tile_group_->tiles);

    if (shuffle_tiles)
      ShuffleTiles(&tiles, TileShuffler());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(tiles)));
  }

  void GetTile(const std::string& tile_id,
               bool shuffle_tiles,
               TileCallback callback) override {
    // First remove the inactive trending tiles.
    RemoveIdleTrendingTiles();
    // Find the tile.
    const Tile* result = nullptr;
    if (tile_group_) {
      TileIterator it(*tile_group_, TileIterator::kAllTiles);
      while (it.HasNext()) {
        const auto* tile = it.Next();
        DCHECK(tile);
        if (tile->id == tile_id) {
          result = tile;
          break;
        }
      }
    }
    auto result_tile = result ? absl::make_optional(*result) : absl::nullopt;
    if (result_tile.has_value()) {
      // Get the tiles to display, and convert the result vector.
      // TODO(qinmin): make GetTile() return a vector of sub tiles, rather than
      // the parent tile so we don't need the conversion below.
      std::vector<Tile> sub_tiles =
          trending_tile_handler_.FilterExtraTrendingTiles(
              result_tile->sub_tiles);
      if (!sub_tiles.empty()) {
        if (shuffle_tiles)
          ShuffleTiles(&sub_tiles, TileShuffler());
        std::vector<std::unique_ptr<Tile>> sub_tile_ptrs;
        for (auto& tile : sub_tiles)
          sub_tile_ptrs.emplace_back(std::make_unique<Tile>(std::move(tile)));
        result_tile->sub_tiles = std::move(sub_tile_ptrs);
      }
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result_tile)));
  }

  TileGroupStatus PurgeDb() override {
    if (!initialized_)
      return TileGroupStatus::kUninitialized;
    if (!tile_group_)
      return TileGroupStatus::kNoTiles;
    store_->Delete(tile_group_->id,
                   base::BindOnce(&TileManagerImpl::OnGroupDeleted,
                                  weak_ptr_factory_.GetWeakPtr()));
    tile_group_.reset();
    return TileGroupStatus::kNoTiles;
  }

  void SetAcceptLanguagesForTesting(
      const std::string& accept_languages) override {
    accept_languages_ = accept_languages;
  }

  TileGroup* GetTileGroup() override {
    return tile_group_ ? tile_group_.get() : nullptr;
  }

  void OnTileStoreInitialized(
      TileGroupStatusCallback callback,
      bool success,
      std::map<std::string, std::unique_ptr<TileGroup>> loaded_groups) {
    if (!success) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    TileGroupStatus::kFailureDbOperation));
      return;
    }

    initialized_ = true;

    PruneAndSelectGroup(std::move(callback), std::move(loaded_groups));
  }

  // Select the most recent unexpired group from |loaded_groups| with the
  // correct locale, and delete other groups.
  void PruneAndSelectGroup(
      TileGroupStatusCallback callback,
      std::map<std::string, std::unique_ptr<TileGroup>> loaded_groups) {
    TileGroupStatus status = TileGroupStatus::kSuccess;
    base::Time last_updated_time;
    std::string selected_group_id;
    for (const auto& pair : loaded_groups) {
      DCHECK(!pair.first.empty()) << "Should not have empty tile group key.";
      auto* group = pair.second.get();
      if (!group)
        continue;

      if (pair.first == kTileStatsGroup)
        continue;

      if (ValidateLocale(group) && !IsGroupExpired(group) &&
          (group->last_updated_ts > last_updated_time)) {
        last_updated_time = group->last_updated_ts;
        selected_group_id = pair.first;
      }
    }

    // Moves the selected group into in memory holder.
    if (!selected_group_id.empty()) {
      tile_group_ = std::move(loaded_groups[selected_group_id]);
      loaded_groups.erase(selected_group_id);
    } else {
      status = TileGroupStatus::kNoTiles;
    }

    // Keep the stats group in memory for tile score calculation.
    if (loaded_groups.find(kTileStatsGroup) != loaded_groups.end()) {
      tile_stats_group_ = std::move(loaded_groups[kTileStatsGroup]);
      // prevent the stats group from being deleted.
      loaded_groups.erase(kTileStatsGroup);
      if (tile_group_) {
        SortTilesAndClearUnusedStats(&tile_group_->tiles,
                                     &tile_stats_group_->tile_stats);
      }
    }
    trending_tile_handler_.Reset();

    // Deletes other groups.
    for (const auto& group_to_delete : loaded_groups)
      DeleteGroup(group_to_delete.first);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
  }

  // Returns true if the group is expired.
  bool IsGroupExpired(const TileGroup* group) const {
    if (base::Time::Now() >=
        group->last_updated_ts + TileConfig::GetExpireDuration()) {
      stats::RecordGroupPruned(stats::PrunedGroupReason::kExpired);
      return true;
    }
    return false;
  }

  // Check whether |locale_| matches with that of the |group|.
  bool ValidateLocale(const TileGroup* group) const {
    if (!accept_languages_.empty() && !group->locale.empty()) {
      // In case the primary language matches (en-GB vs en-IN), consider
      // those are matching.
      std::string group_primary =
          group->locale.substr(0, group->locale.find("-"));
      for (auto& lang :
           base::SplitString(accept_languages_, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY)) {
        if (lang.substr(0, lang.find("-")) == group_primary)
          return true;
      }
    }
    stats::RecordGroupPruned(stats::PrunedGroupReason::kInvalidLocale);
    return false;
  }

  void OnGroupSaved(std::unique_ptr<TileGroup> group,
                    TileGroupStatusCallback callback,
                    bool success) {
    if (!success) {
      std::move(callback).Run(TileGroupStatus::kFailureDbOperation);
      return;
    }

    // Only swap the in memory tile group when there is no existing tile group.
    if (!tile_group_) {
      tile_group_ = std::move(group);
      trending_tile_handler_.Reset();
    }

    std::move(callback).Run(TileGroupStatus::kSuccess);
  }

  void DeleteGroup(const std::string& key) {
    store_->Delete(key, base::BindOnce(&TileManagerImpl::OnGroupDeleted,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGroupDeleted(bool success) {
    // TODO(hesen): Record db operation metrics.
    NOTIMPLEMENTED();
  }

  void OnTileClicked(const std::string& tile_id) override {
    // If the tile stats haven't been created, create it here.
    if (!tile_stats_group_) {
      tile_stats_group_ = std::make_unique<TileGroup>();
      tile_stats_group_->id = kTileStatsGroup;
    }
    tile_stats_group_->OnTileClicked(tile_id);
    // It's fine if |tile_stats_group_| is not saved, so no callback needs to
    // be passed to Update().
    store_->Update(kTileStatsGroup, *tile_stats_group_, base::DoNothing());

    trending_tile_handler_.OnTileClicked(tile_id);
  }

  void OnQuerySelected(const absl::optional<std::string>& parent_tile_id,
                       const std::u16string& query_text) override {
    if (!tile_group_)
      return;

    // Find the parent tile first. If it cannot be found, that's fine as the
    // old tile score will be used.
    std::vector<std::unique_ptr<Tile>>* tiles = &tile_group_->tiles;
    if (parent_tile_id) {
      for (const auto& tile : tile_group_->tiles) {
        if (tile->id == parent_tile_id.value()) {
          tiles = &tile->sub_tiles;
          break;
        }
      }
    }
    // Now check if a sub tile has the same query text.
    for (const auto& tile : *tiles) {
      if (query_text == base::UTF8ToUTF16(tile->query_text)) {
        OnTileClicked(tile->id);
        break;
      }
    }
  }

  void RemoveIdleTrendingTiles() {
    if (!tile_group_)
      return;
    std::vector<std::string> tiles_to_remove =
        trending_tile_handler_.GetTrendingTilesToRemove();
    if (tiles_to_remove.empty())
      return;
    tile_group_->RemoveTiles(tiles_to_remove);
    store_->Update(tile_group_->id, *tile_group_, base::DoNothing());
  }

  // Indicates if the db is fully initialized, rejects calls if not.
  bool initialized_;

  // Storage layer of query tiles.
  std::unique_ptr<TileStore> store_;

  // The tile group in-memory holder.
  std::unique_ptr<TileGroup> tile_group_;

  // The tile group that contains stats for ranking all tiles.
  // TODO(qinmin): Having a separate TileGroup just for ranking the tiles
  // seems weird, probably do it through a separate store or use PrefService.
  std::unique_ptr<TileGroup> tile_stats_group_;

  // Accept languages from the PrefService. Used to check if tiles stored are of
  // the same language.
  std::string accept_languages_;

  // Object for managing trending tiles.
  TrendingTileHandler trending_tile_handler_;

  base::WeakPtrFactory<TileManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

TileManager::TileManager() = default;

std::unique_ptr<TileManager> TileManager::Create(
    std::unique_ptr<TileStore> tile_store,
    const std::string& locale) {
  return std::make_unique<TileManagerImpl>(std::move(tile_store), locale);
}

}  // namespace query_tiles
