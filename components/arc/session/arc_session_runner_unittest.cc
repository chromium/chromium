// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr int kContainerStarting =
    static_cast<int>(ArcContainerLifetimeEvent::CONTAINER_STARTING);
constexpr int kContainerFailedToStart =
    static_cast<int>(ArcContainerLifetimeEvent::CONTAINER_FAILED_TO_START);
constexpr int kContainerCrashedEarly =
    static_cast<int>(ArcContainerLifetimeEvent::CONTAINER_CRASHED_EARLY);
constexpr int kContainerCrashed =
    static_cast<int>(ArcContainerLifetimeEvent::CONTAINER_CRASHED);
constexpr char kDefaultLocale[] = "en-US";

UpgradeParams DefaultUpgradeParams() {
  UpgradeParams params;
  params.locale = kDefaultLocale;
  return params;
}

class DoNothingObserver : public ArcSessionRunner::Observer {
 public:
  void OnSessionStopped(ArcStopReason reason, bool restarting) override {
    // Do nothing.
  }
  void OnSessionRestarting() override {
    // Do nothing.
  }
};

}  // namespace

class ArcSessionRunnerTest : public testing::Test,
                             public ArcSessionRunner::Observer {
 public:
  ArcSessionRunnerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    chromeos::SessionManagerClient::InitializeFakeInMemory();

    stop_reason_ = ArcStopReason::SHUTDOWN;
    restarting_ = false;
    stopped_called_ = false;
    restarting_called_ = false;

    // We inject FakeArcSession here so we do not need task_runner.
    arc_session_runner_ = std::make_unique<ArcSessionRunner>(
        base::BindRepeating(FakeArcSession::Create));
    arc_session_runner_->AddObserver(this);
  }

  void TearDown() override {
    arc_session_runner_->RemoveObserver(this);
    arc_session_runner_.reset();

    chromeos::SessionManagerClient::Shutdown();
  }

  ArcSessionRunner* arc_session_runner() { return arc_session_runner_.get(); }

  FakeArcSession* arc_session() {
    return static_cast<FakeArcSession*>(
        arc_session_runner_->GetArcSessionForTesting());
  }

  ArcStopReason stop_reason() {
    EXPECT_TRUE(stopped_called());
    return stop_reason_;
  }

  bool restarting() {
    EXPECT_TRUE(stopped_called());
    return restarting_;
  }

  bool stopped_called() { return stopped_called_; }
  bool restarting_called() { return restarting_called_; }

  void ResetArcSessionFactory(
      const ArcSessionRunner::ArcSessionFactory& factory) {
    arc_session_runner_->RemoveObserver(this);
    arc_session_runner_ = std::make_unique<ArcSessionRunner>(factory);
    arc_session_runner_->AddObserver(this);
  }

  static std::unique_ptr<ArcSession> CreateSuspendedArcSession() {
    auto arc_session = std::make_unique<FakeArcSession>();
    arc_session->SuspendBoot();
    return std::move(arc_session);
  }

  static std::unique_ptr<ArcSession> CreateBootFailureArcSession(
      ArcStopReason reason) {
    auto arc_session = std::make_unique<FakeArcSession>();
    arc_session->EnableBootFailureEmulation(reason);
    return std::move(arc_session);
  }

 private:
  // ArcSessionRunner::Observer:
  void OnSessionStopped(ArcStopReason stop_reason, bool restarting) override {
    // The instance is already destructed in
    // ArcSessionRunner::OnSessionStopped().
    stop_reason_ = stop_reason;
    restarting_ = restarting;
    stopped_called_ = true;
    restarting_called_ = false;
  }
  void OnSessionRestarting() override { restarting_called_ = true; }

  ArcStopReason stop_reason_;
  bool restarting_;
  bool stopped_called_;
  bool restarting_called_;
  std::unique_ptr<ArcSessionRunner> arc_session_runner_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionRunnerTest);
};

// Exercises the basic functionality of the ArcSessionRunner. Observer should
// be notified.
TEST_F(ArcSessionRunnerTest, Basic) {
  class Observer : public ArcSessionRunner::Observer {
   public:
    Observer() = default;

    bool stopped_called() const { return stopped_called_; }

    // ArcSessionRunner::Observer:
    void OnSessionStopped(ArcStopReason reason, bool restarting) override {
      stopped_called_ = true;
    }
    void OnSessionRestarting() override {}

   private:
    bool stopped_called_ = false;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  Observer observer;
  arc_session_runner()->AddObserver(&observer);
  base::ScopedClosureRunner teardown(base::BindOnce(
      [](ArcSessionRunner* arc_session_runner, Observer* observer) {
        arc_session_runner->RemoveObserver(observer);
      },
      arc_session_runner(), &observer));

  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->is_running());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
  EXPECT_TRUE(observer.stopped_called());
}

