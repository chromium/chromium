// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/query_tiles/internal/image_prefetcher.h"
#include "components/query_tiles/test/empty_logger.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace query_tiles {
namespace {

class MockTileManager : public TileManager {
 public:
  MockTileManager() = default;
  MOCK_METHOD(void, Init, (TileGroupStatusCallback));
  MOCK_METHOD(void, GetTiles, (bool, GetTilesCallback));
  MOCK_METHOD(void, GetTile, (const std::string&, bool, TileCallback));
  MOCK_METHOD(void,
              SaveTiles,
              (std::unique_ptr<TileGroup>, TileGroupStatusCallback));
  MOCK_METHOD(void, SetAcceptLanguagesForTesting, (const std::string&));
  MOCK_METHOD(TileGroupStatus, PurgeDb, ());
  MOCK_METHOD(TileGroup*, GetTileGroup, ());
  MOCK_METHOD(void, OnTileClicked, (const std::string&));
  MOCK_METHOD(void,
              OnQuerySelected,
              (const absl::optional<std::string>&, const std::u16string&));
};

class MockTileServiceScheduler : public TileServiceScheduler {
 public:
  MockTileServiceScheduler() = default;
  MOCK_METHOD(void, OnFetchStarted, ());
  MOCK_METHOD(void, OnFetchCompleted, (TileInfoRequestStatus));
  MOCK_METHOD(void, OnTileManagerInitialized, (TileGroupStatus));
  MOCK_METHOD(void, CancelTask, ());
  MOCK_METHOD(void, OnDbPurged, (TileGroupStatus));
  MOCK_METHOD(void, OnGroupDataSaved, (TileGroupStatus));
  MOCK_METHOD(void, SetDelegate, (TileServiceScheduler::Delegate*));
};

class MockImagePrefetcher : public ImagePrefetcher {
 public:
  MockImagePrefetcher() = default;
  ~MockImagePrefetcher() override = default;

  MOCK_METHOD(void, Prefetch, (TileGroup, bool, base::OnceClosure), (override));
};

}  // namespace

class TileServiceImplTest : public testing::Test {
 public:
  TileServiceImplTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~TileServiceImplTest() override = default;

  void SetUp() override {
    auto tile_manager = std::make_unique<MockTileManager>();
    tile_manager_ = tile_manager.get();
    auto image_prefetcher = std::make_unique<MockImagePrefetcher>();
    auto scheduler = std::make_unique<NiceMock<MockTileServiceScheduler>>();
    scheduler_ = scheduler.get();
    image_prefetcher_ = image_prefetcher.get();
    ON_CALL(*image_prefetcher_, Prefetch(_, _, _))
        .WillByDefault(Invoke([](TileGroup, bool, base::OnceClosure callback) {
          std::move(callback).Run();
        }));

    auto tile_fetcher =
        TileFetcher::Create(GURL("https://www.test.com"), "US", "en", "apikey",
                            "", "", test_shared_url_loader_factory_);
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.url.is_valid() && !request.url.is_empty());
          last_resource_request_ = request;
        }));
    auto logger = std::make_unique<test::EmptyLogger>();
    tile_service_impl_ = std::make_unique<TileServiceImpl>(
        std::move(image_prefetcher), std::move(tile_manager),
        std::move(scheduler), std::move(tile_fetcher), &clock_,
        std::move(logger));
  }

  void Initialize(bool expected_result) {
    base::RunLoop loop;
    tile_service_impl_->Initialize(base::BindOnce(
        &TileServiceImplTest::OnInitialized, base::Unretained(this),
        loop.QuitClosure(), expected_result));
    loop.Run();
  }

  void OnInitialized(base::RepeatingClosure closure,
                     bool expected_result,
                     bool success) {
    EXPECT_EQ(expected_result, success);
    std::move(closure).Run();
  }

  void OnGetTileDone(const std::string& expected_id,
                     absl::optional<Tile> actual_tile) {
    EXPECT_EQ(expected_id, actual_tile->id);
  }

  void OnGetTilesDone(size_t expected_size, std::vector<Tile> tiles) {
    EXPECT_EQ(expected_size, tiles.size());
  }

  void FetchForTilesSuceeded() {
    EXPECT_CALL(*scheduler(),
                OnFetchCompleted(TileInfoRequestStatus::kSuccess));
    tile_service_impl_->StartFetchForTiles(
        false, base::BindOnce(&TileServiceImplTest::OnFetchCompleted,
                              base::Unretained(this)));
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        last_resource_request_.url.spec(), "", net::HTTP_OK,
        network::TestURLLoaderFactory::kMostRecentMatch);
    task_environment_.RunUntilIdle();
  }

  void FetchForTilesWithError() {
    EXPECT_CALL(*scheduler(),
                OnFetchCompleted(TileInfoRequestStatus::kShouldSuspend));
    tile_service_impl_->StartFetchForTiles(
        false, base::BindOnce(&TileServiceImplTest::OnFetchCompleted,
                              base::Unretained(this)));
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        last_resource_request_.url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(net::HTTP_FORBIDDEN), std::string(),
        network::TestURLLoaderFactory::kMostRecentMatch);
    task_environment_.RunUntilIdle();
  }

  void OnFetchCompleted(bool reschedule) {
    // Once fetch completes, no reschedule should happen.
    EXPECT_FALSE(reschedule);
  }

  MockTileServiceScheduler* scheduler() { return scheduler_; }
  MockTileManager* tile_manager() { return tile_manager_; }
  MockImagePrefetcher* image_prefetcher() { return image_prefetcher_; }
  TileService* query_tiles_service() { return tile_service_impl_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  std::unique_ptr<TileServiceImpl> tile_service_impl_;
  raw_ptr<MockTileServiceScheduler> scheduler_;
  raw_ptr<MockTileManager> tile_manager_;
  raw_ptr<MockImagePrefetcher> image_prefetcher_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::ResourceRequest last_resource_request_;
};

