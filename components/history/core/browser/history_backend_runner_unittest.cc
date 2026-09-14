// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_backend_runner.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

class TestHistoryBackendDelegate : public HistoryBackend::Delegate {
 public:
  TestHistoryBackendDelegate() = default;
  ~TestHistoryBackendDelegate() override = default;

  bool CanAddURL(const GURL& url) const override { return true; }
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {}
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(VisitedURLInfo visited_url_info) override {}
  void NotifyURLsModified(const URLRows& changed_urls) override {}
  void NotifyDeletions(DeletionInfo deletion_info) override {}
  void NotifyVisitedLinksAdded(const HistoryAddPageArgs& args) override {}
  void NotifyVisitedLinksDeleted(
      const std::vector<DeletedVisitedLink>& links) override {}
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const std::u16string& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}
};

}  // namespace

class HistoryBackendRunnerTest : public testing::Test {
 public:
  HistoryBackendRunnerTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    backend_ = base::MakeRefCounted<HistoryBackend>(
        std::make_unique<TestHistoryBackendDelegate>(), nullptr, task_runner_);
  }

  void TearDown() override {
    if (backend_) {
      backend_->Closing();
      backend_.reset();
    }
    if (task_runner_) {
      task_runner_->RunPendingTasks();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<HistoryBackend> backend_;
};

TEST_F(HistoryBackendRunnerTest, NonDeferredSchedulesImmediately) {
  HistoryDatabaseParams params(temp_dir_.GetPath(), 0, 0,
                               version_info::Channel::UNKNOWN);

  EXPECT_EQ(task_runner_->NumPendingTasks(), 0u);
  HistoryBackendRunner runner(task_runner_, backend_, /*no_db=*/false, params,
                              /*defer_init=*/false);
  EXPECT_TRUE(runner.is_init_scheduled());
  EXPECT_EQ(task_runner_->NumPendingTasks(), 1u);

  // Subsequent calls should be no-ops.
  runner.EnsureInitScheduled();
  EXPECT_EQ(task_runner_->NumPendingTasks(), 1u);
}

TEST_F(HistoryBackendRunnerTest, DeferredSchedulesOnExplicitCall) {
  HistoryDatabaseParams params(temp_dir_.GetPath(), 0, 0,
                               version_info::Channel::UNKNOWN);

  HistoryBackendRunner runner(task_runner_, backend_, /*no_db=*/false, params,
                              /*defer_init=*/true);
  EXPECT_FALSE(runner.is_init_scheduled());
  EXPECT_EQ(task_runner_->NumPendingTasks(), 0u);

  runner.EnsureInitScheduled();
  EXPECT_TRUE(runner.is_init_scheduled());
  EXPECT_EQ(task_runner_->NumPendingTasks(), 1u);

  // Subsequent calls should be no-ops.
  runner.EnsureInitScheduled();
  EXPECT_EQ(task_runner_->NumPendingTasks(), 1u);
}

TEST_F(HistoryBackendRunnerTest, DeferredSchedulesOnGetTaskRunner) {
  HistoryDatabaseParams params(temp_dir_.GetPath(), 0, 0,
                               version_info::Channel::UNKNOWN);

  HistoryBackendRunner runner(task_runner_, backend_, /*no_db=*/false, params,
                              /*defer_init=*/true);
  EXPECT_FALSE(runner.is_init_scheduled());
  EXPECT_EQ(task_runner_->NumPendingTasks(), 0u);

  base::SequencedTaskRunner* runner_ptr = runner.GetTaskRunner();
  EXPECT_EQ(runner_ptr, task_runner_.get());
  EXPECT_TRUE(runner.is_init_scheduled());
  EXPECT_EQ(task_runner_->NumPendingTasks(), 1u);
}

TEST_F(HistoryBackendRunnerTest, DeferredSyncDelegateInitializesBackend) {
  HistoryDatabaseParams params(temp_dir_.GetPath(), 0, 0,
                               version_info::Channel::UNKNOWN);

  HistoryBackendRunner runner(task_runner_, backend_, /*no_db=*/false, params,
                              /*defer_init=*/true);
  EXPECT_FALSE(runner.is_init_scheduled());
  EXPECT_EQ(task_runner_->NumPendingTasks(), 0u);

  // Calling GetHistorySyncControllerDelegate on the backend sequence ensures
  // the backend is initialized.
  auto delegate = backend_->GetHistorySyncControllerDelegate();
  EXPECT_TRUE(delegate);
}

}  // namespace history
