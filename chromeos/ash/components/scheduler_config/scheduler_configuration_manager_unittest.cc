// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scheduler_config/scheduler_configuration_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;

}  // namespace

class SchedulerConfigurationManagerTest
    : public testing::Test,
      public SchedulerConfigurationManagerBase::Observer {
 public:
  SchedulerConfigurationManagerTest() {
    SchedulerConfigurationManagerBase::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  // SchedulerConfigurationManagerBase::Observer:
  void OnConfigurationSet(bool success, size_t num_cores_disabled) override {
    ++configuration_set_count_;
    if (configuration_set_quit_closure_) {
      std::move(configuration_set_quit_closure_).Run();
    }
  }

  void WaitForConfigurationSet() {
    base::RunLoop run_loop;
    configuration_set_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FakeDebugDaemonClient debug_daemon_client_;
  TestingPrefServiceSimple local_state_;

  size_t configuration_set_count_ = 0;
  base::OnceClosure configuration_set_quit_closure_;
};

TEST_F(SchedulerConfigurationManagerTest, Startup) {
  local_state_.SetString(prefs::kSchedulerConfiguration, "initial");
  debug_daemon_client_.SetServiceIsAvailable(false);

  // Manager waits on initialization for service to be available.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);

  task_environment_.RunUntilIdle();
  EXPECT_EQ("", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(0u, configuration_set_count_);

  // Config changes don't lead to updates while debugd isn't ready.
  local_state_.SetString(prefs::kSchedulerConfiguration, "config");
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(0u, configuration_set_count_);

  // Once the debugd service becomes available, the config gets set.
  debug_daemon_client_.SetServiceIsAvailable(true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("config", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(1u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, CommandLineDefault) {
  // In real usage the command line is set before Chrome launches, so set it as
  // such here.
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* cmd_line = scoped_command_line.GetProcessCommandLine();
  cmd_line->AppendSwitchASCII("scheduler-configuration-default",
                              "cmd-line-default");

  // Correct default is used when the command line is setup.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);
  task_environment_.RunUntilIdle();

  EXPECT_EQ("cmd-line-default",
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(1u, configuration_set_count_);

  // Change user pref, which should trigger a config change.
  local_state_.SetUserPref(prefs::kSchedulerConfiguration,
                           std::make_unique<base::Value>("user"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("user", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(2u, configuration_set_count_);

  // Set a policy, which should override the user setting
  local_state_.SetManagedPref(prefs::kSchedulerConfiguration,
                              std::make_unique<base::Value>("policy"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(3u, configuration_set_count_);

  // Dropping the user pref doesn't change anything.
  local_state_.RemoveUserPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(4u, configuration_set_count_);

  // Dropping the policy as well reverts to the cmdline default.
  local_state_.RemoveManagedPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("cmd-line-default",
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(5u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, ConfigChange) {
  // Correct default is used when there is no configured value.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(debugd::scheduler_configuration::kCoreIsolationScheduler,
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(1u, configuration_set_count_);

  // Change user pref, which should trigger a config change.
  local_state_.SetUserPref(prefs::kSchedulerConfiguration,
                           std::make_unique<base::Value>("user"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("user", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(2u, configuration_set_count_);

  // Set a policy, which should override the user setting
  local_state_.SetManagedPref(prefs::kSchedulerConfiguration,
                              std::make_unique<base::Value>("policy"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(3u, configuration_set_count_);

  // Dropping the user pref doesn't change anything.
  local_state_.RemoveUserPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(4u, configuration_set_count_);

  // Dropping the policy as well reverts to the default configuration.
  local_state_.RemoveManagedPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(debugd::scheduler_configuration::kCoreIsolationScheduler,
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(5u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, FinchDefault) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      features::kSchedulerConfiguration, {{"config", "finch"}});

  // Finch parameter selects the default.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("finch", debug_daemon_client_.scheduler_configuration_name());

  // Config values override finch default.
  local_state_.SetString(prefs::kSchedulerConfiguration, "config");
  task_environment_.RunUntilIdle();
  EXPECT_EQ("config", debug_daemon_client_.scheduler_configuration_name());
}

TEST_F(SchedulerConfigurationManagerTest, LogsWarningWhenDebugdUnavailable) {
  debug_daemon_client_.SetServiceIsAvailable(false);

  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);

  base::test::MockLog log;
  EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOGGING_WARNING, _, _, _,
                       HasSubstr("Still waiting for debugd D-Bus service")))
      .Times(1);
  log.StartCapturingLogs();

  task_environment_.FastForwardBy(
      SchedulerConfigurationManager::kDebugdWaitWarningTimeout);
  log.StopCapturingLogs();

  EXPECT_FALSE(manager.GetLastReply().has_value());
  EXPECT_EQ(0u, configuration_set_count_);

  // Normal configuration still completes if debugd becomes available later.
  debug_daemon_client_.SetServiceIsAvailable(true);
  WaitForConfigurationSet();
  EXPECT_TRUE(manager.GetLastReply().has_value());
  EXPECT_EQ(1u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, LogsWarningWhenReplySlow) {
  debug_daemon_client_.SetServiceIsAvailable(false);
  debug_daemon_client_.SetDeferSchedulerConfigurationReply(true);

  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);

  const base::TimeDelta half_timeout =
      SchedulerConfigurationManager::kDebugdWaitWarningTimeout / 2;

  {
    base::test::MockLog log;
    EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(log, Log(logging::LOGGING_WARNING, _, _, _, _)).Times(0);
    log.StartCapturingLogs();

    // Becoming available after half the timeout starts a fresh reply timeout
    // measured from SetSchedulerConfigurationV2(), not from construction.
    task_environment_.FastForwardBy(half_timeout);
    debug_daemon_client_.SetServiceIsAvailable(true);
    ASSERT_TRUE(
        debug_daemon_client_.has_pending_scheduler_configuration_callback());
    task_environment_.FastForwardBy(half_timeout);
    log.StopCapturingLogs();
  }

  {
    base::test::MockLog log;
    EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(
        log,
        Log(logging::LOGGING_WARNING, _, _, _,
            HasSubstr("Still waiting for scheduler configuration reply from "
                      "debugd")))
        .Times(1);
    log.StartCapturingLogs();
    task_environment_.FastForwardBy(
        SchedulerConfigurationManager::kDebugdWaitWarningTimeout -
        half_timeout);
    log.StopCapturingLogs();
  }

  EXPECT_FALSE(manager.GetLastReply().has_value());
  EXPECT_EQ(0u, configuration_set_count_);

  debug_daemon_client_.RunNextPendingSetSchedulerConfigurationCallback(
      /*result=*/true, /*num_cores_disabled=*/2);
  EXPECT_EQ(std::make_pair(true, size_t{2}), manager.GetLastReply());
  EXPECT_EQ(1u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, LogsWarningWhenSubsequentReplySlow) {
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);
  WaitForConfigurationSet();
  ASSERT_EQ(1u, configuration_set_count_);

  debug_daemon_client_.SetDeferSchedulerConfigurationReply(true);
  local_state_.SetString(prefs::kSchedulerConfiguration, "performance");
  ASSERT_TRUE(
      debug_daemon_client_.has_pending_scheduler_configuration_callback());

  base::test::MockLog log;
  EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(
      log, Log(logging::LOGGING_WARNING, _, _, _,
               HasSubstr("Still waiting for scheduler configuration reply from "
                         "debugd")))
      .Times(1);
  log.StartCapturingLogs();

  task_environment_.FastForwardBy(
      SchedulerConfigurationManager::kDebugdWaitWarningTimeout);
  log.StopCapturingLogs();

  debug_daemon_client_.RunNextPendingSetSchedulerConfigurationCallback(
      /*result=*/true, /*num_cores_disabled=*/0);
  EXPECT_EQ(2u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest,
       OverlappingRequestsKeepTimerUntilAllRepliesArrive) {
  debug_daemon_client_.SetServiceIsAvailable(false);
  debug_daemon_client_.SetDeferSchedulerConfigurationReply(true);

  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);
  debug_daemon_client_.SetServiceIsAvailable(true);
  ASSERT_TRUE(
      debug_daemon_client_.has_pending_scheduler_configuration_callback());

  const base::TimeDelta half_timeout =
      SchedulerConfigurationManager::kDebugdWaitWarningTimeout / 2;

  {
    base::test::MockLog log;
    EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(log, Log(logging::LOGGING_WARNING, _, _, _, _)).Times(0);
    log.StartCapturingLogs();

    // Issue a second request before the first finishes, then complete the
    // first.
    task_environment_.FastForwardBy(half_timeout);
    local_state_.SetString(prefs::kSchedulerConfiguration, "performance");
    debug_daemon_client_.RunNextPendingSetSchedulerConfigurationCallback(
        /*result=*/true, /*num_cores_disabled=*/0);
    EXPECT_EQ(1u, configuration_set_count_);
    ASSERT_TRUE(
        debug_daemon_client_.has_pending_scheduler_configuration_callback());

    // Half the timeout after the second request should not warn yet.
    task_environment_.FastForwardBy(half_timeout);
    log.StopCapturingLogs();
  }

  {
    // Full timeout after the second request should warn since it is still
    // pending.
    base::test::MockLog log;
    EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(
        log,
        Log(logging::LOGGING_WARNING, _, _, _,
            HasSubstr("Still waiting for scheduler configuration reply from "
                      "debugd")))
        .Times(1);
    log.StartCapturingLogs();
    task_environment_.FastForwardBy(
        SchedulerConfigurationManager::kDebugdWaitWarningTimeout -
        half_timeout);
    log.StopCapturingLogs();
  }

  debug_daemon_client_.RunNextPendingSetSchedulerConfigurationCallback(
      /*result=*/true, /*num_cores_disabled=*/4);
  EXPECT_EQ(std::make_pair(true, size_t{4}), manager.GetLastReply());
  EXPECT_EQ(2u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest,
       NoWarningWhenInitialReplyArrivesBeforeTimeout) {
  base::test::MockLog log;
  EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOGGING_WARNING, _, _, _, _)).Times(0);
  log.StartCapturingLogs();

  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);
  WaitForConfigurationSet();
  task_environment_.FastForwardBy(
      SchedulerConfigurationManager::kDebugdWaitWarningTimeout);
  log.StopCapturingLogs();

  EXPECT_TRUE(manager.GetLastReply().has_value());
}

TEST_F(SchedulerConfigurationManagerTest,
       NoWarningWhenDebugdReportsUnavailable) {
  debug_daemon_client_.SetServiceIsAvailable(false);

  base::test::MockLog log;
  EXPECT_CALL(log, Log(_, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOGGING_ERROR, _, _, _,
                       HasSubstr("Debug daemon unavailable")))
      .Times(1);
  EXPECT_CALL(log, Log(logging::LOGGING_WARNING, _, _, _, _)).Times(0);
  log.StartCapturingLogs();

  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  debug_daemon_client_.ReportServiceIsNotAvailable();
  task_environment_.FastForwardBy(
      SchedulerConfigurationManager::kDebugdWaitWarningTimeout);
  log.StopCapturingLogs();

  EXPECT_FALSE(manager.GetLastReply().has_value());
}

}  // namespace ash
