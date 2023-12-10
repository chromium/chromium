// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_API_TASKS_CHROME_TASKS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_API_TASKS_CHROME_TASKS_DELEGATE_H_

#include "ash/api/tasks/tasks_delegate.h"
#include "base/containers/flat_map.h"
#include "components/account_id/account_id.h"

namespace ash {

class TasksClientImpl;

namespace api {

class ChromeTasksDelegate : public TasksDelegate {
 public:
  ChromeTasksDelegate();
  ChromeTasksDelegate(const ChromeTasksDelegate&) = delete;
  ChromeTasksDelegate& operator=(const ChromeTasksDelegate&) = delete;
  ~ChromeTasksDelegate() override;

  // TasksDelegate:
  void UpdateClientForProfileSwitch(const AccountId& account_id) override;
  void GetTaskLists(TasksClient::GetTaskListsCallback callback) override;
  void GetTasks(const std::string& task_list_id,
                TasksClient::GetTasksCallback callback) override;
  void MarkAsCompleted(const std::string& task_list_id,
                       const std::string& task_id,
                       bool completed) override;
  void SendCompletedTasks() override;
  void AddTask(const std::string& task_list_id,
               const std::string& title) override;
  void UpdateTaskTitle(const std::string& task_list_id,
                       const std::string& task_id,
                       const std::string& title) override;

 private:
  // Returns the `TasksClientImpl` associated with the `active_account_id_`.
  // Returns nullptr if the client cannot be found.
  TasksClientImpl* GetActiveAccountClient() const;

  // The id of the currently active user account.
  AccountId active_account_id_;

  // The clients that communicate with the Google Tasks API on behalf of each
  // account.
  base::flat_map<AccountId, std::unique_ptr<TasksClientImpl>> clients_;
};

}  // namespace api

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_API_TASKS_CHROME_TASKS_DELEGATE_H_
