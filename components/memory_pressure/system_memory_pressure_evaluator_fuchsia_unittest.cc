// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_fuchsia.h"

#include <fuchsia/memorypressure/cpp/fidl.h>
#include <fuchsia/memorypressure/cpp/fidl_test_base.h>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

class MockMemoryPressureVoter : public MemoryPressureVoter {
 public:
  MOCK_METHOD2(SetVote, void(base::MemoryPressureLevel, bool));
};

class TestSystemMemoryPressureEvaluator
    : public SystemMemoryPressureEvaluatorFuchsia {
 public:
  TestSystemMemoryPressureEvaluator(std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluatorFuchsia(std::move(voter)) {}

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;
};

}  // namespace

class SystemMemoryPressureEvaluatorFuchsiaTest
    : public testing::Test,
      public fuchsia::memorypressure::testing::Provider_TestBase {
 public:
  SystemMemoryPressureEvaluatorFuchsiaTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SendPressureLevel(fuchsia::memorypressure::Level level) {
    base::RunLoop wait_loop;
    watcher_->OnLevelChanged(
        level, [quit_loop = wait_loop.QuitClosure()]() { quit_loop.Run(); });
    wait_loop.Run();
  }

  bool have_watcher() const { return watcher_.is_bound(); }

  // fuchsia::memorypressure::Provider implementation.
  void RegisterWatcher(fidl::InterfaceHandle<fuchsia::memorypressure::Watcher>
                           watcher) override {
    watcher_.Bind(std::move(watcher));
    SendPressureLevel(fuchsia::memorypressure::Level::NORMAL);
  }

 protected:
  // fuchsia::memorypressure::testing::Provider_TestBase implementation.
  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unexpected call to method: " << name;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::TestComponentContextForProcess test_context_;

  fuchsia::memorypressure::WatcherPtr watcher_;
};

using SystemMemoryPressureEvaluatorFuchsiaDeathTest =
    SystemMemoryPressureEvaluatorFuchsiaTest;

TEST_F(SystemMemoryPressureEvaluatorFuchsiaDeathTest, ProviderUnavailable) {
  auto voter = std::make_unique<MockMemoryPressureVoter>();
  TestSystemMemoryPressureEvaluator evaluator(std::move(voter));

  // Spin the loop to allow the evaluator to notice that the Provider is not
  // available and verify that this causes a fatal failure.
  ASSERT_DEATH(base::RunLoop().RunUntilIdle(),
               "fuchsia\\.memorypressure\\.Provider disconnected unexpectedly, "
               "exiting: ZX_ERR_PEER_CLOSED \\(-24\\)");
}

TEST_F(SystemMemoryPressureEvaluatorFuchsiaTest, Basic) {
  base::ScopedServiceBinding<::fuchsia::memorypressure::Provider>
      publish_provider(test_context_.additional_services(), this);

  auto voter = std::make_unique<MockMemoryPressureVoter>();
  // NONE pressure will be reported once the first Watch() call returns, which
  // should not notify listeners.
  testing::InSequence s;
  EXPECT_CALL(*voter, SetVote(base::MEMORY_PRESSURE_LEVEL_NONE, false));
  EXPECT_CALL(*voter, SetVote(base::MEMORY_PRESSURE_LEVEL_CRITICAL, true));
  // and then again when the fakes system level transitions from
  // CRITICAL->NORMAL.
  EXPECT_CALL(*voter, SetVote(base::MEMORY_PRESSURE_LEVEL_NONE, true));
  EXPECT_CALL(*voter, SetVote(base::MEMORY_PRESSURE_LEVEL_MODERATE, true));

  TestSystemMemoryPressureEvaluator evaluator(std::move(voter));

  // Spin the loop to ensure that RegisterWatcher() is processed.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(have_watcher());

  SendPressureLevel(fuchsia::memorypressure::Level::CRITICAL);
  EXPECT_EQ(evaluator.current_vote(), base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  SendPressureLevel(fuchsia::memorypressure::Level::NORMAL);
  EXPECT_EQ(evaluator.current_vote(), base::MEMORY_PRESSURE_LEVEL_NONE);

  SendPressureLevel(fuchsia::memorypressure::Level::WARNING);
  EXPECT_EQ(evaluator.current_vote(), base::MEMORY_PRESSURE_LEVEL_MODERATE);
}

TEST_F(SystemMemoryPressureEvaluatorFuchsiaTest, Periodic) {
  base::ScopedServiceBinding<fuchsia::memorypressure::Provider>
      publish_provider(test_context_.additional_services(), this);

  MultiSourceMemoryPressureMonitor monitor;

  TestSystemMemoryPressureEvaluator evaluator(monitor.CreateVoter());

  // Spin the loop to ensure that RegisterWatcher() is processed.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(have_watcher());

  testing::StrictMock<base::RegisteredMockMemoryPressureListener> listener;

  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  SendPressureLevel(fuchsia::memorypressure::Level::WARNING);
  EXPECT_EQ(evaluator.current_vote(), base::MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Verify that MODERATE pressure level is reported periodically.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  task_environment_.FastForwardBy(evaluator.kRenotifyVotePeriod);
  testing::Mock::VerifyAndClearExpectations(&listener);

  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  SendPressureLevel(fuchsia::memorypressure::Level::CRITICAL);
  EXPECT_EQ(evaluator.current_vote(), base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Verify that CRITICAL pressure level is reported periodically.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  task_environment_.FastForwardBy(evaluator.kRenotifyVotePeriod);
  testing::Mock::VerifyAndClearExpectations(&listener);

  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE));
  SendPressureLevel(fuchsia::memorypressure::Level::NORMAL);
  EXPECT_EQ(evaluator.current_vote(), base::MEMORY_PRESSURE_LEVEL_NONE);

  // Verify that NONE pressure level is not reported periodically.
  task_environment_.FastForwardBy(evaluator.kRenotifyVotePeriod);
  testing::Mock::VerifyAndClearExpectations(&listener);
}

}  // namespace memory_pressure
