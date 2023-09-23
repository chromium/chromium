// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"

TabOrganizationResponse::Organization::Organization(
    std::u16string label_,
    std::vector<TabData::TabID> tabs_)
    : label(label_), tabs(tabs_) {}
TabOrganizationResponse::Organization::Organization(
    const Organization& organization) = default;
TabOrganizationResponse::Organization::~Organization() = default;

TabOrganizationResponse::TabOrganizationResponse(
    std::vector<TabOrganizationResponse::Organization> organizations_)
    : organizations(organizations_) {}
TabOrganizationResponse::~TabOrganizationResponse() = default;

TabOrganizationRequest::TabOrganizationRequest(
    BackendStartRequest backend_start_request_lambda,
    BackendCancelRequest backend_cancel_request_lambda)
    : backend_start_request_lambda_(std::move(backend_start_request_lambda)),
      backend_cancel_request_lambda_(std::move(backend_cancel_request_lambda)) {
}

TabOrganizationRequest::~TabOrganizationRequest() {
  if (state_ == State::STARTED) {
    CancelRequest();
  }
}

void TabOrganizationRequest::SetResponseCallback(OnResponseCallback callback) {
  response_callback_ = std::move(callback);
}

void TabOrganizationRequest::StartRequest() {
  CHECK(state_ == State::NOT_STARTED);
  state_ = State::STARTED;

  std::move(backend_start_request_lambda_).Run(this);
}

void TabOrganizationRequest::CompleteRequest(
    std::unique_ptr<TabOrganizationResponse> response) {
  // Ignore cancelled state.
  if (state_ == State::CANCELED) {
    return;
  }
  CHECK(state_ == State::STARTED);

  state_ = State::COMPLETED;
  response_ = std::move(response);

  if (response_callback_) {
    std::move(response_callback_).Run(response_.get());
  }
}

void TabOrganizationRequest::FailRequest() {
  CHECK(state_ != State::COMPLETED);
  state_ = State::FAILED;
}

void TabOrganizationRequest::CancelRequest() {
  CHECK(state_ == State::STARTED);
  CHECK(backend_cancel_request_lambda_);
  state_ = State::CANCELED;

  std::move(backend_cancel_request_lambda_).Run(this);
}
