// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/remote_web_approvals_manager.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/supervised_user/core/browser/permission_request_creator.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "url/gurl.h"

namespace {

void CreateURLAccessRequest(
    const GURL& url,
    supervised_user::PermissionRequestCreator* creator,
    supervised_user::RemoteWebApprovalsManager::ApprovalRequestInitiatedCallback
        callback) {
  creator->CreateURLAccessRequest(url, std::move(callback));
}

}  // namespace

namespace supervised_user {

RemoteWebApprovalsManager::RemoteWebApprovalsManager() = default;

RemoteWebApprovalsManager::~RemoteWebApprovalsManager() = default;
void RemoteWebApprovalsManager::RequestApproval(
    const GURL& url,
    const UrlFormatter& url_formatter,
    ApprovalRequestInitiatedCallback callback) {
  GURL target_url = url_formatter.FormatUrl(url);

  AddApprovalRequestInternal(
      base::BindRepeating(CreateURLAccessRequest, target_url),
      std::move(callback), 0);
}

bool RemoteWebApprovalsManager::AreApprovalRequestsEnabled() const {
  return FindEnabledRemoteApprovalRequestCreator(0) <
         approval_request_creators_.size();
}

void RemoteWebApprovalsManager::AddApprovalRequestCreator(
    std::unique_ptr<PermissionRequestCreator> creator) {
  approval_request_creators_.push_back(std::move(creator));
}

void RemoteWebApprovalsManager::ClearApprovalRequestsCreators() {
  approval_request_creators_.clear();
}

size_t RemoteWebApprovalsManager::FindEnabledRemoteApprovalRequestCreator(
    size_t start) const {
  for (size_t i = start; i < approval_request_creators_.size(); ++i) {
    if (approval_request_creators_[i]->IsEnabled()) {
      return i;
    }
  }
  return approval_request_creators_.size();
}

void RemoteWebApprovalsManager::AddApprovalRequestInternal(
    const CreateApprovalRequestCallback& create_request,
    ApprovalRequestInitiatedCallback callback,
    size_t index) {
  size_t next_index = FindEnabledRemoteApprovalRequestCreator(index);
  if (next_index >= approval_request_creators_.size()) {
    std::move(callback).Run(false);
    return;
  }

  create_request.Run(
      approval_request_creators_[next_index].get(),
      base::BindOnce(&RemoteWebApprovalsManager::OnApprovalRequestIssued,
                     weak_ptr_factory_.GetWeakPtr(), create_request,
                     std::move(callback), next_index));
}

void RemoteWebApprovalsManager::OnApprovalRequestIssued(
    const CreateApprovalRequestCallback& create_request,
    ApprovalRequestInitiatedCallback callback,
    size_t index,
    bool success) {
  if (success) {
    std::move(callback).Run(true);
    return;
  }

  AddApprovalRequestInternal(create_request, std::move(callback), index + 1);
}

}  // namespace supervised_user
