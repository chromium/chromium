// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_store.h"
#include "components/query_tiles/switches.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;

namespace query_tiles {
namespace {

const char kGuid[] = "awesome_guid";

class MockTileStore : public Store<TileGroup> {
 public:
  MockTileStore() = default;
  ~MockTileStore() override = default;

  MOCK_METHOD(void, InitAndLoad, (TileStore::LoadCallback), (override));
  MOCK_METHOD(void,
              Update,
              (const std::string&, const TileGroup&, UpdateCallback),
              (override));
  MOCK_METHOD(void,
              Delete,
              (const std::string&, TileStore::DeleteCallback),
              (override));
};

class TileManagerTest : public testing::Test {
 public:
  TileManagerTest() = default;
  ~TileManagerTest() override = default;

  TileManagerTest(const TileManagerTest& other) = delete;
  TileManagerTest& operator=(const TileManagerTest& other) = delete;

  void SetUp() override {
    auto tile_store = std::make_unique<StrictMock<MockTileStore>>();
    tile_store_ = tile_store.get();
    current_time_ = base::Time::Now();
    manager_ = TileManager::Create(std::move(tile_store), "en-US");
  }

  // Initialize the store, compare the |expected_status| to the
  // actual returned status.
  void Init(TileGroupStatus expected_status, bool success = true) {
    InitWithData(expected_status, std::vector<TileGroup>(), success);
  }

  // Initialize the store and load |groups|, compare the |expected_status| to
  // the actual returned status.
  void InitWithData(TileGroupStatus expected_status,
                    std::vector<TileGroup> groups,
                    bool success = true) {
    MockTileStore::KeysAndEntries entries;
    for (const auto& group : groups) {
      entries[group.id] = std::make_unique<TileGroup>(group);
    }

    EXPECT_CALL(*tile_store(), InitAndLoad(_))
        .WillOnce(Invoke(
            [&](base::OnceCallback<void(bool, MockTileStore::KeysAndEntries)>
                    callback) {
              std::move(callback).Run(success, std::move(entries));
            }));

    base::RunLoop loop;
    manager()->Init(base::BindOnce(&TileManagerTest::OnInitCompleted,
                                   base::Unretained(this), loop.QuitClosure(),
                                   expected_status));
    loop.Run();
  }

  void OnInitCompleted(base::RepeatingClosure closure,
                       TileGroupStatus expected_status,
                       TileGroupStatus status) {
    EXPECT_EQ(status, expected_status);
    std::move(closure).Run();
  }

  TileGroup CreateValidGroup(const std::string& group_id,
                             const std::string& tile_id) {
    TileGroup group;
    group.last_updated_ts = current_time();
    group.id = group_id;
    group.locale = "en-US";
    Tile tile;
    tile.id = tile_id;
    group.tiles.emplace_back(std::make_unique<Tile>(tile));
    return group;
  }

  // Run SaveTiles call from manager_, compare the |expected_status| to the
  // actual returned status.
  void SaveTiles(std::vector<std::unique_ptr<Tile>> tiles,
                 TileGroupStatus expected_status) {
    auto group = CreateValidGroup(kGuid, "");
    group.tiles = std::move(tiles);
    SaveTiles(group, expected_status);
  }

  void SaveTiles(const TileGroup& group, TileGroupStatus expected_status) {
    base::RunLoop loop;
    manager()->SaveTiles(
        std::make_unique<TileGroup>(group),
        base::BindOnce(&TileManagerTest::OnTilesSaved, base::Unretained(this),
                       loop.QuitClosure(), expected_status));
    loop.Run();
  }

  void OnTilesSaved(base::RepeatingClosure closure,
                    TileGroupStatus expected_status,
                    TileGroupStatus status) {
    EXPECT_EQ(status, expected_status);
    std::move(closure).Run();
  }

  // Run GetTiles call from manager_, compare the |expected| to the actual
  // returned tiles.
  void GetTiles(std::vector<Tile> expected) {
    base::RunLoop loop;
    manager()->GetTiles(
        true, base::BindOnce(&TileManagerTest::OnTilesReturned,
                             base::Unretained(this), loop.QuitClosure(),
                             std::move(expected)));
    loop.Run();
  }

