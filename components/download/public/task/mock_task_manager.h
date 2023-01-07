// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_TASK_MOCK_TASK_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_TASK_MOCK_TASK_MANAGER_H_

#include "components/download/public/task/task_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockTaskManager : public TaskManager {
 public:
  MockTaskManager();
  ~MockTaskManager() override;

  MOCK_METHOD2(ScheduleTask, void(DownloadTaskType, const TaskParams&));
  MOCK_METHOD1(UnscheduleTask, void(DownloadTaskType));
  MOCK_METHOD2(OnStartScheduledTask,
               void(DownloadTaskType, TaskFinishedCallback));
  MOCK_METHOD1(OnStopScheduledTask, void(DownloadTaskType));
  MOCK_METHOD2(NotifyTaskFinished, void(DownloadTaskType, bool));
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_TASK_MOCK_TASK_MANAGER_H_
