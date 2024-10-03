// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"

#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "content/public/browser/navigation_handle.h"

namespace safe_browsing {

PasswordProtectionCommitDeferringCondition::
    PasswordProtectionCommitDeferringCondition(
        content::NavigationHandle& navigation_handle,
        PasswordProtectionRequestContent& request)
    : content::CommitDeferringCondition(navigation_handle),
      request_(request.AsWeakPtrImpl()) {
  DCHECK(request_);
  request_->AddDeferredNavigation(*this);
}

PasswordProtectionCommitDeferringCondition::
    ~PasswordProtectionCommitDeferringCondition() {
  if (request_)
    request_->RemoveDeferredNavigation(*this);
}

content::CommitDeferringCondition::Result
PasswordProtectionCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  if (invoke_callback_for_testing_)
    std::move(invoke_callback_for_testing_).Run();

  // The request may have asked for a resumption before this condition was
  // executed. In that case, proceed without deferring.
  if (navigation_was_resumed_)
    return Result::kProceed;

  // If the request was already deleted, it should have called
  // ResumeNavigation.
  DCHECK(request_);

  resume_ = std::move(resume);
  return Result::kDefer;
}

const char* PasswordProtectionCommitDeferringCondition::TraceEventName() const {
  return "PasswordProtectionCommitDeferringCondition";
}

void PasswordProtectionCommitDeferringCondition::ResumeNavigation() {
  navigation_was_resumed_ = true;

  if (resume_)
    std::move(resume_).Run();

  // Warning: Run() may have deleted `self`.
}

}  // namespace safe_browsing