  void OnTilesReturned(base::RepeatingClosure closure,
                       std::vector<Tile> expected,
                       std::vector<Tile> tiles) {
    EXPECT_TRUE(test::AreTilesIdentical(expected, tiles));
    std::move(closure).Run();
  }

  void GetSingleTile(const std::string& id, absl::optional<Tile> expected) {
    base::RunLoop loop;
    manager()->GetTile(
        id, true,
        base::BindOnce(&TileManagerTest::OnGetTile, base::Unretained(this),
                       loop.QuitClosure(), std::move(expected)));
    loop.Run();
  }

  void OnGetTile(base::RepeatingClosure closure,
                 absl::optional<Tile> expected,
                 absl::optional<Tile> actual) {
    ASSERT_EQ(expected.has_value(), actual.has_value());
    if (expected.has_value())
      EXPECT_TRUE(test::AreTilesIdentical(expected.value(), actual.value()));
    std::move(closure).Run();
  }

  void OnTileClicked(const std::string& tile_id) {
    EXPECT_CALL(*tile_store(), Update(_, _, _))
        .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                            MockTileStore::UpdateCallback callback) {
          std::move(callback).Run(true);
        }));
    manager()->OnTileClicked(tile_id);
  }

 protected:
  TileManager* manager() { return manager_.get(); }
  MockTileStore* tile_store() { return tile_store_; }
  const base::Time current_time() const { return current_time_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TileManager> manager_;
  raw_ptr<MockTileStore> tile_store_;
  base::Time current_time_;
};

TEST_F(TileManagerTest, InitAndLoadWithDbOperationFailed) {
  Init(TileGroupStatus::kFailureDbOperation, false /*success*/);
  GetTiles(std::vector<Tile>() /*expect an empty result*/);
}

TEST_F(TileManagerTest, InitWithEmptyDb) {
  InitWithData(TileGroupStatus::kNoTiles, std::vector<TileGroup>());
  GetTiles(std::vector<Tile>() /*expect an empty result*/);
}

// Expired group will be deleted during initialization.
TEST_F(TileManagerTest, InitAndLoadWithInvalidGroup) {
  // Create an expired group.
  auto expired_group = CreateValidGroup("expired_group_id", "tile_id");
  expired_group.last_updated_ts = current_time() - base::Days(3);

  // Locale mismatch group.
  auto locale_mismatch_group =
      CreateValidGroup("locale_group_id", "locale_tile_id");
  locale_mismatch_group.locale = "";

  EXPECT_CALL(*tile_store(), Delete("expired_group_id", _));
  InitWithData(TileGroupStatus::kNoTiles, {expired_group});
  GetTiles(std::vector<Tile>());
}

// The most recent valid group will be selected during initialization.
TEST_F(TileManagerTest, InitAndLoadSuccess) {
  // Two valid groups are loaded, the most recent one will be selected.
  auto group1 = CreateValidGroup("group_id_1", "tile_id_1");
  group1.last_updated_ts -= base::Minutes(5);
  auto group2 = CreateValidGroup("group_id_2", "tile_id_2");
  const Tile expected = *group2.tiles[0];

  EXPECT_CALL(*tile_store(), Delete("group_id_1", _));
  InitWithData(TileGroupStatus::kSuccess, {group1, group2});
  GetTiles({expected});
}

// Failed to init an empty db, and save tiles call failed because of db is
// uninitialized. GetTiles should return empty result.
TEST_F(TileManagerTest, SaveTilesWhenUnintialized) {
  EXPECT_CALL(*tile_store(), Update(_, _, _)).Times(0);
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kFailureDbOperation, false /*success*/);

  auto tile_to_save = std::make_unique<Tile>();
  test::ResetTestEntry(tile_to_save.get());
  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(tile_to_save));

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kUninitialized);
  GetTiles(std::vector<Tile>() /*expect an empty result*/);
}

// Init with empty db successfully, and save tiles failed because of db
// operation failed. GetTiles should return empty result.
TEST_F(TileManagerTest, SaveTilesFailed) {
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(false);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  auto tile_to_save = std::make_unique<Tile>();
  test::ResetTestEntry(tile_to_save.get());
  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(tile_to_save));

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kFailureDbOperation);
  GetTiles(std::vector<Tile>() /*expect an empty result*/);
}

