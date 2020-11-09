// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/catalog_store.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/games/core/games_types.h"
#include "components/games/core/games_utils.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/test/mocks.h"
#include "components/games/core/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace games {

class CatalogStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_parser = std::make_unique<test::MockDataFilesParser>();
    mock_parser_ = mock_parser.get();
    catalog_store_ = std::make_unique<CatalogStore>(std::move(mock_parser));
    ASSERT_FALSE(catalog_store_->cached_catalog());
  }

  // We have to use TaskEnvironment since we post a task to the thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<CatalogStore> catalog_store_;
  test::MockDataFilesParser* mock_parser_;
  base::FilePath fake_install_dir_ =
      base::FilePath(FILE_PATH_LITERAL("some/path"));
};

TEST_F(CatalogStoreTest, UpdateCatalogAsync_Success_WithCache_AndClearCache) {
  GamesCatalog fake_catalog = test::CreateGamesCatalogWithOneGame();
  EXPECT_CALL(*mock_parser_, TryParseCatalog(fake_install_dir_))
      .Times(1)
      .WillOnce([&fake_catalog](const base::FilePath& install_dir) {
        return base::Optional<GamesCatalog>(fake_catalog);
      });

  base::RunLoop run_loop;
  catalog_store_->UpdateCatalogAsync(
      fake_install_dir_,
      base::BindLambdaForTesting([&run_loop](ResponseCode code) {
        EXPECT_EQ(ResponseCode::kSuccess, code);
        run_loop.Quit();
      }));

  run_loop.Run();

  ASSERT_TRUE(catalog_store_->cached_catalog());
  test::ExpectProtosEqual(fake_catalog, *catalog_store_->cached_catalog());

  catalog_store_->ClearCache();

  EXPECT_FALSE(catalog_store_->cached_catalog());
}

TEST_F(CatalogStoreTest, UpdateCatalogAsync_FileNotFound) {
  EXPECT_CALL(*mock_parser_, TryParseCatalog(fake_install_dir_))
      .WillOnce(
          [](const base::FilePath& install_dir) { return base::nullopt; });

  base::RunLoop run_loop;
  catalog_store_->UpdateCatalogAsync(
      fake_install_dir_,
      base::BindLambdaForTesting([&run_loop](ResponseCode code) {
        EXPECT_EQ(ResponseCode::kFileNotFound, code);
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(catalog_store_->cached_catalog());
}

TEST_F(CatalogStoreTest, UpdateCatalogAsync_InvalidData) {
  EXPECT_CALL(*mock_parser_, TryParseCatalog(fake_install_dir_))
      .WillOnce([](const base::FilePath& install_dir) {
        return base::Optional<GamesCatalog>(GamesCatalog());
      });

  base::RunLoop run_loop;
  catalog_store_->UpdateCatalogAsync(
      fake_install_dir_,
      base::BindLambdaForTesting([&run_loop](ResponseCode code) {
        EXPECT_EQ(ResponseCode::kInvalidData, code);
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(catalog_store_->cached_catalog());
}

}  // namespace games
