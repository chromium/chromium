// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_MANAGER_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_MANAGER_H_

#include <vector>

#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/chrome_cleaner/components/component_api.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/os/rebooter_api.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// A delegate API to be called back when all requested tasks are completed.
class ComponentManagerDelegate {
 public:
  virtual ~ComponentManagerDelegate() {}
  virtual void PreScanDone() = 0;
  virtual void PostScanDone() = 0;
  virtual void PreCleanupDone() = 0;
  virtual void PostCleanupDone() = 0;
};

// This class is used to register components that are to be executed by the main
// controller either before the scanner or after the cleaner.
class ComponentManager {
 public:
  // |delegate| must outlive the ComponentManager.
  explicit ComponentManager(ComponentManagerDelegate* delegate);

  // |CloseAllComponents| must be called before deleting the component manager.
  // Self cleanup is NOT supported!
  ~ComponentManager();

  // Add a new component. The ComponentManager takes ownership of the component.
  // This method can't be called while there is an active call to the
  // ComponentsAPI methods below that has not been completed by a call to
  // |delegate_| yet.
  void AddComponent(std::unique_ptr<ComponentAPI> component);

  // All ComponentsAPI methods are duplicated here. Each of these calls run
  // asynchronously and call their respective counterpart on |delegate_|. There
  // can only be one of these calls active at a time. Callers must wait for
  // |delegate_| to be called back before attempting other calls on this API.
  void PreScan();
  void PostScan(const std::vector<UwSId>& found_pups);
  void PreCleanup();
  void PostCleanup(ResultCode result_code, RebooterAPI* rebooter);
  // This call is synchronous so doesn't have an equivalent done call on the
  // delegate. TODO(csharp): This is confusing, fix it! b/23372645.
  void PostValidation(ResultCode result_code);

  // Call OnClose on all components and then destroy them. Any pending component
  // tasks will be canceled and any pending threads will be joined. |delegate_|
  // won't be called, even if there was a pending task. This must absolutely
  // be called before the object is destroyed.
  void CloseAllComponents(ResultCode result_code);

  // Return the number of pending tasks. Mainly used by tests.
  size_t num_tasks_pending() const { return num_tasks_pending_; }

 private:
  // Common code to post tasks to the worker threads. |component_task| is called
  // to create the closure that will run in the worker threads for each
  // component. |method_name| is used for logging.
  void PostComponentTasks(const base::RepeatingCallback<
                              base::OnceClosure(ComponentAPI*)> component_task,
                          const char* method_name);

  // Called back on the main thread when a task is completed. When the last task
  // completes, run |done_callback_| asynchronously to avoid re-entrance.
  void TaskCompleted();

  // The components that are to be called before / after the scan / cleanup.
  std::vector<std::unique_ptr<ComponentAPI>> components_;

  // The task tracker that can be used to cancel pending tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  // The delegate to be called when tasks are completed.
  ComponentManagerDelegate* delegate_;

  // The closure to callback once all tasks completed. This always contains a
  // call to |delegate_|.
  base::OnceClosure done_callback_;

  // The number of pending tasks that have not called |TaskCompleted| yet.
  size_t num_tasks_pending_ = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_MANAGER_H_
