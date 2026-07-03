// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_backend.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace critical_actions {

class CriticalActionBackendTest : public testing::Test {
 public:
  CriticalActionBackendTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
    backend_ = std::make_unique<CriticalActionBackend>(db_path_);
  }

  void TearDown() override { backend_.reset(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<CriticalActionBackend> backend_;
};

TEST_F(CriticalActionBackendTest, InitBackend) {
  backend_->Init();
  EXPECT_TRUE(base::PathExists(db_path_));
}

TEST_F(CriticalActionBackendTest, CallBeforeInitReturnsGracefully) {
  const std::string action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.timestamp = base::Time::Now();
  entry.action_type = ActionType::kFormFill;

  // Database is not initialized. All the operations should return gracefully
  // without crashing.
  backend_->AddCriticalAction(entry);
  EXPECT_FALSE(backend_->GetCriticalAction(action_id).has_value());
  backend_->DeleteCriticalAction(action_id);
  backend_->DeleteCriticalActionsInTimeRange(base::Time::Now(),
                                             base::Time::Now());
  backend_->DeleteCriticalActionsByVisitIds({123, 456});
}

TEST_F(CriticalActionBackendTest, ForwardCallsToDatabase) {
  backend_->Init();

  const std::string action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.timestamp = base::Time::Now();
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.actor_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.action_type = ActionType::kFormFill;
  entry.url = GURL("https://example.com");
  entry.metadata = "{}";

  // Verify basic crud operations are successfully forwarded.
  backend_->AddCriticalAction(entry);

  auto retrieved = backend_->GetCriticalAction(action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);

  backend_->DeleteCriticalAction(action_id);
  EXPECT_FALSE(backend_->GetCriticalAction(action_id).has_value());
}

}  // namespace critical_actions
