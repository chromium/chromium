// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/task_manager.h"

#include <windows.h>

#include <atlcomcli.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/credential_provider/extension/extension_utils.h"
#include "chrome/credential_provider/extension/task.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class TaskManagerTest : public ::testing::Test {
 public:
  TaskManagerTest() {}

  void RunTasks() {
    fake_task_manager_.RunTasks(task_environment_.GetMainThreadTaskRunner());
  }

  void SetUp() override {
    ScopedLsaPolicy::SetCreatorForTesting(
        fake_scoped_lsa_factory_.GetCreatorCallback());
    registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  }

  FakeTaskManager* fake_task_manager() { return &fake_task_manager_; }
  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

  base::test::SingleThreadTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  FakeTaskManager fake_task_manager_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_factory_;
  registry_util::RegistryOverrideManager registry_override;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

class FakeTask : public extension::Task {
 public:
  explicit FakeTask(base::TimeDelta period);

  ~FakeTask() override;

  extension::Config GetConfig() override;

  HRESULT SetContext(
      const std::vector<extension::UserDeviceContext>& c) override;

  HRESULT Execute() override;

  static std::vector<extension::UserDeviceContext> user_device_context_;
  static int num_fails_;

 private:
  base::TimeDelta period_;
};

std::vector<extension::UserDeviceContext> FakeTask::user_device_context_;

int FakeTask::num_fails_ = 0;

FakeTask::FakeTask(base::TimeDelta period) : period_(period) {}

FakeTask::~FakeTask() {}

extension::Config FakeTask::GetConfig() {
  extension::Config config;
  config.execution_period = period_;
  return config;
}

HRESULT FakeTask::SetContext(
    const std::vector<extension::UserDeviceContext>& c) {
  user_device_context_ = c;
  return S_OK;
}

HRESULT FakeTask::Execute() {
  if (num_fails_ != 0) {
    num_fails_--;
    return E_FAIL;
  }
  return S_OK;
}

std::unique_ptr<extension::Task> AuxTaskCreator(base::TimeDelta period) {
  auto task = std::make_unique<FakeTask>(period);
  return std::move(task);
}

extension::TaskCreator GenerateTaskCreator(base::TimeDelta period) {
  return base::BindRepeating(&AuxTaskCreator, period);
}

TEST_F(TaskManagerTest, PeriodicDelay) {
  std::string fake_task_name = "fake_task";

  // Registers a task which has a config to run every 3 hours.
  fake_task_manager()->RegisterTask(fake_task_name,
                                    GenerateTaskCreator(base::Hours(3)));

  // Starts running registered tasks for all associated GCPW users.
  RunTasks();

  task_environment()->FastForwardBy(base::Hours(5));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 2);

  std::wstring fake_task_reg_name =
      extension::GetLastSyncRegNameForTask(base::UTF8ToWide(fake_task_name));
  ASSERT_NE(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  task_environment()->FastForwardBy(base::Hours(2));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 3);
}

TEST_F(TaskManagerTest, PreviouslyExecuted) {
  std::string fake_task_name = "fake_task";

  std::wstring fake_task_reg_name =
      extension::GetLastSyncRegNameForTask(base::UTF8ToWide(fake_task_name));

  const base::Time sync_time = base::Time::Now();
  const std::wstring sync_time_millis = base::NumberToWString(
      (sync_time.ToDeltaSinceWindowsEpoch() - base::Hours(1)).InMilliseconds());

  SetGlobalFlag(fake_task_reg_name, sync_time_millis);

  // Registers a task which has a config to run every 3 hours.
  fake_task_manager()->RegisterTask(fake_task_name,
                                    GenerateTaskCreator(base::Hours(5)));

  // Starts running registered tasks for all associated GCPW users.
  RunTasks();

  // First execution should happen after 4 hours as the registry says it was
  // executed an hour ago.
  task_environment()->FastForwardBy(base::Hours(3) + base::Minutes(59));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 0);

  task_environment()->FastForwardBy(base::Minutes(1));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 1);

  ASSERT_NE(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  task_environment()->FastForwardBy(base::Hours(5));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 2);
}

