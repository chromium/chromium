// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/highlighted_games_store.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/games/core/games_types.h"
#include "components/games/core/games_utils.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"
#include "components/games/core/test/mocks.h"
#include "components/games/core/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace games {

class HighlightedGamesStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    ResetClock();

    auto mock_parser = std::make_unique<test::MockDataFilesParser>();
    mock_parser_ = mock_parser.get();

    highlighted_games_store_ = std::make_unique<HighlightedGamesStore>(
        std::move(mock_parser), &mock_clock_);
    AssertCacheEmpty();
  }

  void ResetClock() {
    base::Time fake_time;
    ASSERT_TRUE(
        base::Time::FromString("Wed, 16 Nov 1994, 00:00:00", &fake_time));
    mock_clock_.MockNow(fake_time);
  }

  void AssertCacheEmpty() {
    base::Optional<Game> test_cache =
        highlighted_games_store_->TryGetFromCache();
    ASSERT_FALSE(test_cache.has_value());
  }

  void AddValidHighlightedGame(HighlightedGamesResponse* response, int id) {
    // Set a highlighted game around the currently mocked time to make sure its
    // valid.
    HighlightedGame fake_highlighted_game;
    fake_highlighted_game.set_game_id(id);
    test::SetDateProtoTo(mock_clock_.Now() - base::TimeDelta::FromDays(1),
                         fake_highlighted_game.mutable_start_date());
    test::SetDateProtoTo(mock_clock_.Now() + base::TimeDelta::FromDays(1),
                         fake_highlighted_game.mutable_end_date());

    response->mutable_games()->Add(std::move(fake_highlighted_game));
  }

  void ExpectProcessAsyncFailure(ResponseCode expected_code,
                                 const GamesCatalog& catalog) {
    base::RunLoop run_loop;

    // We'll use the barrier closure to make sure both the pending callback and
    // the done callbacks were invoked upon success.
    auto barrier_closure = base::BarrierClosure(
        2, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

    highlighted_games_store_->SetPendingCallback(base::BindLambdaForTesting(
        [&expected_code, &barrier_closure](ResponseCode code, const Game game) {
          test::ExpectProtosEqual(Game(), game);
          EXPECT_EQ(expected_code, code);
          barrier_closure.Run();
        }));

    highlighted_games_store_->ProcessAsync(
        fake_install_dir_, catalog,
        base::BindLambdaForTesting(
            [&barrier_closure]() { barrier_closure.Run(); }));

    run_loop.Run();

    AssertCacheEmpty();
  }

  Game PopulateCache() {
    // Get a game to be cached.
    GamesCatalog fake_catalog = test::CreateCatalogWithTwoGames();
    Game fake_selected_game = fake_catalog.games().at(1);

    HighlightedGamesResponse fake_response;
    AddValidHighlightedGame(&fake_response, fake_selected_game.id());

    EXPECT_CALL(*mock_parser_, TryParseHighlightedGames(fake_install_dir_))
        .WillOnce([&fake_response](const base::FilePath& install_dir) {
          return base::Optional<HighlightedGamesResponse>(fake_response);
        });

    base::RunLoop run_loop;
    highlighted_games_store_->ProcessAsync(
        fake_install_dir_, fake_catalog,
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

    run_loop.Run();
    return fake_selected_game;
  }

  // TaskEnvironment is used instead of SingleThreadTaskEnvironment since we
  // post a task to the thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<HighlightedGamesStore> highlighted_games_store_;
  test::MockDataFilesParser* mock_parser_;
  test::MockClock mock_clock_;
  base::FilePath fake_install_dir_ =
      base::FilePath(FILE_PATH_LITERAL("some/path"));
};

TEST_F(HighlightedGamesStoreTest,
       ProcessAsync_Success_WithCache_AndCacheExpiry) {
  GamesCatalog fake_catalog = test::CreateCatalogWithTwoGames();
  Game fake_selected_game = fake_catalog.games().at(1);

  HighlightedGamesResponse fake_response;
  AddValidHighlightedGame(&fake_response, fake_selected_game.id());

  EXPECT_CALL(*mock_parser_, TryParseHighlightedGames(fake_install_dir_))
      .WillOnce([&fake_response](const base::FilePath& install_dir) {
        return base::Optional<HighlightedGamesResponse>(fake_response);
      });

  base::RunLoop run_loop;

  // We'll use the barrier closure to make sure both the pending callback and
  // the done callbacks were invoked upon success.
  auto barrier_closure = base::BarrierClosure(
      2, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  highlighted_games_store_->SetPendingCallback(
      base::BindLambdaForTesting([&barrier_closure, &fake_selected_game](
                                     ResponseCode code, const Game game) {
        test::ExpectProtosEqual(fake_selected_game, game);
        EXPECT_EQ(ResponseCode::kSuccess, code);
        barrier_closure.Run();
      }));

  highlighted_games_store_->ProcessAsync(fake_install_dir_, fake_catalog,
                                         barrier_closure);

  run_loop.Run();

  // Now the game should be cached.
  auto test_cache = highlighted_games_store_->TryGetFromCache();
  EXPECT_TRUE(test_cache);
  test::ExpectProtosEqual(fake_selected_game, test_cache.value());

  // Days going by...
  mock_clock_.AdvanceDays(4);

  // Now the highlighted game should be highlighted no more (we went past its
  // end date).
  AssertCacheEmpty();
}

TEST_F(HighlightedGamesStoreTest, ProcessAsync_EmptyCatalog) {
  GamesCatalog empty_catalog;
  base::RunLoop run_loop;

  // We'll use the barrier closure to make sure both the pending callback and
  // the done callbacks were invoked upon success.
  auto barrier_closure = base::BarrierClosure(
      2, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  EXPECT_CALL(*mock_parser_, TryParseHighlightedGames(fake_install_dir_))
      .WillOnce([](const base::FilePath& install_dir) {
        return base::Optional<HighlightedGamesResponse>(
            HighlightedGamesResponse());
      });

  highlighted_games_store_->SetPendingCallback(base::BindLambdaForTesting(
      [&barrier_closure](ResponseCode code, const Game game) {
        test::ExpectProtosEqual(Game(), game);
        EXPECT_EQ(ResponseCode::kInvalidData, code);
        barrier_closure.Run();
      }));

  highlighted_games_store_->ProcessAsync(fake_install_dir_, empty_catalog,
                                         barrier_closure);

  run_loop.Run();

  // Cache should be empty.
  AssertCacheEmpty();
}

TEST_F(HighlightedGamesStoreTest, ProcessAsync_NoCallback_Caches) {
  GamesCatalog fake_catalog = test::CreateCatalogWithTwoGames();
  Game fake_selected_game = fake_catalog.games().at(1);

  HighlightedGamesResponse fake_response;
  AddValidHighlightedGame(&fake_response, fake_selected_game.id());

  EXPECT_CALL(*mock_parser_, TryParseHighlightedGames(fake_install_dir_))
      .WillOnce([&fake_response](const base::FilePath& install_dir) {
        return base::Optional<HighlightedGamesResponse>(fake_response);
      });

  base::RunLoop run_loop;

  highlighted_games_store_->ProcessAsync(
      fake_install_dir_, fake_catalog,
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  run_loop.Run();

  // Even if we didn't have any pending callback, the game should now be cached.
  base::Optional<Game> test_cache = highlighted_games_store_->TryGetFromCache();
  ASSERT_TRUE(test_cache.has_value());
  test::ExpectProtosEqual(fake_selected_game, test_cache.value());
}

TEST_F(HighlightedGamesStoreTest, ProcessAsync_NoHighlightedGamesResponse) {
  GamesCatalog fake_catalog = test::CreateCatalogWithTwoGames();

  // Mock as if we couldn't find the highlighted games response data file.
  EXPECT_CALL(*mock_parser_, TryParseHighlightedGames(fake_install_dir_))
      .WillOnce(
          [](const base::FilePath& install_dir) { return base::nullopt; });

  ExpectProcessAsyncFailure(ResponseCode::kFileNotFound, fake_catalog);
}

TEST_F(HighlightedGamesStoreTest, ProcessAsync_CurrentGameIdNotFoundInCatalog) {
  GamesCatalog fake_catalog = test::CreateCatalogWithTwoGames();

  // No game has ID 99 in our fake catalog.
  HighlightedGamesResponse fake_response;
  AddValidHighlightedGame(&fake_response, 99);

  EXPECT_CALL(*mock_parser_, TryParseHighlightedGames(fake_install_dir_))
      .WillOnce([&fake_response](const base::FilePath& install_dir) {
        return base::Optional<HighlightedGamesResponse>(fake_response);
      });

  ExpectProcessAsyncFailure(ResponseCode::kInvalidData, fake_catalog);
}

TEST_F(HighlightedGamesStoreTest, ProcessAsync_NoCurrentGame) {
  GamesCatalog fake_catalog = test::CreateCatalogWithTwoGames();

  // Create a future HighlightedGame.
  HighlightedGame fake_highlighted_game;
  fake_highlighted_game.set_game_id(fake_catalog.games().at(0).id());
  test::SetDateProtoTo(mock_clock_.Now() + base::TimeDelta::FromDays(1),
                       fake_highlighted_game.mutable_start_date());
  test::SetDateProtoTo(mock_clock_.Now() + base::TimeDelta::FromDays(2),
                       fake_highlighted_game.mutable_end_date());

  HighlightedGamesResponse fake_response;
  fake_response.mutable_games()->Add(std::move(fake_highlighted_game));

  EXPECT_CALL(*mock_parser_, TryParseHighlightedGames(fake_install_dir_))
      .WillOnce([&fake_response](const base::FilePath& install_dir) {
        return base::Optional<HighlightedGamesResponse>(fake_response);
      });

  ExpectProcessAsyncFailure(ResponseCode::kInvalidData, fake_catalog);
}

TEST_F(HighlightedGamesStoreTest, HandleCatalogFailure_CallsCallback) {
  bool callback_called;
  ResponseCode expected_code = ResponseCode::kMissingCatalog;
  highlighted_games_store_->SetPendingCallback(base::BindLambdaForTesting(
      [&expected_code, &callback_called](ResponseCode code, const Game game) {
        EXPECT_EQ(expected_code, code);
        test::ExpectProtosEqual(Game(), game);
        callback_called = true;
      }));

  highlighted_games_store_->HandleCatalogFailure(expected_code);

  EXPECT_TRUE(callback_called);
}

TEST_F(HighlightedGamesStoreTest, HandleCatalogFailure_NoCallback) {
  // No exception should be thrown.
  highlighted_games_store_->HandleCatalogFailure(ResponseCode::kMissingCatalog);
}

TEST_F(HighlightedGamesStoreTest, TryRespondFromCache_NoCallbackNoCache) {
  EXPECT_FALSE(highlighted_games_store_->TryRespondFromCache());
}

TEST_F(HighlightedGamesStoreTest, TryRespondFromCache_NoCache) {
  highlighted_games_store_->SetPendingCallback(
      base::BindLambdaForTesting([](ResponseCode code, const Game game) {
        // Callback should not be invoked where there's no cached game.
        FAIL();
      }));
  EXPECT_FALSE(highlighted_games_store_->TryRespondFromCache());
}

TEST_F(HighlightedGamesStoreTest, TryRespondFromCache_NoCallback) {
  PopulateCache();
  EXPECT_FALSE(highlighted_games_store_->TryRespondFromCache());
}

TEST_F(HighlightedGamesStoreTest,
       TryRespondFromCache_CallbackAndCache_Success) {
  Game expected_game = PopulateCache();

  base::RunLoop run_loop;
  highlighted_games_store_->SetPendingCallback(base::BindLambdaForTesting(
      [&expected_game, &run_loop](ResponseCode code, const Game game) {
        test::ExpectProtosEqual(expected_game, game);
        EXPECT_EQ(ResponseCode::kSuccess, code);
        run_loop.Quit();
      }));

  EXPECT_TRUE(highlighted_games_store_->TryRespondFromCache());
  run_loop.Run();
}

}  // namespace games
