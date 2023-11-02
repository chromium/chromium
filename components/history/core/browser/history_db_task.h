// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_DB_TASK_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_DB_TASK_H_

namespace history {

class HistoryBackend;
class HistoryDatabase;

// HistoryDBTask can be used to process arbitrary work on the history backend
// thread. HistoryDBTask is scheduled using HistoryService::ScheduleDBTask.
// When HistoryBackend processes the task it invokes RunOnDBThread. Once the
// task completes and has not been canceled, DoneRunOnMainThread is invoked back
// on the main thread, after which this object is destroyed, also on the
// main thread.
class HistoryDBTask {
 public:
  virtual ~HistoryDBTask() {}

  // Invoked on the database thread. The return value indicates whether the
  // task is done. A return value of true signals the task is done and
  // RunOnDBThread should NOT be invoked again. A return value of false
  // indicates the task is not done, and should be run again after other
  // tasks are given a chance to be processed.
  virtual bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) = 0;

  // Invoked on the main thread once RunOnDBThread has returned true. This is
  // only invoked if the request was not canceled and returned true from
  // RunOnDBThread.
  virtual void DoneRunOnMainThread() = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_DB_TASK_H_
