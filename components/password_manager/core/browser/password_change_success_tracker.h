// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace password_manager {

// Abstract interface to track whether a change password flow leads to a
// successful password update.
// Usage notes:
// - |OnChangePasswordFlowStarted| is supposed to be called when a change flow
// starts.
// - |OnChangePasswordFlowSucceeded| is supposed to be called when a compromised
// password is updated or at the end of kAutomatedResetLinkRequestFlow when
// the email with the reset link or code was requested.
// - |url| param is the url of the current page. The tracker is supposed to
// normalize it to eTLD+1 for matching.
class PasswordChangeSuccessTracker : public KeyedService {
 public:
  // Start and end events for automated (i.e. Autofill Assistant-driven) and
  // manual flows (i.e. Chrome opens a CCT and a user updates a password on
  // their own).
  enum class StartEvent {
    // Some automated flow started. A specific type will be known only at the
    // end.
    kAutomatedFlow = 0,

    // Flow started with opening an origin of a stored credential (which is
    // supposed to be the homepage).
    kManualHomepageFlow = 1,
    // Flow started with opening the .well-known/change-password path.
    KManualWellKnownUrlFlow = 2,
    // Flow started with opening a domain-specific URL.
    kManualChangePasswordUrlFlow = 3,

    // Flow started as an automated flow that requested a reset link, now a user
    // is supposed to open the link and update the password manually. Should be
    // used only internally when kAutomatedResetLinkRequestFlow ends.
    kManualResetLinkOpenningFlow = 4,
  };
  enum class EndEvent {
    // Fully automated flow completed.
    kAutomatedGeneratedPasswordFlow = 0,
    // Flow started as an automated flow, but a user chose to create their own
    // password.
    kAutomatedOwnPasswordFlow = 1,
    // Password-reset link was requested. Autofill Assistant's part is done and
    // a user is supposed to continue the flow on their own.
    kAutomatedResetLinkRequestFlow = 2,

    // Some manual flow completed. A specific type is known only at the start.
    kManualFlow = 3,
  };

  // Called when a change flow starts. Emits a report that a flow has started
  // and stores an entry to wait for a matching FlowSucceeded call. If there is
  // no matching call for a while, the entry will expire.
  virtual void OnChangePasswordFlowStarted(const GURL& url,
                                           const std::string& username,
                                           StartEvent event_type) = 0;

  // Called when a change flow succeeds. If there is a recent matching
  // FlowStarted call, it emits a report about a successful password change.
  virtual void OnChangePasswordFlowCompleted(const GURL& url,
                                             const std::string& username,
                                             EndEvent event_type) = 0;
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_H_
