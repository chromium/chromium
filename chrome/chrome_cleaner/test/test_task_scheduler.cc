// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_task_scheduler.h"

#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TestTaskScheduler::TestTaskScheduler() {
  TaskScheduler::SetMockDelegateForTesting(this);
}

TestTaskScheduler::~TestTaskScheduler() {
  TaskScheduler::SetMockDelegateForTesting(nullptr);
}

bool TestTaskScheduler::IsTaskRegistered(const wchar_t* task_name) {
  ADD_FAILURE() << "TestTaskScheduler::IsTaskRegistered is not implemented.";
  return false;
}

bool TestTaskScheduler::SetTaskEnabled(const wchar_t* task_name, bool enabled) {
  ADD_FAILURE() << "TestTaskScheduler::SetTaskEnabled is not implemented.";
  return false;
}

bool TestTaskScheduler::IsTaskEnabled(const wchar_t* task_name) {
  DCHECK(task_name);

  // We currently assume that all tasks are enabled. This function needs to be
  // adapted when implementing |SetTaskEnabled| in this mock class.
  return tasks_.find(task_name) != tasks_.end();
}

bool TestTaskScheduler::DeleteTask(const wchar_t* task_name) {
  DCHECK(task_name);

  tasks_.erase(task_name);
  delete_task_called_ = true;
  return true;
}

bool TestTaskScheduler::GetNextTaskRunTime(const wchar_t* task_name,
                                           base::Time* next_run_time) {
  ADD_FAILURE() << "TestTaskScheduler::GetNextTaskRunTime is not implemented.";
  return false;
}

bool TestTaskScheduler::GetTaskNameList(std::vector<std::wstring>* task_names) {
  DCHECK(task_names);

  for (const auto& task : tasks_)
    task_names->push_back(task.first);
  return true;
}

bool TestTaskScheduler::GetTaskInfo(const wchar_t* task_name,
                                    TaskScheduler::TaskInfo* info) {
  DCHECK(task_name);
  DCHECK(info);

  const auto& task_iterator = tasks_.find(task_name);
  if (task_iterator != tasks_.end()) {
    *info = task_iterator->second;
    return true;
  }
  return false;
}

bool TestTaskScheduler::RegisterTask(const wchar_t* task_name,
                                     const wchar_t* task_description,
                                     const base::CommandLine& run_command,
                                     TriggerType trigger_type,
                                     bool hidden) {
  DCHECK(task_name);
  DCHECK(task_description);

  if (!register_task_return_value_)
    return false;

  TaskExecAction task_action = {
      /* .application_path = */ run_command.GetProgram(),
      /* .working_dir = */ base::FilePath(),
      /* .arguments = */ run_command.GetArgumentsString()};

  std::vector<TaskExecAction> task_actions = {task_action};

  TaskInfo task_info;
  task_info.name = task_name;
  task_info.description = task_description;
  task_info.exec_actions = task_actions;

  tasks_[task_name] = task_info;

  register_task_called_ = true;
  return true;
}

bool TestTaskScheduler::AddTaskAction(const wchar_t* task_name,
                                      const base::CommandLine& run_command) {
  DCHECK(task_name);

  auto task_iterator = tasks_.find(task_name);
  if (task_iterator != tasks_.end()) {
    TaskExecAction task_action = {
        /* .application_path = */ run_command.GetProgram(),
        /* .working_dir = */ run_command.GetProgram().DirName(),
        /* .arguments = */ run_command.GetArgumentsString()};
    task_iterator->second.exec_actions.push_back(task_action);
    return true;
  }
  return false;
}

void TestTaskScheduler::SetRegisterTaskReturnValue(bool value) {
  register_task_return_value_ = value;
}

void TestTaskScheduler::ExpectRegisterTaskCalled(bool called) const {
  if (register_task_called_ != called)
    ADD_FAILURE() << "TestTaskScheduler::ExpectRegisterTaskCalled failed.";
}

void TestTaskScheduler::ExpectDeleteTaskCalled(bool called) const {
  if (delete_task_called_ != called)
    ADD_FAILURE() << "TestTaskScheduler::ExpectDeleteTaskCalled failed.";
}

void TestTaskScheduler::ExpectRegisteredTasksSize(size_t count) const {
  if (tasks_.size() != count) {
    ADD_FAILURE()
        << "TestTaskScheduler::ExpectRegisteredTasksSize failed: expected "
        << count << " task but got " << tasks_.size() << " tasks.";
  }
}

}  // namespace chrome_cleaner
