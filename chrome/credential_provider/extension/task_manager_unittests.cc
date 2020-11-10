// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <atlcomcli.h>
#include <windows.h>

#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/credential_provider/extension/extension_strings.h"
#include "chrome/credential_provider/extension/task.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
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

  void TearDown() override { fake_task_manager_.Quit(); }

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

TEST_F(TaskManagerTest, PeriodicExecution) {
  ASSERT_EQ(
      GetGlobalFlagOrDefault(
          credential_provider::extension::kLastPeriodicSyncTimeRegKey, L""),
      L"");

  RunTasks();

  task_environment()->FastForwardBy(base::TimeDelta::FromHours(5));

  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(), 5);

  ASSERT_NE(
      GetGlobalFlagOrDefault(
          credential_provider::extension::kLastPeriodicSyncTimeRegKey, L""),
      L"");
  task_environment()->FastForwardBy(base::TimeDelta::FromHours(2));

  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(), 7);
}

class FakeTask : public extension::Task {
 public:
  ~FakeTask() override;

  extension::Config GetConfig() override;

  HRESULT SetContext(
      const std::vector<extension::UserDeviceContext>& c) override;

  HRESULT Execute() override;

  static int number_of_times_executed_;
  static std::vector<extension::UserDeviceContext> user_device_context_;
};

int FakeTask::number_of_times_executed_ = 0;
std::vector<extension::UserDeviceContext> FakeTask::user_device_context_;

FakeTask::~FakeTask() {}

extension::Config FakeTask::GetConfig() {
  return {};
}

HRESULT FakeTask::SetContext(
    const std::vector<extension::UserDeviceContext>& c) {
  user_device_context_ = c;
  return S_OK;
}

HRESULT FakeTask::Execute() {
  ++number_of_times_executed_;
  return S_OK;
}

std::unique_ptr<extension::Task> FakeTaskCreator() {
  auto task = std::make_unique<FakeTask>();
  return std::move(task);
}

TEST_F(TaskManagerTest, TaskExecuted) {
  base::string16 serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);
  base::string16 machine_guid = L"machine_guid";
  SetMachineGuidForTesting(machine_guid);

  FakeTokenGenerator fake_token_generator;
  fake_token_generator.SetTokensForTesting(
      {base::GenerateGUID(), base::GenerateGUID()});

  // Create a fake user associated to a gaia id.
  CComBSTR sid1;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo@gmail.com", L"password", L"Full Name", L"comment",
                      L"test-gaia-id", base::string16(), L"domain", &sid1));

  base::string16 device_resource_id1 = L"foo_resource_id";
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid1), L"device_resource_id",
                                  device_resource_id1));

  ASSERT_EQ(S_OK, GenerateGCPWDmToken((BSTR)sid1));

  base::string16 dm_token1;
  ASSERT_EQ(S_OK, GetGCPWDmToken((BSTR)sid1, &dm_token1));

  ASSERT_EQ(
      GetGlobalFlagOrDefault(
          credential_provider::extension::kLastPeriodicSyncTimeRegKey, L""),
      L"");

  fake_task_manager()->RegisterTask("fake_task",
                                    base::BindRepeating(&FakeTaskCreator));

  RunTasks();

  task_environment()->FastForwardBy(base::TimeDelta::FromHours(5));

  ASSERT_EQ(FakeTask::number_of_times_executed_, 5);
  ASSERT_EQ(FakeTask::user_device_context_.size(), (size_t)1);
  extension::UserDeviceContext c1 = {device_resource_id1, serial_number,
                                     machine_guid, OLE2W(sid1), dm_token1};
  ASSERT_TRUE(FakeTask::user_device_context_[0] == c1);

  ASSERT_NE(
      GetGlobalFlagOrDefault(
          credential_provider::extension::kLastPeriodicSyncTimeRegKey, L""),
      L"");

  // Create another user associated to a gaia id.
  CComBSTR sid2;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"bar@gmail.com", L"password", L"Full Name", L"comment",
                      L"test-gaia-id2", base::string16(), L"domain", &sid2));
  base::string16 device_resource_id2 = L"foo_resource_id";
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid2), L"device_resource_id",
                                  device_resource_id2));

  ASSERT_EQ(S_OK, GenerateGCPWDmToken((BSTR)sid2));

  base::string16 dm_token2;
  ASSERT_EQ(S_OK, GetGCPWDmToken((BSTR)sid2, &dm_token2));

  task_environment()->FastForwardBy(base::TimeDelta::FromHours(2));

  ASSERT_EQ(FakeTask::number_of_times_executed_, 7);
  ASSERT_EQ(FakeTask::user_device_context_.size(), (size_t)2);

  extension::UserDeviceContext c2 = {device_resource_id2, serial_number,
                                     machine_guid, OLE2W(sid2), dm_token2};
  ASSERT_TRUE(FakeTask::user_device_context_[0] == c1);
  ASSERT_TRUE(FakeTask::user_device_context_[1] == c2);
}

}  // namespace testing
}  // namespace credential_provider
