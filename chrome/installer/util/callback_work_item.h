// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CALLBACK_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_CALLBACK_WORK_ITEM_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "chrome/installer/util/work_item.h"

// A work item that invokes a callback on Do() and Rollback().  In the following
// example, the function SomeWorkItemCallback() will be invoked when an item
// list is processed.
//
// // A callback invoked to do some work.
// bool SomeWorkItemCallback(const CallbackWorkItem& item) {
//   // Roll forward work goes here.  The return value indicates success/failure
//   // of the item.
//   return true;
// }
//
// // A callback invoked to roll it back.
// void SomeWorkItemRollbackCallback(const CallbackWorkItem& item) {
//   // Rollback work goes here.
// }
//
// void SomeFunctionThatAddsItemsToAList(WorkItemList* item_list) {
//   ...
//   item_list->AddCallbackWorkItem(
//       base::BindOnce(&SomeWorkItemCallback),
//       base::BindOnce(&SomeWorkItemRollbackCallback));
//   ...
// }
class CallbackWorkItem : public WorkItem {
 public:
  ~CallbackWorkItem() override;

 private:
  friend class WorkItem;

  CallbackWorkItem(
      base::OnceCallback<bool(const CallbackWorkItem&)> do_action,
      base::OnceCallback<void(const CallbackWorkItem&)> rollback_action);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  base::OnceCallback<bool(const CallbackWorkItem&)> do_action_;
  base::OnceCallback<void(const CallbackWorkItem&)> rollback_action_;

  FRIEND_TEST_ALL_PREFIXES(CallbackWorkItemTest, TestFailure);
  FRIEND_TEST_ALL_PREFIXES(CallbackWorkItemTest, TestForwardBackward);
};

#endif  // CHROME_INSTALLER_UTIL_CALLBACK_WORK_ITEM_H_