TEST_F(TaskManagerTest, TaskExecuted) {
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);
  std::wstring machine_guid = L"machine_guid";
  SetMachineGuidForTesting(machine_guid);

  // Create a fake user associated to a gaia id.
  CComBSTR sid1;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo@gmail.com", L"password", L"Full Name", L"comment",
                      L"test-gaia-id", std::wstring(), L"domain", &sid1));

  std::wstring device_resource_id1 = L"foo_resource_id";
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid1), L"device_resource_id",
                                  device_resource_id1));

  FakeTokenGenerator fake_token_generator;
  fake_token_generator.SetTokensForTesting(
      {base::Uuid::GenerateRandomV4().AsLowercaseString(),
       base::Uuid::GenerateRandomV4().AsLowercaseString()});

  ASSERT_EQ(S_OK, GenerateGCPWDmToken((BSTR)sid1));

  std::wstring dm_token1;
  ASSERT_EQ(S_OK, GetGCPWDmToken((BSTR)sid1, &dm_token1));

  std::string fake_task_name = "fake_task";

  fake_task_manager()->RegisterTask(fake_task_name,
                                    GenerateTaskCreator(base::Hours(3)));

  RunTasks();

  task_environment()->FastForwardBy(base::Hours(5));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 2);

  ASSERT_EQ(FakeTask::user_device_context_.size(), (size_t)1);
  extension::UserDeviceContext c1 = {device_resource_id1, serial_number,
                                     machine_guid, OLE2W(sid1), dm_token1};
  ASSERT_TRUE(FakeTask::user_device_context_[0] == c1);

  std::wstring fake_task_reg_name =
      extension::GetLastSyncRegNameForTask(base::UTF8ToWide(fake_task_name));
  ASSERT_NE(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  // Create another user associated to a gaia id.
  CComBSTR sid2;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"bar@gmail.com", L"password", L"Full Name", L"comment",
                      L"test-gaia-id2", std::wstring(), L"domain", &sid2));
  std::wstring device_resource_id2 = L"foo_resource_id";
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid2), L"device_resource_id",
                                  device_resource_id2));

  ASSERT_EQ(S_OK, GenerateGCPWDmToken((BSTR)sid2));

  std::wstring dm_token2;
  ASSERT_EQ(S_OK, GetGCPWDmToken((BSTR)sid2, &dm_token2));

  task_environment()->FastForwardBy(base::Hours(2));

  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 3);
  ASSERT_EQ(FakeTask::user_device_context_.size(), (size_t)2);

  extension::UserDeviceContext c2 = {device_resource_id2, serial_number,
                                     machine_guid, OLE2W(sid2), dm_token2};
  ASSERT_TRUE(FakeTask::user_device_context_[0] == c1);
  ASSERT_TRUE(FakeTask::user_device_context_[1] == c2);
}

TEST_F(TaskManagerTest, TasksWithDifferentPeriods) {
  std::string fake_task_name = "fake_task";
  std::string another_fake_task_name = "another_fake_task";

  fake_task_manager()->RegisterTask(fake_task_name,
                                    GenerateTaskCreator(base::Hours(3)));

  fake_task_manager()->RegisterTask(another_fake_task_name,
                                    GenerateTaskCreator(base::Hours(1)));

  // Starts running registered tasks for all associated GCPW users.
  RunTasks();

  task_environment()->FastForwardBy(base::Hours(5));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 2);
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(another_fake_task_name), 5);

  std::wstring fake_task_reg_name =
      extension::GetLastSyncRegNameForTask(base::UTF8ToWide(fake_task_name));
  ASSERT_NE(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  std::wstring another_fake_task_reg_name =
      extension::GetLastSyncRegNameForTask(
          base::UTF8ToWide(another_fake_task_name));
  ASSERT_NE(GetGlobalFlagOrDefault(another_fake_task_reg_name, L""), L"");

  task_environment()->FastForwardBy(base::Hours(2));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 3);
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(another_fake_task_name), 7);
}

TEST_F(TaskManagerTest, BackOff) {
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);
  std::wstring machine_guid = L"machine_guid";
  SetMachineGuidForTesting(machine_guid);

  // Create a fake user associated to a gaia id.
  CComBSTR sid1;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo@gmail.com", L"password", L"Full Name", L"comment",
                      L"test-gaia-id", std::wstring(), L"domain", &sid1));

  std::wstring device_resource_id1 = L"foo_resource_id";
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid1), L"device_resource_id",
                                  device_resource_id1));

  FakeTokenGenerator fake_token_generator;
  fake_token_generator.SetTokensForTesting(
      {base::Uuid::GenerateRandomV4().AsLowercaseString(),
       base::Uuid::GenerateRandomV4().AsLowercaseString()});

  ASSERT_EQ(S_OK, GenerateGCPWDmToken((BSTR)sid1));

  std::string fake_task_name = "fake_task";

  // Task::Execute returns failure 3 times and backoff mechanism kicks in.
  // 1st backoff is 1 min. 2nd backoff ins 3 mins. 3rd backoff is 6 mins.
  FakeTask::num_fails_ = 3;

  fake_task_manager()->RegisterTask(fake_task_name,
                                    GenerateTaskCreator(base::Minutes(30)));

  // Starts running registered tasks for all associated GCPW users.
  RunTasks();

  std::wstring fake_task_reg_name =
      extension::GetLastSyncRegNameForTask(base::UTF8ToWide(fake_task_name));

  // Seconds 10 - 1st execution failure
  task_environment()->FastForwardBy(base::Seconds(10));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 1);
  ASSERT_EQ(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  // Minutes 2:10 - 2nd execution failure
  task_environment()->FastForwardBy(base::Minutes(2));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 2);
  ASSERT_EQ(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  // Minutes 6:10 - 3rd execution failure
  task_environment()->FastForwardBy(base::Minutes(4));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 3);
  ASSERT_EQ(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  // Minutes 14:10 - success
  task_environment()->FastForwardBy(base::Minutes(8));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 4);
  ASSERT_NE(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");

  // Minutes 13:10 - 3 more success
  task_environment()->FastForwardBy(base::Hours(2));
  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(fake_task_name), 8);
  ASSERT_NE(GetGlobalFlagOrDefault(fake_task_reg_name, L""), L"");
}

}  // namespace testing
}  // namespace credential_provider
