// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
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
  // added to the IssueManager.
  static base::TimeDelta GetAutoDismissTimeout(const IssueInfo& issue_info);

  // Adds an issue. No-ops if the issue already exists.
  // |issue_info|: Info of issue to be added.
  void AddIssue(const IssueInfo& issue_info);

  // Adds an issue for local discovery permission rejected error.
  void AddPermissionRejectedIssue();

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

 private:
  // Checks if the current top issue has changed. Updates |top_issue_|.
  // If |top_issue_| has changed, observers in |issues_observers_| will be
  // notified of the new top issue.
  void MaybeUpdateTopIssue();

  base::flat_map<Issue::Id, Issue> issues_map_;

  // IssueObserver instances are not owned by the manager.
  base::ObserverList<IssuesObserver>::Unchecked issues_observers_;

  // Pointer to the top Issue in `|issues_map_|, or |nullopt| if there are no
  // issues.
  std::optional<Issue::Id> top_issue_id_ = std::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IssueManager> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUE_MANAGER_H_
