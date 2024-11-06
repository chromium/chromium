// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_CONTROLLER_DELEGATE_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_CONTROLLER_DELEGATE_H_

#include "base/functional/callback.h"

namespace collaboration {

// The class responsible for controlling actions on platform specific UI
// elements. This delegate is required by the CollborationController.
class CollaborationControllerDelegate {
 public:
  enum class Flow {
    kUnknown,
    kJoin,
    kShare,
  };

  struct ErrorInfo {};

  CollaborationControllerDelegate() = default;
  virtual ~CollaborationControllerDelegate() = default;

  // Disallow copy/assign.
  CollaborationControllerDelegate(const CollaborationControllerDelegate&) =
      delete;
  CollaborationControllerDelegate& operator=(
      const CollaborationControllerDelegate&) = delete;

  // Callback for informing the service whether a the UI was displayed
  // successfully.
  using ResultCallback = base::OnceCallback<void(bool)>;

  // Request to show the error page/dialog.
  virtual void ShowError(ResultCallback result, const ErrorInfo& error) = 0;

  // Request to cancel and close the current UI screen.
  virtual void Cancel(ResultCallback result) = 0;

  // Request to show the authentication screen.
  virtual void ShowAuthenticationUi(ResultCallback result) = 0;

  // Notification for when sign-in or sync status has been updated to ensure
  // that the update propagated to all relevant components.
  virtual void NotifySignInAndSyncStatusChange(ResultCallback result) = 0;

  // Request to show the invitation dialog.
  virtual void ShowJoinDialog(ResultCallback result) = 0;

  // Request to show the share dialog.
  virtual void ShowShareDialog(ResultCallback result) = 0;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_CONTROLLER_DELEGATE_H_
