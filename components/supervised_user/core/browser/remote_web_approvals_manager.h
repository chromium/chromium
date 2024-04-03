// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_REMOTE_WEB_APPROVALS_MANAGER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_REMOTE_WEB_APPROVALS_MANAGER_H_

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"

class GURL;

namespace supervised_user {

class PermissionRequestCreator;
class UrlFormatter;

// Manages remote web approval requests from Family Link users.
//
// Remote requests are forwarded to the guardian and processed asynchronously.
// The result of the remote approval syncs as a new web rule to the client and
// is not handled in this class.
class RemoteWebApprovalsManager {
 public:
  // Callback indicating whether the URL access request was initiated
  // successfully.
  using ApprovalRequestInitiatedCallback = base::OnceCallback<void(bool)>;

  RemoteWebApprovalsManager();

  RemoteWebApprovalsManager(const RemoteWebApprovalsManager&) = delete;
  RemoteWebApprovalsManager& operator=(const RemoteWebApprovalsManager&) =
      delete;

  ~RemoteWebApprovalsManager();

  // Adds an approval request for the `url`.
  // The `callback` is run when the request was sent or sending of the request
  // failed.
  void RequestApproval(const GURL& url,
                       const UrlFormatter& url_formatter,
                       ApprovalRequestInitiatedCallback callback);

  // Returns whether approval requests are enabled.
  bool AreApprovalRequestsEnabled() const;

  // Adds remote approval request `creator` to handle remote approval requests.
  void AddApprovalRequestCreator(
      std::unique_ptr<PermissionRequestCreator> creator);

  // Clears all approval requests creators.
  void ClearApprovalRequestsCreators();

 private:
  using CreateApprovalRequestCallback =
      base::RepeatingCallback<void(PermissionRequestCreator*,
                                   ApprovalRequestInitiatedCallback)>;

  size_t FindEnabledRemoteApprovalRequestCreator(size_t start) const;

  void AddApprovalRequestInternal(
      const CreateApprovalRequestCallback& create_request,
      ApprovalRequestInitiatedCallback callback,
      size_t index);

  void OnApprovalRequestIssued(
      const CreateApprovalRequestCallback& create_request,
      ApprovalRequestInitiatedCallback callback,
      size_t index,
      bool success);

  // Stores approval request creators.
  // The creators are cleared during shutdown.
  std::vector<std::unique_ptr<PermissionRequestCreator>>
      approval_request_creators_;

  base::WeakPtrFactory<RemoteWebApprovalsManager> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_REMOTE_WEB_APPROVALS_MANAGER_H_
