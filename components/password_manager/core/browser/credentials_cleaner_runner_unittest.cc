// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credentials_cleaner_runner.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class MockCredentialsCleaner : public CredentialsCleaner {
 public:
  MockCredentialsCleaner() = default;
  MockCredentialsCleaner(const MockCredentialsCleaner&) = delete;
  MockCredentialsCleaner& operator=(const MockCredentialsCleaner&) = delete;
  ~MockCredentialsCleaner() override = default;
  MOCK_METHOD0(NeedsCleaning, bool());
  MOCK_METHOD1(StartCleaning, void(Observer* observer));
};

class CredentialsCleanerRunnerTest : public ::testing::Test {
 public:
  CredentialsCleanerRunnerTest() = default;
  CredentialsCleanerRunnerTest(const CredentialsCleanerRunnerTest&) = delete;
  CredentialsCleanerRunnerTest& operator=(const CredentialsCleanerRunnerTest&) =
      delete;
  ~CredentialsCleanerRunnerTest() override = default;

 protected:
  CredentialsCleanerRunner* GetRunner() { return &cleaning_tasks_runner_; }

 private:
  CredentialsCleanerRunner cleaning_tasks_runner_;
};

// In this test we check that credential clean-ups runner executes the clean-up
// tasks in the order they were added.
TEST_F(CredentialsCleanerRunnerTest, NonEmptyTasks) {
  static constexpr int kCleanersCount = 5;

  auto* cleaning_tasks_runner = GetRunner();
  std::vector<MockCredentialsCleaner*> raw_cleaners;
  for (int i = 0; i < kCleanersCount; ++i) {
    auto cleaner = std::make_unique<MockCredentialsCleaner>();
    raw_cleaners.push_back(cleaner.get());
    EXPECT_CALL(*cleaner, NeedsCleaning).WillOnce(::testing::Return(true));
    cleaning_tasks_runner->MaybeAddCleaningTask(std::move(cleaner));
  }

  ::testing::InSequence dummy;
  for (MockCredentialsCleaner* cleaner : raw_cleaners) {
    EXPECT_CALL(*cleaner, StartCleaning(cleaning_tasks_runner));
  }

  cleaning_tasks_runner->StartCleaning();
  for (int i = 0; i < kCleanersCount; ++i) {
    cleaning_tasks_runner->CleaningCompleted();
  }

  EXPECT_FALSE(cleaning_tasks_runner->HasPendingTasks());
}

// In this test we check that StartCleaning() can be called again after the
// first one finished.
TEST_F(CredentialsCleanerRunnerTest, CanBeCalledAgainAfterFinished) {
  auto* cleaning_tasks_runner = GetRunner();

  auto cleaner1 = std::make_unique<MockCredentialsCleaner>();
  EXPECT_CALL(*cleaner1, NeedsCleaning).WillOnce(::testing::Return(true));

  auto cleaner2 = std::make_unique<MockCredentialsCleaner>();
  EXPECT_CALL(*cleaner2, NeedsCleaning).WillOnce(::testing::Return(true));

  ::testing::InSequence dummy;
  EXPECT_CALL(*cleaner1, StartCleaning(cleaning_tasks_runner));
  EXPECT_CALL(*cleaner2, StartCleaning(cleaning_tasks_runner));

  cleaning_tasks_runner->MaybeAddCleaningTask(std::move(cleaner1));
  cleaning_tasks_runner->StartCleaning();
  cleaning_tasks_runner->CleaningCompleted();  // cleaner1

  cleaning_tasks_runner->MaybeAddCleaningTask(std::move(cleaner2));
  cleaning_tasks_runner->StartCleaning();
  cleaning_tasks_runner->CleaningCompleted();  // cleaner2

  EXPECT_FALSE(cleaning_tasks_runner->HasPendingTasks());
}

// In this test we check that StartCleaning() can be called again before the
// first one finished.
TEST_F(CredentialsCleanerRunnerTest, CanBeCalledAgainBeforeFinished) {
  auto* cleaning_tasks_runner = GetRunner();

  auto cleaner1 = std::make_unique<MockCredentialsCleaner>();
  EXPECT_CALL(*cleaner1, NeedsCleaning).WillOnce(::testing::Return(true));

  auto cleaner2 = std::make_unique<MockCredentialsCleaner>();
  EXPECT_CALL(*cleaner2, NeedsCleaning).WillOnce(::testing::Return(true));

  ::testing::InSequence dummy;
  EXPECT_CALL(*cleaner1, StartCleaning(cleaning_tasks_runner));
  EXPECT_CALL(*cleaner2, StartCleaning(cleaning_tasks_runner));

  cleaning_tasks_runner->MaybeAddCleaningTask(std::move(cleaner1));
  cleaning_tasks_runner->StartCleaning();

  cleaning_tasks_runner->MaybeAddCleaningTask(std::move(cleaner2));
  cleaning_tasks_runner->StartCleaning();

  cleaning_tasks_runner->CleaningCompleted();  // cleaner1
  cleaning_tasks_runner->CleaningCompleted();  // cleaner2

  EXPECT_FALSE(cleaning_tasks_runner->HasPendingTasks());
}

}  // namespace password_manager
