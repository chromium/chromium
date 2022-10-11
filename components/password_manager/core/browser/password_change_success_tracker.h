// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_H_

#include <string>

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace password_manager {

// Abstract interface to track whether a change password flow leads to a
// successful update of a credential.
// Usage notes:
// - The type of a change password flow is defined by three enums, a
//   |StartEvent| defining how the flow was started, an |EndEvent| defining
//   how the flow terminated and an |EntryPoint| defining from where in the
//   browser the flow was started. See the enum definitions for details.
// - URL parameters in the passed to the tracker are the URLs of the current
//   page. The tracker normalizes these to eTLD+1 for matching.
class PasswordChangeSuccessTracker : public KeyedService {
 public:
  // Timeout length between the start and end events of a password change flow.
  // Since the flows include password reset for which reset emails may be
  // delayed, a somewhat lengthy timeout is chosen.
  static constexpr base::TimeDelta kFlowTimeout = base::Minutes(60);

  // Timeout length between the start of a manual change flow and its type
  // refinement. The expectation is that this should be near instant and
  // the timeout is just a safe-keeping measure.
  static constexpr base::TimeDelta kFlowTypeRefinementTimeout =
      base::Seconds(30);

  // Start and end events for automated (i.e. Autofill Assistant-driven) and
  // manual flows (i.e. Chrome opens a CCT and a user updates a password on
  // their own).
  // These values are persisted to prefs and used in enums.xml; do not reorder
  // or renumber entries!
  enum class StartEvent {
    // An automated password change flow.
    kAutomatedFlow = 0,

    // A manual password change flow of unknown type since no navigation has
    // been attempted yet.
    kManualUnknownFlow = 1,

    // A manual password change flow for a domain that supports
    // /.well-known/change-password.
    kManualWellKnownUrlFlow = 2,

    // A manual password change flow with a domain-specific URL.
    kManualChangePasswordUrlFlow = 3,

    // A manual password change flow that starts at the origin of a stored
    // credential (supposed to be the homepage).
    kManualHomepageFlow = 4,

    // A manual password reset flow. This StartEvent can currently only be
    // triggered during an automated password change flow for which login fails
    // and the user chooses to request a password reset.
    kManualResetLinkFlow = 5,

    kMaxValue = kManualResetLinkFlow
  };

  // These values are persisted to prefs and used in enums.xml; do not reorder
  // or renumber entries!
  enum class EndEvent {
    // Automated password change flow completed with a generated password.
    kAutomatedFlowGeneratedPasswordChosen = 0,

    // Automated password change flow completed with a user-chosen password.
    kAutomatedFlowOwnPasswordChosen = 1,

    // Password-reset link was requested. Autofill Assistant's part is done and
    // a user is supposed to continue the flow on their own.
    kAutomatedFlowResetLinkRequested = 2,

    // A manual password change flow or password reset flow completed with a
    // generated password.
    kManualFlowGeneratedPasswordChosen = 3,

    // A manual password change flow or password reset flow completed with a
    // user-chosen password.
    kManualFlowOwnPasswordChosen = 4,

    // The password change flow timed out.
    kTimeout = 5,

    kMaxValue = kTimeout
  };

  // The place in Chrome where the password change flow originated.
  // These values are persisted to prefs and used in enums.xml; do not reorder
  // or renumber entries!
  enum class EntryPoint {
    // Started after performing a password check in settings / the password
    // manager.
    kLeakCheckInSettings = 0,

    // Started after receiving a warning after logging into a website with
    // a leaked credential.
    kLeakWarningDialog = 1,

    kMaxValue = kLeakWarningDialog
  };

  // Called when a change flow starts and its |StartEvent| is fully known (
  // currently true only for automated flows). It stores an entry to wait
  // for a matching |OnChangePasswordFlowModified()| or
  // |OnChangePasswordFlowCompleted()| call. Times out after |kFlowTimeout|
  // if no matching EndEvent is received.
  virtual void OnChangePasswordFlowStarted(const GURL& url,
                                           const std::string& username,
                                           StartEvent event_type,
                                           EntryPoint entry_point) = 0;

  // Called when a manual change flow is started. At that point, the
  // exact |StartEvent| (i.e. whether it supports .well-known/change-password)
  // is not yet known and the flow is stored temporarily
  // with |StartEvent::kManualUnknownFlow|.
  virtual void OnManualChangePasswordFlowStarted(const GURL& url,
                                                 const std::string& username,
                                                 EntryPoint entry_point) = 0;

  // Called when a change flow with an unknown username is refined, e.g. the
  // exact |StartEvent| of a manual flow is specified.
  virtual void OnChangePasswordFlowModified(const GURL& url,
                                            StartEvent new_event_type) = 0;

  // Call when a change flow with a known username is modified, e.g. a flow
  // that started as an automated password change and became a password reset.
  virtual void OnChangePasswordFlowModified(const GURL& url,
                                            const std::string& username,
                                            StartEvent new_event_type) = 0;

  // Called when a change flow succeeds and a password is updated.
  // If there is a matching flow that for which a start was registered,
  // it emits a report about a successful password change.
  virtual void OnChangePasswordFlowCompleted(const GURL& url,
                                             const std::string& username,
                                             EndEvent event_type,
                                             bool phished) = 0;
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_H_
