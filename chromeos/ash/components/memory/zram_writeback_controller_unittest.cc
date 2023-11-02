// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/zram_writeback_controller.h"

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/memory/zram_writeback_backend.h"
#include "chromeos/ash/components/memory/zram_writeback_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::memory {

namespace {
using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::Exactly;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
constexpr int kMbShift = 20;
}  // namespace

class MockZramWritebackBackend : public ZramWritebackBackend {
 public:
  MockZramWritebackBackend() = default;
  MockZramWritebackBackend(const MockZramWritebackBackend&) = delete;
  MockZramWritebackBackend& operator=(const MockZramWritebackBackend&) = delete;

  ~MockZramWritebackBackend() override = default;

  MOCK_METHOD2(EnableWriteback, void(uint64_t size_mb, IntCallback cb));
  MOCK_METHOD2(SetWritebackLimit, void(uint64_t size_pages, IntCallback cb));
  MOCK_METHOD2(InitiateWriteback, void(ZramWritebackMode mode, Callback cb));
  MOCK_METHOD2(MarkIdle, void(base::TimeDelta age, Callback cb));
  MOCK_METHOD0(WritebackAlreadyEnabled, bool());

  MOCK_METHOD1(GetCurrentBackingDevSize, void(IntCallback cb));
  MOCK_METHOD0(GetZramDiskSizeBytes, int64_t());
  MOCK_METHOD0(GetCurrentWritebackLimitPages, int64_t());
  MOCK_METHOD0(GetCurrentBackingDevSize, int64_t());
};

class MockZramWritebackPolicy : public ZramWritebackPolicy {
 public:
  MockZramWritebackPolicy() = default;
  MockZramWritebackPolicy(const MockZramWritebackPolicy&) = delete;
  MockZramWritebackPolicy& operator=(const MockZramWritebackPolicy&) = delete;

  ~MockZramWritebackPolicy() override = default;

  MOCK_METHOD2(Initialize,
               void(uint64_t zram_disk_size_mb, uint64_t writeback_size_mb));

  MOCK_METHOD0(CanWritebackHugeIdle, bool());
  MOCK_METHOD0(CanWritebackHuge, bool());
  MOCK_METHOD0(CanWritebackIdle, bool());
  MOCK_METHOD0(GetCurrentWritebackIdleTime, base::TimeDelta());
  MOCK_METHOD0(GetAllowedWritebackLimit, uint64_t());
  MOCK_METHOD0(GetWritebackTimerInterval, base::TimeDelta());
};

class ZramWritebackControllerTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<MockZramWritebackPolicy> policy;
    std::unique_ptr<MockZramWritebackBackend> backend;
    CreatePolicyAndBackend(&policy, &backend);

    // Save for mocks.
    policy_ = policy.get();
    backend_ = backend.get();
    controller_ = base::WrapUnique(
        new ZramWritebackController(std::move(policy), std::move(backend)));
  }

  void TearDown() override {
    if (controller_)
      controller_->Stop();
  }

  // By default we create a nice mock, there is a similar StrictMock below.
  virtual void CreatePolicyAndBackend(
      std::unique_ptr<MockZramWritebackPolicy>* policy,
      std::unique_ptr<MockZramWritebackBackend>* backend) {
    *policy = std::make_unique<NiceMock<MockZramWritebackPolicy>>();
    *backend = std::make_unique<NiceMock<MockZramWritebackBackend>>();
  }

 protected:
  using Callback = MockZramWritebackBackend::Callback;
  using IntCallback = MockZramWritebackBackend::IntCallback;

  MockZramWritebackPolicy* policy() { return policy_; }
  MockZramWritebackBackend* backend() { return backend_; }
  ZramWritebackController* controller() { return controller_.get(); }
  base::test::TaskEnvironment* task_env() { return &task_environment_; }

 private:
  // Capture only for the mock.
  MockZramWritebackPolicy* policy_;
  MockZramWritebackBackend* backend_;

  std::unique_ptr<ZramWritebackController> controller_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ZramWritebackControllerTest, TestInitializationSequence) {
  constexpr uint64_t kWbSize = 555;
  constexpr uint64_t kZramSizeMb = 1000;
  EXPECT_CALL(*backend(), WritebackAlreadyEnabled()).WillOnce(Return(false));
  EXPECT_CALL(*backend(), EnableWriteback(_, _))
      .WillOnce(Invoke([](uint64_t size, IntCallback cb) {
        std::move(cb).Run(true, kWbSize);
      }));
  EXPECT_CALL(*backend(), GetZramDiskSizeBytes())
      .WillRepeatedly(Return(kZramSizeMb << kMbShift));
  EXPECT_CALL(*policy(), Initialize(kZramSizeMb, kWbSize)).Times(1);

  controller()->Start();
}

