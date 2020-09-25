// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/common/issue.h"

namespace media_router {

// IssueManager keeps track of current issues related to casting
// connectivity and quality. It lives on the UI thread.
class IssueManager {
 public:
  IssueManager();
  ~IssueManager();

  // Returns the amount of time before |issue_info| is dismissed after it is
  // added to the IssueManager. Returns base::TimeDelta() if the given IssueInfo
  // is not auto-dismissed.
  static base::TimeDelta GetAutoDismissTimeout(const IssueInfo& issue_info);

  // Adds an issue. No-ops if the issue already exists.
  // |issue_info|: Info of issue to be added.
  void AddIssue(const IssueInfo& issue_info);

  // Removes an issue when user has noted it is resolved.
  // |issue_id|: Issue::Id of the issue to be removed.
  void ClearIssue(const Issue::Id& issue_id);

  // Clears all non-blocking issues.
  void ClearNonBlockingIssues();

  // Registers an issue observer |observer|. The observer will be triggered
  // when the highest priority issue changes.
  // If there is already an observer registered with this instance, do nothing.
  // Does not assume ownership of |observer|.
  // |observer|: IssuesObserver to be registered.
  void RegisterObserver(IssuesObserver* observer);

  // Unregisters |observer| from |issues_observers_|.
  // |observer|: IssuesObserver to be unregistered.
  void UnregisterObserver(IssuesObserver* observer);

  void set_task_runner_for_test(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    task_runner_ = task_runner;
  }

 private:
  // Issues tracked internally by the IssueManager.
  // TODO(imcheng): Rather than holding a base::CancelableClosure, it might be a
  // bit simpler to use a CancelableTaskTracker and track TaskIds here. This
  // will require adding support for delayed tasks to CancelableTaskTracker.
  struct Entry {
    Entry(const Issue& issue,
          std::unique_ptr<base::CancelableOnceClosure>
              cancelable_dismiss_callback);
    ~Entry();

    Issue issue;

    // Set to non-null if |issue| can be auto-dismissed.
    std::unique_ptr<base::CancelableOnceClosure> cancelable_dismiss_callback;

   private:
    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  // Checks if the current top issue has changed. Updates |top_issue_|.
  // If |top_issue_| has changed, observers in |issues_observers_| will be
  // notified of the new top issue.
  void MaybeUpdateTopIssue();

  base::small_map<std::map<Issue::Id, std::unique_ptr<Entry>>> blocking_issues_;
  base::small_map<std::map<Issue::Id, std::unique_ptr<Entry>>>
      non_blocking_issues_;

  // IssueObserver instances are not owned by the manager.
  base::ObserverList<IssuesObserver>::Unchecked issues_observers_;

  // Pointer to the top Issue in |issues_|, or |nullptr| if there are no issues.
  const Issue* top_issue_;

  // The SingleThreadTaskRunner that this IssueManager runs on, and is used
  // for posting issue auto-dismissal tasks.
  // When a non-blocking issues is added to the IssueManager, a delayed task
  // will be added to remove the issue. This is done to automatically clean up
  // issues that are no longer relevant.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IssueManager);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_
