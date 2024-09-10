// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/permission_request_creator_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"

namespace supervised_user {

namespace {
void DispatchResult(bool success,
                    PermissionRequestCreator::SuccessCallback callback) {
  std::move(callback).Run(success);
}

// TODO(b/4598236): Remove the validation and handle invalid responses
// in proto_fetcher (using a DataError status).
bool ValidateResponse(
    const kidsmanagement::CreatePermissionRequestResponse& response) {
  if (!response.has_permission_request()) {
    DLOG(WARNING) << "Permission request not found";
    return false;
  }
  if (!response.permission_request().has_id()) {
    DLOG(WARNING) << "ID not found";
    return false;
  }
  return true;
}

void OnSuccess(const kidsmanagement::CreatePermissionRequestResponse& response,
               PermissionRequestCreator::SuccessCallback callback) {
  DispatchResult(ValidateResponse(response), std::move(callback));
}

void OnFailure(const ProtoFetcherStatus& error,
               PermissionRequestCreator::SuccessCallback callback) {
  DispatchResult(false, std::move(callback));
}

void OnResponse(
    PermissionRequestCreator::SuccessCallback callback,
    const ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::CreatePermissionRequestResponse> response) {
  if (!status.IsOk()) {
    OnFailure(status, std::move(callback));
    return;
  }
  OnSuccess(*response, std::move(callback));
}

// Flips order of arguments so that the unbound arguments will be the
// request and callback
std::unique_ptr<PermissionRequestFetcher> FetcherFactory(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::PermissionRequest& request,
    PermissionRequestFetcher::Callback callback) {
  return CreatePermissionRequestFetcher(*identity_manager, url_loader_factory,
                                        request, std::move(callback),
                                        kCreatePermissionRequestConfig);
}

}  // namespace

PermissionRequestCreatorImpl::PermissionRequestCreatorImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : fetch_manager_(base::BindRepeating(&FetcherFactory,
                                         identity_manager,
                                         url_loader_factory)) {}

PermissionRequestCreatorImpl::~PermissionRequestCreatorImpl() = default;

bool PermissionRequestCreatorImpl::IsEnabled() const {
  return true;
}

void PermissionRequestCreatorImpl::CreateURLAccessRequest(
    const GURL& url_requested,
    SuccessCallback callback) {
  kidsmanagement::PermissionRequest request =
      kidsmanagement::PermissionRequest();
  request.mutable_object_ref()->assign(url_requested.spec());
  request.set_state(kidsmanagement::PermissionRequestState::PENDING);
  request.set_event_type(
      kidsmanagement::FamilyEventType::PERMISSION_CHROME_URL);

  fetch_manager_.Fetch(request,
                       base::BindOnce(&OnResponse, std::move(callback)));
}

}  // namespace supervised_user
