// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/issue_manager.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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
  }
  NOTREACHED();
  return base::TimeDelta();
}

IssueManager::IssueManager()
    : top_issue_(nullptr), task_runner_(content::GetUIThreadTaskRunner({})) {}

IssueManager::~IssueManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IssueManager::AddIssue(const IssueInfo& issue_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& key_value_pair : issues_map_) {
    const auto& issue = key_value_pair.second->issue;
    if (issue.info() == issue_info)
      return;
  }

  Issue issue(issue_info);
  std::unique_ptr<base::CancelableOnceClosure> cancelable_dismiss_cb;
  base::TimeDelta timeout = GetAutoDismissTimeout(issue_info);
  if (!timeout.is_zero()) {
    cancelable_dismiss_cb =
        std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
            &IssueManager::ClearIssue, base::Unretained(this), issue.id()));
    task_runner_->PostDelayedTask(FROM_HERE, cancelable_dismiss_cb->callback(),
                                  timeout);
  }

  issues_map_.emplace(issue.id(), std::make_unique<IssueManager::Entry>(
                                      issue, std::move(cancelable_dismiss_cb)));
  MaybeUpdateTopIssue();
}

void IssueManager::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (issues_map_.erase(issue_id))
    MaybeUpdateTopIssue();
}

void IssueManager::ClearAllIssues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (issues_map_.empty())
    return;

  issues_map_.clear();
  MaybeUpdateTopIssue();
}

void IssueManager::ClearTopIssueForSink(const MediaSink::Id& sink_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!top_issue_ || top_issue_->info().sink_id != sink_id)
    return;

  ClearIssue(top_issue_->id());
}

void IssueManager::RegisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  DCHECK(!issues_observers_.HasObserver(observer));

  issues_observers_.AddObserver(observer);
  if (top_issue_)
    observer->OnIssue(*top_issue_);
}

void IssueManager::UnregisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  issues_observers_.RemoveObserver(observer);
}

IssueManager::Entry::Entry(
    const Issue& issue,
    std::unique_ptr<base::CancelableOnceClosure> cancelable_dismiss_callback)
    : issue(issue),
      cancelable_dismiss_callback(std::move(cancelable_dismiss_callback)) {}

IssueManager::Entry::~Entry() = default;

void IssueManager::MaybeUpdateTopIssue() {
  const Issue* new_top_issue = nullptr;
  // Select the first issue in the list of issues.
  if (!issues_map_.empty()) {
    new_top_issue = &issues_map_.begin()->second->issue;
  }

  // If we've found a new top issue, then report it via the observer.
  if (new_top_issue != top_issue_) {
    top_issue_ = new_top_issue;
    for (auto& observer : issues_observers_) {
      if (top_issue_)
        observer.OnIssue(*top_issue_);
      else
        observer.OnIssuesCleared();
    }
  }
}

}  // namespace media_router