// If the ArcSessionRunner accepts a request to stop ARC instance, it should
// stop it, even mid-startup.
TEST_F(ArcSessionRunnerTest, StopMidStartup) {
  ResetArcSessionFactory(
      base::BindRepeating(&ArcSessionRunnerTest::CreateSuspendedArcSession));
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->is_running());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(restarting());
}

// Does the same for mini instance.
TEST_F(ArcSessionRunnerTest, StopMidStartup_MiniInstance) {
  ResetArcSessionFactory(
      base::BindRepeating(&ArcSessionRunnerTest::CreateSuspendedArcSession));
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStartMiniInstance();
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->is_running());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
}

// If the boot procedure is failed, then restarting mechanism should not
// triggered.
TEST_F(ArcSessionRunnerTest, BootFailure) {
  ResetArcSessionFactory(
      base::BindRepeating(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                          ArcStopReason::GENERIC_BOOT_FAILURE));
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE, stop_reason());
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(restarting());
}

// Does the same with the mini instance.
TEST_F(ArcSessionRunnerTest, BootFailure_MiniInstance) {
  ResetArcSessionFactory(
      base::BindRepeating(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                          ArcStopReason::GENERIC_BOOT_FAILURE));
  EXPECT_FALSE(arc_session());

  // If starting the mini instance fails, arc_session_runner()'s state goes back
  // to STOPPED, but its observers won't be notified.
  arc_session_runner()->RequestStartMiniInstance();
  arc_session()->EmulateMiniContainerStart();
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(stopped_called());

  // Also make sure that RequestUpgrade() works just fine after the boot
  // failure.
  ResetArcSessionFactory(base::BindRepeating(FakeArcSession::Create));
  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->is_running());
}

// Similary, CRASH should do same for GENERIC_BOOT_FAILURE case, because
// in mini instance, Mojo connection should not be established.
TEST_F(ArcSessionRunnerTest, Crash_MiniInstance) {
  ResetArcSessionFactory(
      base::BindRepeating(&ArcSessionRunnerTest::CreateBootFailureArcSession,
                          ArcStopReason::CRASH));
  EXPECT_FALSE(arc_session());

  // If starting the mini instance fails, arc_session_runner()'s state goes back
  // to STOPPED, but its observers won't be notified.
  arc_session_runner()->RequestStartMiniInstance();
  arc_session()->EmulateMiniContainerStart();
  EXPECT_FALSE(arc_session());
  EXPECT_FALSE(stopped_called());
}

// Tests that RequestUpgrade works after calling RequestStart.
TEST_F(ArcSessionRunnerTest, Upgrade) {
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStartMiniInstance();
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->is_running());

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->is_running());
}

// If the instance is stopped, it should be re-started.
TEST_F(ArcSessionRunnerTest, Restart) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->is_running());

  // Simulate a connection loss.
  ASSERT_TRUE(arc_session());
  arc_session()->StopWithReason(ArcStopReason::CRASH);
  EXPECT_FALSE(arc_session());
  EXPECT_TRUE(restarting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(restarting_called());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->is_running());
  // Checks if the restart retains the original parameter.
  EXPECT_EQ(std::string(kDefaultLocale),
            arc_session()->upgrade_locale_param());

  arc_session_runner()->RequestStop();
  EXPECT_FALSE(arc_session());
}

TEST_F(ArcSessionRunnerTest, GracefulStop) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->is_running());

  // Graceful stop.
  arc_session_runner()->RequestStop();
  EXPECT_EQ(ArcStopReason::SHUTDOWN, stop_reason());
  EXPECT_FALSE(restarting());
  EXPECT_FALSE(restarting_called());
  EXPECT_FALSE(arc_session());
}

TEST_F(ArcSessionRunnerTest, Shutdown) {
  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_TRUE(arc_session());
  EXPECT_TRUE(arc_session()->is_running());

  // Simulate shutdown.
  arc_session_runner()->OnShutdown();
  EXPECT_EQ(ArcStopReason::SHUTDOWN, stop_reason());
  EXPECT_FALSE(arc_session());
}

// Removing the same observer more than once should be okay.
TEST_F(ArcSessionRunnerTest, RemoveObserverTwice) {
  EXPECT_FALSE(arc_session());

  DoNothingObserver do_nothing_observer;
  arc_session_runner()->AddObserver(&do_nothing_observer);
  // Call RemoveObserver() twice.
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
}

// Removing an unknown observer should be allowed.
TEST_F(ArcSessionRunnerTest, RemoveUnknownObserver) {
  EXPECT_FALSE(arc_session());

  DoNothingObserver do_nothing_observer;
  arc_session_runner()->RemoveObserver(&do_nothing_observer);
}

