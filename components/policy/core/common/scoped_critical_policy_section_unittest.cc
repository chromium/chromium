// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/scoped_critical_policy_section.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ScopedCriticalPolicySectionTest : public ::testing::Test {
 public:
  ScopedCriticalPolicySectionTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

  ~ScopedCriticalPolicySectionTest() override = default;
  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

TEST_F(ScopedCriticalPolicySectionTest, Normal) {
  ::testing::StrictMock<base::MockOnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(1);
  ScopedCriticalPolicySection::EnterWithEnterSectionCallback(
      mock_callback.Get(),
      base::BindOnce(
          [](ScopedCriticalPolicySection::OnSectionEnteredCallback callback) {
            std::move(callback).Run(ScopedCriticalPolicySection::Handles());
          }),
      task_runner());
  task_environment()->RunUntilIdle();
}

TEST_F(ScopedCriticalPolicySectionTest, Timeout) {
  auto fake_enter_section_function = base::BindOnce(
      [](ScopedCriticalPolicySection::OnSectionEnteredCallback callback) {
        base::ThreadPool::PostDelayedTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           ScopedCriticalPolicySection::Handles()),
            base::Seconds(30));
      });

  ::testing::StrictMock<base::MockOnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(0);
  ScopedCriticalPolicySection::EnterWithEnterSectionCallback(
      mock_callback.Get(), std::move(fake_enter_section_function),
      task_runner());
  task_environment()->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback, Run()).Times(1);
  task_environment()->FastForwardBy(base::Seconds(15));
}

}  // namespace policy
