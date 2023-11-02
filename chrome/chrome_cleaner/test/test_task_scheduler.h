// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_TASK_SCHEDULER_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_TASK_SCHEDULER_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/chrome_cleaner/os/task_scheduler.h"

namespace chrome_cleaner {

class TestTaskScheduler : public TaskScheduler {
 public:
  TestTaskScheduler();
  ~TestTaskScheduler() override;

  // TaskScheduler:
  bool IsTaskRegistered(const wchar_t* task_name) override;
  bool SetTaskEnabled(const wchar_t* task_name, bool enabled) override;
  bool IsTaskEnabled(const wchar_t* task_name) override;
  bool DeleteTask(const wchar_t* task_name) override;
  bool GetNextTaskRunTime(const wchar_t* task_name,
                          base::Time* next_run_time) override;
  bool GetTaskNameList(std::vector<std::wstring>* task_names) override;
  bool GetTaskInfo(const wchar_t* task_name,
                   TaskScheduler::TaskInfo* info) override;
  bool RegisterTask(const wchar_t* task_name,
                    const wchar_t* task_description,
                    const base::CommandLine& run_command,
                    TriggerType trigger_type,
                    bool hidden) override;

  // Used by tests to add a new action to an existing task.
  void SetRegisterTaskReturnValue(bool value);
  bool AddTaskAction(const wchar_t* task_name,
                     const base::CommandLine& run_command);
  void ExpectRegisterTaskCalled(bool called) const;
  void ExpectDeleteTaskCalled(bool called) const;
  void ExpectRegisteredTasksSize(size_t count) const;

 private:
  bool delete_task_called_ = false;
  bool register_task_called_ = false;
  bool register_task_return_value_ = true;
  std::map<std::wstring, TaskInfo> tasks_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_TASK_SCHEDULER_H_
