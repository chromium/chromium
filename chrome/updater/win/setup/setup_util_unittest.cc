// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/test/test_executables.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(SetupUtilTest, DeleteLegacyEntriesPerUser) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    return;
  }

  ASSERT_TRUE(DeleteLegacyEntriesPerUser());

  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

#define INTERFACE_PAIR(interface) \
  std::make_pair(__uuidof(interface), L#interface)

  for (const auto& [iid, interface_name] :
       {INTERFACE_PAIR(IProcessLauncher), INTERFACE_PAIR(IProcessLauncher2)}) {
    AddInstallComInterfaceWorkItems(HKEY_CURRENT_USER,
                                    base::FilePath(L"C:\\foo.exe"), iid,
                                    interface_name, list.get());
  }
#undef INTERFACE_PAIR

  ASSERT_TRUE(list->Do());

  ASSERT_TRUE(DeleteLegacyEntriesPerUser());
}

TEST(SetupUtilTest, RegisterTypeLibs) {
  ASSERT_HRESULT_SUCCEEDED(RegisterTypeLibs(GetUpdaterScopeForTesting(), true));
  ASSERT_HRESULT_SUCCEEDED(
      RegisterTypeLibs(GetUpdaterScopeForTesting(), false));
}

class SetupUtilRegisterWakeTaskWorkItemTests : public ::testing::Test {
 public:
  void SetUp() override {
    task_scheduler_ =
        TaskScheduler::CreateInstance(GetUpdaterScopeForTesting());
    ASSERT_TRUE(task_scheduler_);
  }

  void TearDown() override {
    std::wstring task_name(GetTaskName(GetUpdaterScopeForTesting()));
    EXPECT_TRUE(task_name.empty());
    while (!task_name.empty()) {
      EXPECT_TRUE(task_scheduler_->DeleteTask(task_name.c_str()));
      task_name = GetTaskName(GetUpdaterScopeForTesting());
    }
  }

 protected:
  scoped_refptr<TaskScheduler> task_scheduler_;
  const base::CommandLine command_line_ =
      GetTestProcessCommandLine(GetUpdaterScopeForTesting(),
                                test::GetTestName());
};

TEST_F(SetupUtilRegisterWakeTaskWorkItemTests, TaskDoesNotExist) {
  ASSERT_TRUE(GetTaskName(GetUpdaterScopeForTesting()).empty());

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddWorkItem(
      new RegisterWakeTaskWorkItem(command_line_, GetUpdaterScopeForTesting()));
  ASSERT_TRUE(install_list->Do());
  ASSERT_FALSE(GetTaskName(GetUpdaterScopeForTesting()).empty());

  install_list->Rollback();
  ASSERT_TRUE(GetTaskName(GetUpdaterScopeForTesting()).empty());
}

TEST_F(SetupUtilRegisterWakeTaskWorkItemTests, TaskAlreadyExists) {
  ASSERT_TRUE(GetTaskName(GetUpdaterScopeForTesting()).empty());

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddWorkItem(
      new RegisterWakeTaskWorkItem(command_line_, GetUpdaterScopeForTesting()));
  ASSERT_TRUE(install_list->Do());
  ASSERT_FALSE(GetTaskName(GetUpdaterScopeForTesting()).empty());

  std::unique_ptr<WorkItemList> install_list_task_exists(
      WorkItem::CreateWorkItemList());
  install_list_task_exists->AddWorkItem(
      new RegisterWakeTaskWorkItem(command_line_, GetUpdaterScopeForTesting()));
  ASSERT_TRUE(install_list_task_exists->Do());
  ASSERT_FALSE(GetTaskName(GetUpdaterScopeForTesting()).empty());

  install_list_task_exists->Rollback();
  ASSERT_FALSE(GetTaskName(GetUpdaterScopeForTesting()).empty());

  install_list->Rollback();
  ASSERT_TRUE(GetTaskName(GetUpdaterScopeForTesting()).empty());
}

}  // namespace updater
