// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_

#include <stddef.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/common/issue.h"

namespace media_router {

// IssueManager keeps track of current issues related to casting
// connectivity and quality. It lives on the UI thread.
class IssueManager {
 public:
  IssueManager();

  IssueManager(const IssueManager&) = delete;
  IssueManager& operator=(const IssueManager&) = delete;

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

  // Clears all issues.
  void ClearAllIssues();

  // Clears the top issue if it belongs to the given sink_id.
  void ClearTopIssueForSink(const MediaSink::Id& sink_id);

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
  // TODO(imcheng): Rather than holding a base::CancelableOnceClosure, it might
  // be a bit simpler to use a CancelableTaskTracker and track TaskIds here.
  // This will require adding support for delayed tasks to
  // CancelableTaskTracker.
  struct Entry {
    Entry(const Issue& issue,
          std::unique_ptr<base::CancelableOnceClosure>
              cancelable_dismiss_callback);

    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

    ~Entry();

    Issue issue;

    // Set to non-null if |issue| can be auto-dismissed.
    std::unique_ptr<base::CancelableOnceClosure> cancelable_dismiss_callback;
  };

  // Checks if the current top issue has changed. Updates |top_issue_|.
  // If |top_issue_| has changed, observers in |issues_observers_| will be
  // notified of the new top issue.
  void MaybeUpdateTopIssue();

  base::flat_map<Issue::Id, std::unique_ptr<Entry>> issues_map_;

  // IssueObserver instances are not owned by the manager.
  base::ObserverList<IssuesObserver>::Unchecked issues_observers_;

  // Pointer to the top Issue in |issues_|, or |nullptr| if there are no issues.
  raw_ptr<const Issue, DanglingUntriaged> top_issue_;

  // The SingleThreadTaskRunner that this IssueManager runs on, and is used
  // for posting issue auto-dismissal tasks.
  // When an issue is added to the IssueManager, a delayed task
  // will be added to remove the issue. This is done to automatically clean up
  // issues that are no longer relevant.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_
