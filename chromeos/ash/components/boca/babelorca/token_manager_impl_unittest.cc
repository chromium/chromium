// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/token_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_fetcher.h"
#include "chromeos/ash/components/boca/babelorca/token_data_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kOAuthToken[] = "test-token";

TEST(TokenManagerImplTest, TokenInitiallyEmpty) {
  TokenManagerImpl token_manager(std::make_unique<FakeTokenFetcher>());

  EXPECT_THAT(token_manager.GetTokenString(), testing::IsNull());
  EXPECT_EQ(token_manager.GetFetchedVersion(), 0);
}

TEST(TokenManagerImplTest, RepsondOnFetchAndStoreToken) {
  auto token_fetcher = std::make_unique<FakeTokenFetcher>();
  auto* token_fetcher_ptr = token_fetcher.get();
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  TokenManagerImpl token_manager(std::move(token_fetcher), base::Seconds(5),
                                 &test_clock);
  base::test::TestFuture<bool> test_future;

  token_manager.ForceFetchToken(test_future.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::make_optional(
      TokenDataWrapper(kOAuthToken, test_clock.Now() + base::Seconds(10))));
  test_clock.Advance(base::Seconds(4));

  EXPECT_TRUE(test_future.Get());
  ASSERT_THAT(token_manager.GetTokenString(), testing::NotNull());
  EXPECT_THAT(*(token_manager.GetTokenString()), testing::StrEq(kOAuthToken));
  EXPECT_EQ(token_manager.GetFetchedVersion(), 1);
}

TEST(TokenManagerImplTest, ReturnNullIfTokenExpired) {
  auto token_fetcher = std::make_unique<FakeTokenFetcher>();
  auto* token_fetcher_ptr = token_fetcher.get();
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  TokenManagerImpl token_manager(std::move(token_fetcher), base::Seconds(5),
                                 &test_clock);
  base::test::TestFuture<bool> test_future;

  token_manager.ForceFetchToken(test_future.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::make_optional(
      TokenDataWrapper(kOAuthToken, test_clock.Now() + base::Seconds(10))));
  test_clock.Advance(base::Seconds(6));

  EXPECT_TRUE(test_future.Get());
  EXPECT_THAT(token_manager.GetTokenString(), testing::IsNull());
  EXPECT_EQ(token_manager.GetFetchedVersion(), 1);
}

TEST(TokenManagerImplTest, QueueRequestsUntilFetchIsComplete) {
  auto token_fetcher = std::make_unique<FakeTokenFetcher>();
  auto* token_fetcher_ptr = token_fetcher.get();
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  TokenManagerImpl token_manager(std::move(token_fetcher), base::Seconds(5),
                                 &test_clock);
  base::test::TestFuture<bool> test_future1;
  base::test::TestFuture<bool> test_future2;

  token_manager.ForceFetchToken(test_future1.GetCallback());
  token_manager.ForceFetchToken(test_future2.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::make_optional(
      TokenDataWrapper(kOAuthToken, test_clock.Now() + base::Seconds(10))));

  EXPECT_TRUE(test_future1.Get());
  EXPECT_TRUE(test_future2.Get());
  ASSERT_THAT(token_manager.GetTokenString(), testing::NotNull());
  EXPECT_THAT(*(token_manager.GetTokenString()), testing::StrEq(kOAuthToken));
  EXPECT_EQ(token_manager.GetFetchedVersion(), 1);
}

TEST(TokenManagerImplTest, RespondWithFalseOnFetchFailure) {
  auto token_fetcher = std::make_unique<FakeTokenFetcher>();
  auto* token_fetcher_ptr = token_fetcher.get();
  TokenManagerImpl token_manager(std::move(token_fetcher));
  base::test::TestFuture<bool> test_future;

  token_manager.ForceFetchToken(test_future.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::nullopt);

  EXPECT_FALSE(test_future.Get());
}

TEST(TokenManagerImplTest, IncrementVersionOnNewTokenFetch) {
  auto token_fetcher = std::make_unique<FakeTokenFetcher>();
  auto* token_fetcher_ptr = token_fetcher.get();
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  TokenManagerImpl token_manager(std::move(token_fetcher), base::Seconds(5),
                                 &test_clock);
  base::test::TestFuture<bool> test_future1;
  base::test::TestFuture<bool> test_future2;

  token_manager.ForceFetchToken(test_future1.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::make_optional(
      TokenDataWrapper(kOAuthToken, test_clock.Now() + base::Seconds(10))));
  token_manager.ForceFetchToken(test_future2.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::make_optional(
      TokenDataWrapper(kOAuthToken, test_clock.Now() + base::Seconds(10))));

  EXPECT_TRUE(test_future1.Get());
  EXPECT_TRUE(test_future2.Get());
  ASSERT_THAT(token_manager.GetTokenString(), testing::NotNull());
  EXPECT_THAT(*(token_manager.GetTokenString()), testing::StrEq(kOAuthToken));
  EXPECT_EQ(token_manager.GetFetchedVersion(), 2);
}

TEST(TokenManagerImplTest, DoesNotOverwriteTokenOnFailure) {
  auto token_fetcher = std::make_unique<FakeTokenFetcher>();
  auto* token_fetcher_ptr = token_fetcher.get();
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  TokenManagerImpl token_manager(std::move(token_fetcher), base::Seconds(5),
                                 &test_clock);
  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<bool> fail_future;

  // Success.
  token_manager.ForceFetchToken(success_future.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::make_optional(
      TokenDataWrapper(kOAuthToken, test_clock.Now() + base::Seconds(10))));
  // Failure.
  token_manager.ForceFetchToken(fail_future.GetCallback());
  token_fetcher_ptr->RespondToFetchRequest(std::nullopt);

  EXPECT_TRUE(success_future.Get());
  EXPECT_FALSE(fail_future.Get());
  ASSERT_THAT(token_manager.GetTokenString(), testing::NotNull());
  EXPECT_THAT(*(token_manager.GetTokenString()), testing::StrEq(kOAuthToken));
  EXPECT_EQ(token_manager.GetFetchedVersion(), 1);
}

}  // namespace
}  // namespace ash::babelorca
