// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/issue_manager.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace media_router {

namespace {

// The number of minutes a NOTIFICATION Issue stays in the IssueManager
// before it is auto-dismissed.
constexpr int kNotificationAutoDismissMins = 1;

// The number of minutes a WARNING Issue stays in the IssueManager before it
// is auto-dismissed.
constexpr int kWarningAutoDismissMins = 5;

}  // namespace

// static
base::TimeDelta IssueManager::GetAutoDismissTimeout(
    const IssueInfo& issue_info) {
  switch (issue_info.severity) {
    case IssueInfo::Severity::NOTIFICATION:
      return base::Minutes(kNotificationAutoDismissMins);
    case IssueInfo::Severity::WARNING:
      return base::Minutes(kWarningAutoDismissMins);
    default:
      NOTREACHED();
  }
}

IssueManager::IssueManager() = default;
IssueManager::~IssueManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IssueManager::AddIssue(const IssueInfo& issue_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& key_value_pair : issues_map_) {
    if (key_value_pair.second.info() == issue_info) {
      return;
    }
  }

  Issue issue = Issue::CreateIssueWithIssueInfo(issue_info);
  // No-op if the task is invoked after the issue is cleared.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IssueManager::ClearIssue, weak_ptr_factory_.GetWeakPtr(),
                     issue.id()),
      GetAutoDismissTimeout(issue_info));

  issues_map_.emplace(issue.id(), issue);
  MaybeUpdateTopIssue();
}

void IssueManager::AddPermissionRejectedIssue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Issue issue = Issue::CreatePermissionRejectedIssue();
  issues_map_.clear();
  issues_map_.emplace(issue.id(), issue);
  MaybeUpdateTopIssue();
}

void IssueManager::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (issues_map_.erase(issue_id)) {
    MaybeUpdateTopIssue();
  }
}

void IssueManager::ClearAllIssues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (issues_map_.empty()) {
    return;
  }

  issues_map_.clear();
  MaybeUpdateTopIssue();
}

void IssueManager::ClearTopIssueForSink(const MediaSink::Id& sink_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto issue_it = issues_map_.find(top_issue_id_.value_or(-1));
  if (issue_it != issues_map_.end() &&
      issue_it->second.info().sink_id == sink_id) {
    ClearIssue(top_issue_id_.value());
  }
}

void IssueManager::RegisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  DCHECK(!issues_observers_.HasObserver(observer));

  issues_observers_.AddObserver(observer);
  auto issue_it = issues_map_.find(top_issue_id_.value_or(-1));
  if (issue_it != issues_map_.end()) {
    observer->OnIssue(issue_it->second);
  }
}

void IssueManager::UnregisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  issues_observers_.RemoveObserver(observer);
}

void IssueManager::MaybeUpdateTopIssue() {
  if (issues_map_.empty()) {
    top_issue_id_ = std::nullopt;
    for (auto& observer : issues_observers_) {
      observer.OnIssuesCleared();
    }
    return;
  }

  // Select the first issue in the list of issues.
  Issue::Id new_top_issue_id = issues_map_.begin()->first;
  if (top_issue_id_.has_value() && new_top_issue_id == top_issue_id_.value()) {
    return;
  }

  // If we've found a new top issue, then report it via the observer.
  top_issue_id_ = new_top_issue_id;
  for (auto& observer : issues_observers_) {
    observer.OnIssue(issues_map_.at(new_top_issue_id));
  }
}

}  // namespace media_router
