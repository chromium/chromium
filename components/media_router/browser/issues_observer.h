// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUES_OBSERVER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUES_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/media_router/common/issue.h"

namespace media_router {

class IssueManager;

// Base class for observing Media Router related Issues. IssueObserver will
// receive at most one Issue at any given time.
// TODO(imcheng): Combine this with issue_manager.{h,cc}.
class IssuesObserver {
 public:
  explicit IssuesObserver(IssueManager* issue_manager);

  IssuesObserver(const IssuesObserver&) = delete;
  IssuesObserver& operator=(const IssuesObserver&) = delete;

  virtual ~IssuesObserver();

  // Registers with |issue_manager_| to start observing for Issues. No-ops if
  // Init() has already been called before.
  void Init();

  // Called when there is an updated Issue.
  // Note that |issue| is owned by the IssueManager that is calling the
  // observers. Implementations that wish to retain the data must make a copy
  // of |issue|.
  virtual void OnIssue(const Issue& issue) {}

  // Called when there are no more issues.
  virtual void OnIssuesCleared() {}

 private:
  const raw_ptr<IssueManager> issue_manager_;
  bool initialized_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_ISSUES_OBSERVER_H_
