// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SUBMISSION_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SUBMISSION_OBSERVER_H_

namespace password_manager {

// Observer interface for the password manager about the relevant events from
// the embedder.

class FormSubmissionObserver {
 public:
  // Notifies the listener that the main frame navigation happened. Not called
  // for same document navigation. |form_may_be_submitted| true if the reason of
  // this navigation might be a form submission.
  virtual void DidNavigateMainFrame(bool form_may_be_submitted) = 0;

 protected:
  virtual ~FormSubmissionObserver() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SUBMISSION_OBSERVER_H_
