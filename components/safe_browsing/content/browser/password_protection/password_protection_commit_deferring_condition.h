// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_COMMIT_DEFERRING_CONDITION_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_COMMIT_DEFERRING_CONDITION_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {
class PasswordProtectionRequestContent;

// PasswordProtectionCommitDeferringCondition defers navigations under the
// following conditions:
// (1) The navigation starts while there is a on-going sync password
//     reuse ping. When the verdict comes back, if the verdict results in
//     showing a modal warning dialog, the navigation continues to be deferred;
//     otherwise, the deferred navigation will be resumed.
// (2) The navigation starts while there is a modal warning showing.
//
// If a modal warning is showing, the navigation will continue to be deferred
// until the user takes an action and dismisses the dialog, at which point the
// navigation will resume.
class PasswordProtectionCommitDeferringCondition
    : public content::CommitDeferringCondition {
 public:
  PasswordProtectionCommitDeferringCondition(
      content::NavigationHandle& navigation_handle,
      PasswordProtectionRequestContent& request);

  PasswordProtectionCommitDeferringCondition(
      const PasswordProtectionCommitDeferringCondition&) = delete;
  PasswordProtectionCommitDeferringCondition& operator=(
      const PasswordProtectionCommitDeferringCondition&) = delete;

  ~PasswordProtectionCommitDeferringCondition() override;

  // Overrides content::CommitDeferringCondition
  //
  // Called from a NavigationHandle when a navigation is ready and about to
  // commit. Unless ResumeNavigation() was already called, this will return
  // kDefer which will defer the navigation from committing until `resume` is
  // invoked (via `ResumeNavigation()`).
  Result WillCommitNavigation(base::OnceClosure resume) override;

  const char* TraceEventName() const override;

  // Called by the PasswordProtectionService when it decides this navigation no
  // longer needs to be deferred (e.g. because a ping resulted in not showing a
  // modal, or a shown modal was dismissed by the user). If called after this
  // navigation tried to commit, this invokes the `resume` closure passed to
  // WillCommitNavigation. If called before WillCommitNavigation,
  // WillCommitNavigation will not defer the commit.
  void ResumeNavigation();

  // For testing only. Registers a callback that will be invoked when
  // WillCommitNavigatoin is called by the navigation. This is useful for
  // yielding in a test when this condition is invoked.
  void register_invoke_callback_for_testing(base::OnceClosure callback) {
    invoke_callback_for_testing_ = std::move(callback);
  }

 private:
  // A pointer to the PasswordProtectionRequestContent on whose behalf this
  // condition is deferring navigations.
  base::WeakPtr<PasswordProtectionRequestContent> request_;

  // The resume closure used to resume the navigation past this
  // CommitDeferringCondition.
  base::OnceClosure resume_;

  base::OnceClosure invoke_callback_for_testing_;

  // Set to true to indicate the request has asked to resume the navigation,
  // i.e. the request result didn't result in a modal being shown, or the user
  // has dismissed the modal.
  bool navigation_was_resumed_ = false;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_COMMIT_DEFERRING_CONDITION_H_
