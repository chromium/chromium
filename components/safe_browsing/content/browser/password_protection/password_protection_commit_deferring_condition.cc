// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"

#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "content/public/browser/navigation_handle.h"

namespace safe_browsing {

PasswordProtectionCommitDeferringCondition::
    PasswordProtectionCommitDeferringCondition(
        content::NavigationHandle& navigation_handle,
        scoped_refptr<PasswordProtectionRequestContent> request)
    : content::CommitDeferringCondition(navigation_handle), request_(request) {
  DCHECK(request_);
  request_->AddDeferredNavigation(*this);
}

PasswordProtectionCommitDeferringCondition::
    ~PasswordProtectionCommitDeferringCondition() {
  // It's ok we won't call RemoveDeferredNavigation if !navigation_was_resumed
  // since `request_` will clean up all conditions after calling
  // ResumeNavigation.
  if (!navigation_was_resumed_)
    request_->RemoveDeferredNavigation(*this);
}

content::CommitDeferringCondition::Result
PasswordProtectionCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  // The request may have asked for a resumption before this condition was
  // executed. In that case, proceed without deferring.
  if (navigation_was_resumed_)
    return Result::kProceed;

  resume_ = std::move(resume);
  return Result::kDefer;
}

void PasswordProtectionCommitDeferringCondition::ResumeNavigation() {
  navigation_was_resumed_ = true;

  if (resume_)
    std::move(resume_).Run();

  // Warning: Run() may have deleted `self`.
}

}  // namespace safe_browsing
