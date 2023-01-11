// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/init_aware_tile_service.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/query_tiles/internal/tile_service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::StrictMock;

namespace query_tiles {
namespace {

class MockInitializableTileService : public InitializableTileService {
 public:
  MockInitializableTileService() = default;
  ~MockInitializableTileService() override = default;

  void Initialize(SuccessCallback callback) override {
    init_callback_ = std::move(callback);
  }

  void InvokeInitCallback(bool success) {
    DCHECK(init_callback_);
    std::move(init_callback_).Run(success);
  }

  // InitializableTileService implementation.
  MOCK_METHOD(void, GetQueryTiles, (GetTilesCallback), (override));
  MOCK_METHOD(void, GetTile, (const std::string&, TileCallback), (override));
  MOCK_METHOD(void,
              StartFetchForTiles,
              (bool, BackgroundTaskFinishedCallback),
              (override));
  MOCK_METHOD(void, CancelTask, (), (override));
  MOCK_METHOD(void, PurgeDb, (), (override));
  MOCK_METHOD(Logger*, GetLogger, (), (override));
  MOCK_METHOD(void, SetServerUrl, (const std::string&), (override));
  MOCK_METHOD(void, OnTileClicked, (const std::string&), (override));
  MOCK_METHOD(void,
              OnQuerySelected,
              (const absl::optional<std::string>&, const std::u16string&),
              (override));

  // Callback stubs.
  MOCK_METHOD(void, GetTilesCallbackStub, (TileList), ());
  MOCK_METHOD(void, TileCallbackStub, (absl::optional<Tile>), ());
  MOCK_METHOD(void, BackgroundTaskFinishedCallbackStub, (bool), ());

 private:
  SuccessCallback init_callback_;
};

class InitAwareTileServiceTest : public testing::Test {
 public:
  InitAwareTileServiceTest() : mock_service_(nullptr) {}
  ~InitAwareTileServiceTest() override = default;

 protected:
  void SetUp() override {
    auto mock_service =
        std::make_unique<StrictMock<MockInitializableTileService>>();
    mock_service_ = mock_service.get();
    init_aware_service_ =
        std::make_unique<InitAwareTileService>(std::move(mock_service));

    ON_CALL(*mock_service_, GetQueryTiles(_))
        .WillByDefault(Invoke([](GetTilesCallback callback) {
          std::move(callback).Run({Tile()});
        }));
    ON_CALL(*mock_service_, GetTile(_, _))
        .WillByDefault(Invoke([](const std::string&, TileCallback callback) {
          std::move(callback).Run(Tile());
        }));
    ON_CALL(*mock_service_, StartFetchForTiles(_, _))
        .WillByDefault(
            Invoke([](bool, BackgroundTaskFinishedCallback callback) {
              std::move(callback).Run(true);
            }));
  }

 protected:
  TileService* init_aware_service() {
    DCHECK(init_aware_service_);
    return init_aware_service_.get();
  }

  MockInitializableTileService* mock_service() {
    DCHECK(mock_service_);
    return mock_service_;
  }

  void InvokeInitCallback(bool success) {
    mock_service_->InvokeInitCallback(success);
  }

  void GetQueryTiles() {
    auto callback =
        base::BindOnce(&MockInitializableTileService::GetTilesCallbackStub,
                       base::Unretained(mock_service_));
    init_aware_service()->GetQueryTiles(std::move(callback));
  }

  void GetTile() {
    auto callback =
        base::BindOnce(&MockInitializableTileService::TileCallbackStub,
                       base::Unretained(mock_service_));
    init_aware_service()->GetTile("id", std::move(callback));
  }

  void StartFetchForTiles() {
    auto callback = base::BindOnce(
        &MockInitializableTileService::BackgroundTaskFinishedCallbackStub,
        base::Unretained(mock_service_));
    init_aware_service()->StartFetchForTiles(false /*is_from_reduced_mode*/,
                                             std::move(callback));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockInitializableTileService> mock_service_;
  std::unique_ptr<InitAwareTileService> init_aware_service_;
};

// API calls invoked after successful initialization should just pass through.
TEST_F(InitAwareTileServiceTest, AfterInitSuccessPassThrough) {
  InvokeInitCallback(true /*success*/);
  {
    InSequence sequence;
    EXPECT_CALL(*mock_service(), GetQueryTiles(_));
    EXPECT_CALL(*mock_service(), GetTile(_, _));
    EXPECT_CALL(*mock_service(), StartFetchForTiles(false, _));
  }

  EXPECT_CALL(*mock_service(), GetTilesCallbackStub(TileList({Tile()})));
  EXPECT_CALL(*mock_service(), TileCallbackStub(absl::make_optional<Tile>()));
  EXPECT_CALL(*mock_service(), BackgroundTaskFinishedCallbackStub(true));

  GetQueryTiles();
  GetTile();
  StartFetchForTiles();
  RunUntilIdle();
}

// API calls invoked after failed initialization should not pass through.
TEST_F(InitAwareTileServiceTest, AfterInitFailureNotPassThrough) {
  InvokeInitCallback(false /*success*/);
  {
    InSequence sequence;
    EXPECT_CALL(*mock_service(), GetQueryTiles(_)).Times(0);
    EXPECT_CALL(*mock_service(), GetTile(_, _)).Times(0);
    EXPECT_CALL(*mock_service(), StartFetchForTiles(_, _)).Times(0);
  }

  EXPECT_CALL(*mock_service(), GetTilesCallbackStub(TileList()));
  EXPECT_CALL(*mock_service(), TileCallbackStub(absl::optional<Tile>()));
  EXPECT_CALL(*mock_service(), BackgroundTaskFinishedCallbackStub(false));

  GetQueryTiles();
  GetTile();
  StartFetchForTiles();
  RunUntilIdle();
}

// API calls invoked before successful initialization should be flushed through.
TEST_F(InitAwareTileServiceTest, BeforeInitSuccessFlushedThrough) {
  {
    InSequence sequence;
    EXPECT_CALL(*mock_service(), GetQueryTiles(_));
    EXPECT_CALL(*mock_service(), GetTile(_, _));
    EXPECT_CALL(*mock_service(), StartFetchForTiles(false, _));
  }

  EXPECT_CALL(*mock_service(), GetTilesCallbackStub(TileList({Tile()})));
  EXPECT_CALL(*mock_service(), TileCallbackStub(absl::make_optional<Tile>()));
  EXPECT_CALL(*mock_service(), BackgroundTaskFinishedCallbackStub(true));

  GetQueryTiles();
  GetTile();
  StartFetchForTiles();
  InvokeInitCallback(true /*success*/);
  RunUntilIdle();
}

// API calls invoked before failed initialization should not be flushed through.
TEST_F(InitAwareTileServiceTest, BeforeInitFailureNotFlushedThrough) {
  {
    InSequence sequence;
    EXPECT_CALL(*mock_service(), GetQueryTiles(_)).Times(0);
    EXPECT_CALL(*mock_service(), GetTile(_, _)).Times(0);
    EXPECT_CALL(*mock_service(), StartFetchForTiles(_, _)).Times(0);
  }

  EXPECT_CALL(*mock_service(), GetTilesCallbackStub(TileList()));
  EXPECT_CALL(*mock_service(), TileCallbackStub(absl::optional<Tile>()));
  EXPECT_CALL(*mock_service(), BackgroundTaskFinishedCallbackStub(false));

  GetQueryTiles();
  GetTile();
  StartFetchForTiles();
  InvokeInitCallback(false /*success*/);
  RunUntilIdle();
}

}  // namespace
}  // namespace query_tiles