// Init with empty db successfully, and save tiles successfully. GetTiles should
// return the recent saved tiles, and no Delete call is executed.
TEST_F(TileManagerTest, SaveTilesSuccess) {
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  auto tile_to_save = std::make_unique<Tile>();
  auto expected_tile = std::make_unique<Tile>();
  test::ResetTestEntry(tile_to_save.get());
  test::ResetTestEntry(expected_tile.get());
  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(tile_to_save));
  std::vector<Tile> expected;
  expected.emplace_back(*expected_tile.get());

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kSuccess);
  GetTiles(std::move(expected));
}

// Init with store successfully. The store originally has entries loaded into
// memory. Save new tiles successfully. GetTiles should return original saved
// tiles.
TEST_F(TileManagerTest, SaveTilesStillReturnOldTiles) {
  TileGroup old_group = CreateValidGroup("old_group_id", "old_tile_id");
  const Tile old_tile = *old_group.tiles[0].get();
  InitWithData(TileGroupStatus::kSuccess, {old_group});

  EXPECT_CALL(*tile_store(), Update("new_group_id", _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));

  TileGroup new_group = CreateValidGroup("new_group_id", "new_tile_id");
  SaveTiles(new_group, TileGroupStatus::kSuccess);
  GetTiles({old_tile});
}

// Verifies GetTile(tile_id) API can return the right thing.
TEST_F(TileManagerTest, GetTileById) {
  TileGroup group;
  test::ResetTestGroup(&group, current_time());
  InitWithData(TileGroupStatus::kSuccess, {group});
  GetSingleTile("guid-1-1", *group.tiles[0]);
  GetSingleTile("id_not_exist", absl::nullopt);
}

// Verify that GetTiles will return empty result if no matching AcceptLanguages
// is found.
TEST_F(TileManagerTest, GetTilesWithoutMatchingAcceptLanguages) {
  manager()->SetAcceptLanguagesForTesting("zh");
  TileGroup group;
  test::ResetTestGroup(&group, current_time());

  EXPECT_CALL(*tile_store(), Delete("group_guid", _));
  InitWithData(TileGroupStatus::kNoTiles, {group});
  GetTiles(std::vector<Tile>());
}

// Verify that GetTiles will return a valid result if a matching AcceptLanguages
// is found.
TEST_F(TileManagerTest, GetTilesWithMatchingAcceptLanguages) {
  manager()->SetAcceptLanguagesForTesting("zh, en");
  TileGroup group = CreateValidGroup("group_id", "tile_id");
  const Tile tile = *group.tiles[0];

  InitWithData(TileGroupStatus::kSuccess, {group});
  GetTiles({tile});
}

TEST_F(TileManagerTest, PurgeDb) {
  TileGroup group;
  test::ResetTestGroup(&group, current_time());
  InitWithData(TileGroupStatus::kSuccess, {group});
  EXPECT_CALL(*tile_store(), Delete(group.id, _));
  manager()->PurgeDb();
  GetTiles(std::vector<Tile>() /*expect an empty result*/);
}

TEST_F(TileManagerTest, GetTileGroup) {
  TileGroup expected;
  test::ResetTestGroup(&expected, current_time());
  InitWithData(TileGroupStatus::kSuccess, {expected});

  TileGroup* actual = manager()->GetTileGroup();
  EXPECT_TRUE(test::AreTileGroupsIdentical(*actual, expected));
}

// Check that the right number of trending tiles are returned.
TEST_F(TileManagerTest, GetTilesWithTrendingTiles) {
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  std::vector<std::unique_ptr<Tile>> tiles_to_save =
      test::GetTestTrendingTileList();

  std::vector<Tile> expected;
  expected.emplace_back(*tiles_to_save[0].get());
  expected.emplace_back(*tiles_to_save[1].get());

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kSuccess);
  GetTiles(std::move(expected));
}

// Check that the getTiles() will return all trending subtiles.
TEST_F(TileManagerTest, GetTilesWithTrendingSubTiles) {
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  auto parent_tile = std::make_unique<Tile>();
  parent_tile->id = "parent";
  parent_tile->sub_tiles = test::GetTestTrendingTileList();

  // The last subtile will be removed from the result.
  std::vector<Tile> expected;
  expected.emplace_back(*parent_tile.get());

  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(parent_tile));
  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kSuccess);
  GetTiles(std::move(expected));
}

