// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/issues_observer.h"

#include "base/check.h"
#include "components/media_router/browser/issue_manager.h"

namespace media_router {

IssuesObserver::IssuesObserver(IssueManager* issue_manager)
    : issue_manager_(issue_manager), initialized_(false) {
  DCHECK(issue_manager_);
}

IssuesObserver::~IssuesObserver() {
  if (initialized_)
    issue_manager_->UnregisterObserver(this);
}

void IssuesObserver::Init() {
  if (initialized_)
    return;

  issue_manager_->RegisterObserver(this);
  initialized_ = true;
}

}  // namespace media_router