TEST_F(ZramWritebackControllerTest, TestInitializationSequenceAlreadyEnabled) {
  constexpr uint64_t kWbSize = 555;
  constexpr uint64_t kZramSizeMb = 1000;
  EXPECT_CALL(*backend(), WritebackAlreadyEnabled()).WillOnce(Return(true));
  EXPECT_CALL(*backend(), EnableWriteback(_, _)).Times(0);
  EXPECT_CALL(*backend(), GetCurrentBackingDevSize(_))
      .WillOnce(
          Invoke([](IntCallback cb) { std::move(cb).Run(true, kWbSize); }));

  EXPECT_CALL(*backend(), GetZramDiskSizeBytes())
      .WillRepeatedly(Return(kZramSizeMb << kMbShift));
  EXPECT_CALL(*policy(), Initialize(kZramSizeMb, kWbSize)).Times(1);

  controller()->Start();
}

// SimpleStrictZramWritebackController mocks out methods to bring up a simple
// controller removing the boiler plate and allowing tests to just focus on one
// piece.
class SimpleStrictZramWritebackControllerTest
    : public ZramWritebackControllerTest {
 public:
  ~SimpleStrictZramWritebackControllerTest() override = default;

  void SetUp() override {
    ZramWritebackControllerTest::SetUp();

    // Do a basic setup with standard values.
    EXPECT_CALL(*backend(), WritebackAlreadyEnabled()).WillOnce(Return(false));
    EXPECT_CALL(*backend(), EnableWriteback(_, _))
        .WillOnce(Invoke([](uint64_t size, IntCallback cb) {
          std::move(cb).Run(true, kWbSize);
        }));
    EXPECT_CALL(*backend(), GetZramDiskSizeBytes())
        .WillRepeatedly(Return(kZramSizeMb << kMbShift));
    EXPECT_CALL(*policy(), Initialize(kZramSizeMb, kWbSize)).Times(1);
  }

  void CreatePolicyAndBackend(
      std::unique_ptr<MockZramWritebackPolicy>* policy,
      std::unique_ptr<MockZramWritebackBackend>* backend) override {
    *policy = std::make_unique<StrictMock<MockZramWritebackPolicy>>();
    *backend = std::make_unique<StrictMock<MockZramWritebackBackend>>();
  }

  void TearDown() override { ZramWritebackControllerTest::TearDown(); }

 protected:
  static constexpr uint64_t kWbSize = 555;
  static constexpr uint64_t kZramSizeMb = 1000;
};

TEST_F(SimpleStrictZramWritebackControllerTest, TestPeriodicWritebackTimer) {
  EXPECT_CALL(*policy(), GetWritebackTimerInterval())
      .WillOnce(Return(base::Seconds(1)));

  // Validate that our timer is fired at the appropriate interval.
  EXPECT_CALL(*policy(), GetAllowedWritebackLimit())
      .Times(10)
      .WillRepeatedly(Return(0));

  controller()->Start();

  base::RunLoop run_loop;

  // If we run for 10 seconds we should check our writeback limit 10 times.
  task_env()->FastForwardBy(base::Seconds(10));
  run_loop.RunUntilIdle();
}