// Check that GetSingleTile() will filter and return the right number of
// trending subtiles.
TEST_F(TileManagerTest, GetSingleTileWithTrendingSubTiles) {
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  auto parent_tile = std::make_unique<Tile>();
  parent_tile->id = "parent";

  parent_tile->sub_tiles = test::GetTestTrendingTileList();

  absl::optional<Tile> parent_tile2 = absl::make_optional(*parent_tile.get());
  parent_tile2->sub_tiles.pop_back();

  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(parent_tile));

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kSuccess);
  GetSingleTile("parent", std::move(parent_tile2));
}

// Check that trending tiles get removed after inactivity.
TEST_F(TileManagerTest, TrendingTopTilesRemovedAfterShown) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  std::vector<std::unique_ptr<Tile>> tiles_to_save =
      test::GetTestTrendingTileList();

  std::vector<Tile> expected;
  expected.emplace_back(*tiles_to_save[0].get());
  expected.emplace_back(*tiles_to_save[1].get());
  Tile trending_3 = *tiles_to_save[2].get();

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kSuccess);
  GetTiles(expected);

  // Click the 2nd tile.
  OnTileClicked("trending_2");

  // Both the first and the second tile will be removed.
  expected.erase(expected.begin(), expected.begin() + 2);
  expected.emplace_back(std::move(trending_3));
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  GetTiles(expected);

  // The 3rd tile will be removed due to max impression threshold.
  expected.erase(expected.begin());
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  GetTiles(expected);
}

// Check that trending subtiles will not be removed if they are not displayed.
TEST_F(TileManagerTest, UnshownTrendingSubTilesNotRemoved) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  auto parent_tile = std::make_unique<Tile>();
  parent_tile->id = "parent";
  parent_tile->sub_tiles = test::GetTestTrendingTileList();

  // The last subtile will be removed from the result.
  std::vector<Tile> expected;
  expected.emplace_back(*parent_tile.get());

  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(parent_tile));

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kSuccess);
  GetTiles(expected);

  // Click the parent tile and then get top level tiles.
  OnTileClicked("parent");
  GetTiles(expected);

  // Get top level tiles again. Since sub tiles were never shown,
  // they will not be removed.
  OnTileClicked("parent");
  GetTiles(expected);
}

// Check that if OnTileClicked() is followed by GetTile(), impression is
// correctly counted.
TEST_F(TileManagerTest, GetSingleTileAfterOnTileClicked) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);
  Init(TileGroupStatus::kNoTiles);

  auto parent_tile = std::make_unique<Tile>();
  parent_tile->id = "parent";
  parent_tile->sub_tiles = test::GetTestTrendingTileList();

  // The last subtile will be removed from the result.
  std::vector<Tile> expected;
  expected.emplace_back(*parent_tile.get());
  Tile trending_3 = *(expected[0].sub_tiles[2]).get();

  absl::optional<Tile> get_single_tile_expected =
      absl::make_optional(*parent_tile.get());
  get_single_tile_expected->sub_tiles.pop_back();

  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(parent_tile));

  SaveTiles(std::move(tiles_to_save), TileGroupStatus::kSuccess);
  GetTiles(expected);

  // Click the parent tile to show the subtiles.
  OnTileClicked("parent");
  GetSingleTile("parent", get_single_tile_expected);

  // Click a trending tile will not reset its impression.
  OnTileClicked("trending_1");

  // The first two tiles will get removed.
  expected[0].sub_tiles.erase(expected[0].sub_tiles.begin(),
                              expected[0].sub_tiles.begin() + 2);
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  GetTiles(expected);

  get_single_tile_expected->sub_tiles.clear();
  get_single_tile_expected->sub_tiles.emplace_back(
      std::make_unique<Tile>(std::move(trending_3)));
  OnTileClicked("parent");
  GetSingleTile("parent", get_single_tile_expected);

  // Finally all tiles are removed.
  get_single_tile_expected->sub_tiles.clear();
  OnTileClicked("parent");
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  GetSingleTile("parent", get_single_tile_expected);
}

}  // namespace

}  // namespace query_tiles