// Tests UMA recording on mini instance -> full instance -> shutdown case.
TEST_F(ArcSessionRunnerTest, UmaRecording_StartUpgradeShutdown) {
  base::HistogramTester tester;

  arc_session_runner()->RequestStartMiniInstance();
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1 /* count of the sample */);

  // Boot continue should not increase the count.
  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1);

  // "0" should be recorded as a restart count on shutdown.
  arc_session_runner()->OnShutdown();
  tester.ExpectUniqueSample("Arc.ContainerRestartAfterCrashCount",
                            0 /* sample */, 1 /* count of the sample */);
}

// Tests UMA recording on full instance -> shutdown case.
TEST_F(ArcSessionRunnerTest, UmaRecording_StartShutdown) {
  base::HistogramTester tester;

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1);
  // "0" should be recorded as a restart count on shutdown.
  arc_session_runner()->OnShutdown();
  tester.ExpectUniqueSample("Arc.ContainerRestartAfterCrashCount", 0, 1);
}

// Tests UMA recording on mini instance -> full instance -> crash -> shutdown
// case.
TEST_F(ArcSessionRunnerTest, UmaRecording_CrashTwice) {
  base::HistogramTester tester;

  arc_session_runner()->SetRestartDelayForTesting(base::TimeDelta());
  EXPECT_FALSE(arc_session());

  arc_session_runner()->RequestStartMiniInstance();
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1);
  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());

  // Stop the instance with CRASH.
  arc_session()->StopWithReason(ArcStopReason::CRASH);
  tester.ExpectBucketCount("Arc.ContainerLifetimeEvent", kContainerCrashed, 1);
  tester.ExpectTotalCount("Arc.ContainerLifetimeEvent", 2);

  // Restart the instance, then crash the instance again. The second CRASH
  // should not affect Arc.ContainerLifetimeEvent.
  base::RunLoop().RunUntilIdle();
  arc_session()->StopWithReason(ArcStopReason::CRASH);
  tester.ExpectBucketCount("Arc.ContainerLifetimeEvent", kContainerCrashed, 1);
  tester.ExpectTotalCount("Arc.ContainerLifetimeEvent", 2);

  // However, "2" should be recorded as a restart count on shutdown.
  base::RunLoop().RunUntilIdle();
  arc_session_runner()->OnShutdown();
  tester.ExpectUniqueSample("Arc.ContainerRestartAfterCrashCount", 2, 1);
}

// Tests UMA recording on mini instance -> crash -> shutdown case.
TEST_F(ArcSessionRunnerTest, UmaRecording_CrashMini) {
  base::HistogramTester tester;

  arc_session_runner()->RequestStartMiniInstance();
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1);

  // Stop the instance with CRASH.
  arc_session()->StopWithReason(ArcStopReason::CRASH);
  tester.ExpectBucketCount("Arc.ContainerLifetimeEvent", kContainerCrashedEarly,
                           1);
  tester.ExpectTotalCount("Arc.ContainerLifetimeEvent", 2);

  // In this case, no restart happened. "0" should be recorded.
  arc_session_runner()->OnShutdown();
  tester.ExpectUniqueSample("Arc.ContainerRestartAfterCrashCount", 0, 1);
}

// Tests UMA recording on mini instance -> boot fail -> shutdown case.
TEST_F(ArcSessionRunnerTest, UmaRecording_BootFail) {
  base::HistogramTester tester;

  arc_session_runner()->RequestStartMiniInstance();
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1);

  arc_session()->StopWithReason(ArcStopReason::GENERIC_BOOT_FAILURE);
  tester.ExpectBucketCount("Arc.ContainerLifetimeEvent",
                           kContainerFailedToStart, 1);
  tester.ExpectTotalCount("Arc.ContainerLifetimeEvent", 2);

  // No restart happened. "0" should be recorded.
  arc_session_runner()->OnShutdown();
  tester.ExpectUniqueSample("Arc.ContainerRestartAfterCrashCount", 0, 1);
}

// Tests UMA recording on full instance -> low disk -> shutdown case.
TEST_F(ArcSessionRunnerTest, UmaRecording_LowDisk) {
  base::HistogramTester tester;

  arc_session_runner()->RequestUpgrade(DefaultUpgradeParams());
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1);

  // We don't record UMA for LOW_DISK_SPACE.
  arc_session()->StopWithReason(ArcStopReason::LOW_DISK_SPACE);
  tester.ExpectUniqueSample("Arc.ContainerLifetimeEvent", kContainerStarting,
                            1);

  // No restart happened. "0" should be recorded.
  arc_session_runner()->OnShutdown();
  tester.ExpectUniqueSample("Arc.ContainerRestartAfterCrashCount", 0, 1);
}

}  // namespace arc