TEST_F(SimpleStrictZramWritebackControllerTest, TestFailSetWritebackLimit) {
  EXPECT_CALL(*policy(), GetWritebackTimerInterval())
      .WillOnce(Return(base::Seconds(1)));

  constexpr uint64_t kPageLimit = 15;
  EXPECT_CALL(*policy(), GetAllowedWritebackLimit())
      .Times(1)
      .WillRepeatedly(Return(kPageLimit));

  // Ultimately the controller needs to set the limit on the system via the
  // backend.
  EXPECT_CALL(*backend(), SetWritebackLimit(_, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          [](uint64_t limit, IntCallback cb) { std::move(cb).Run(false, 1); }));
  // We invoked the SetWritebackLimit callback with false (failed), so no
  // further methods should be called, we're a strict mock so that's how we
  // confirm this.

  controller()->Start();
  base::RunLoop run_loop;
  task_env()->FastForwardBy(base::Seconds(1));
  run_loop.RunUntilIdle();
}

// AdvancedStrictZramWritebackController test builds on the simple variant,
// allowing us to test more intriciate scenarios with less boiler plate code.
class AdvancedStrictZramWritebackControllerTest
    : public SimpleStrictZramWritebackControllerTest {
 public:
  ~AdvancedStrictZramWritebackControllerTest() override = default;

  void SetUp() override { SimpleStrictZramWritebackControllerTest::SetUp(); }
  void TearDown() override {
    SimpleStrictZramWritebackControllerTest::TearDown();
  }

  void RunTest() {
    task_env()->FastForwardBy(times_ * period_);
    run_loop_.RunUntilIdle();
  }

  void SetTestConfig(base::TimeDelta period, int times) {
    period_ = period;
    times_ = times;
  }

 protected:
  static constexpr uint64_t kPageLimit = 15;
  base::RunLoop run_loop_;
  int times_;
  base::TimeDelta period_;
};

TEST_F(AdvancedStrictZramWritebackControllerTest,
       IdleWritebackDoesIdleMarkAndWb) {
  // This test is validating that when we've enabled idle writeback we're doing
  // an idle sweep with the calculated age.
  SetTestConfig(/*period=*/base::Seconds(1), /*times=*/1);
  EXPECT_CALL(*policy(), GetWritebackTimerInterval()).WillOnce(Return(period_));

  EXPECT_CALL(*policy(), GetAllowedWritebackLimit())
      .Times(times_)
      .WillRepeatedly(Return(kPageLimit));

  EXPECT_CALL(*backend(), SetWritebackLimit(_, _))
      .Times(times_)
      .WillRepeatedly(Invoke([](uint64_t limit, IntCallback cb) {
        std::move(cb).Run(true, kPageLimit);
      }));
  EXPECT_CALL(*policy(), CanWritebackIdle()).WillOnce(Return(true));

  // The callback will check our current state, so it checks huge idle one more
  // time.
  EXPECT_CALL(*policy(), CanWritebackHugeIdle())
      .Times(2)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*policy(), GetCurrentWritebackIdleTime())
      .WillOnce(Return(base::Seconds(60)));

  // We expect to do an idle sweep of 60s.
  EXPECT_CALL(*backend(), MarkIdle(base::Seconds(60), _))
      .WillOnce(Invoke(
          [](base::TimeDelta age, Callback cb) { std::move(cb).Run(true); }));

  // We expect to now initiate a writeback for idle.
  EXPECT_CALL(*backend(), InitiateWriteback(ZramWritebackMode::kModeIdle, _))
      .WillOnce(Invoke([](ZramWritebackMode mode, Callback cb) {
        std::move(cb).Run(true);
      }));

  // Finally we will report that our writeback limit is 0 (all pages were
  // written back).
  EXPECT_CALL(*backend(), GetCurrentWritebackLimitPages()).WillOnce(Return(0));

  controller()->Start();
  RunTest();
}

}  // namespace ash::memory