// Tests that TileManager and TileServiceImpl both initialize successfully.
TEST_F(TileServiceImplTest, ManagerInitSucceeded) {
  EXPECT_CALL(*tile_manager(), Init(_))
      .WillOnce(Invoke([](base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kSuccess);
      }));
  Initialize(true);
}

// Tests that TileManager and TileServiceImpl both initialize successfully with
// no tiles.
TEST_F(TileServiceImplTest, ManagerInitSucceededWithNoTiles) {
  EXPECT_CALL(*scheduler(),
              OnTileManagerInitialized(TileGroupStatus::kNoTiles));
  EXPECT_CALL(*tile_manager(), Init(_))
      .WillOnce(Invoke([](base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kNoTiles);
      }));
  Initialize(true);
}

// Tests that TileManager fail to init, that causes TileServiceImpl to fail to
// initialize too.
TEST_F(TileServiceImplTest, ManagerInitFailed) {
  EXPECT_CALL(*tile_manager(), Init(_))
      .WillOnce(Invoke([](base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kFailureDbOperation);
      }));
  Initialize(false);
}

// Tests that tiles are successfully fetched and saved.
TEST_F(TileServiceImplTest, FetchForTilesSucceeded) {
  EXPECT_CALL(*tile_manager(), SaveTiles(_, _))
      .WillOnce(Invoke([](std::unique_ptr<TileGroup> tile_group,
                          base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kSuccess);
      }));
  EXPECT_CALL(*image_prefetcher(), Prefetch(_, _, _));

  FetchForTilesSuceeded();
}

// Tests that tiles failed to fetch.
TEST_F(TileServiceImplTest, FetchForTilesFailed) {
  FetchForTilesWithError();
}

TEST_F(TileServiceImplTest, CancelTask) {
  EXPECT_CALL(*scheduler(), CancelTask());
  query_tiles_service()->CancelTask();
}

TEST_F(TileServiceImplTest, GetTiles) {
  int expected_size = 5;
  EXPECT_CALL(*tile_manager(), GetTiles(true, _))
      .WillOnce(Invoke([&](bool, GetTilesCallback callback) {
        std::vector<Tile> out = std::vector<Tile>(expected_size, Tile());
        std::move(callback).Run(std::move(out));
      }));
  query_tiles_service()->GetQueryTiles(
      base::BindOnce(&TileServiceImplTest::OnGetTilesDone,
                     base::Unretained(this), expected_size));
}

TEST_F(TileServiceImplTest, GetTile) {
  std::string tile_id = "test-id";
  EXPECT_CALL(*tile_manager(), GetTile(tile_id, true, _))
      .WillOnce(Invoke([&](const std::string& id, bool, TileCallback callback) {
        EXPECT_EQ(id, tile_id);
        Tile out;
        out.id = tile_id;
        std::move(callback).Run(std::move(out));
      }));
  query_tiles_service()->GetTile(
      tile_id, base::BindOnce(&TileServiceImplTest::OnGetTileDone,
                              base::Unretained(this), tile_id));
}

TEST_F(TileServiceImplTest, PurgeDb) {
  EXPECT_CALL(*tile_manager(), PurgeDb());
  EXPECT_CALL(*tile_manager(), GetTiles(true, _));
  EXPECT_CALL(*scheduler(), OnDbPurged(_));
  query_tiles_service()->PurgeDb();
  query_tiles_service()->GetQueryTiles(base::BindOnce(
      &TileServiceImplTest::OnGetTilesDone, base::Unretained(this), 0));
}

}  // namespace query_tiles
