// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"

GroupData::GroupData(tab_groups::TabGroupId id_,
                     std::u16string label_,
                     std::vector<std::unique_ptr<TabData>> tabs_)
    : id(id_), label(label_), tabs(std::move(tabs_)) {}

GroupData::~GroupData() = default;

TabOrganizationResponse::Organization::Organization(
    std::u16string label_,
    std::vector<TabData::TabID> tab_ids_,
    std::optional<tab_groups::TabGroupId> group_id_,
    std::optional<TabOrganization::ID> organization_id_)
    : label(label_),
      tab_ids(std::move(tab_ids_)),
      group_id(group_id_),
      organization_id(organization_id_) {}

TabOrganizationResponse::Organization::Organization(
    const Organization& organization) = default;

TabOrganizationResponse::Organization::Organization(
    Organization&& organization) = default;

TabOrganizationResponse::Organization::~Organization() = default;

TabOrganizationResponse::TabOrganizationResponse(
    std::vector<TabOrganizationResponse::Organization> organizations_,
    std::u16string feedback_id_,
    LogResultsCallback log_results_callback_)
    : organizations(organizations_),
      feedback_id(feedback_id_),
      log_results_callback(std::move(log_results_callback_)) {}

TabOrganizationResponse::~TabOrganizationResponse() = default;

int TabOrganizationResponse::GetTabCount() {
  int count = 0;
  for (const auto& organization : organizations) {
    count += organization.tab_ids.size();
  }
  return count;
}

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

TabData* TabOrganizationRequest::AddTabData(std::unique_ptr<TabData> tab_data) {
  TabData* tab_data_ptr = tab_data.get();
  tab_datas_.emplace_back(std::move(tab_data));
  return tab_data_ptr;
}

void TabOrganizationRequest::AddGroupData(
    tab_groups::TabGroupId id,
    std::u16string label,
    std::vector<std::unique_ptr<TabData>> tabs) {
  group_datas_.emplace_back(
      std::make_unique<GroupData>(id, label, std::move(tabs)));
}

void TabOrganizationRequest::StartRequest() {
  CHECK(state_ == State::NOT_STARTED);
  state_ = State::STARTED;
  request_start_time_ = base::Time::Now();

  std::move(backend_start_request_lambda_)
      .Run(this,
           base::BindOnce(&TabOrganizationRequest::CompleteRequest,
                          weak_ptr_factory_.GetWeakPtr()),
           base::BindOnce(&TabOrganizationRequest::FailRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TabOrganizationRequest::CompleteRequest(
    std::unique_ptr<TabOrganizationResponse> response) {
  // Ignore non-started states.
  if (state_ != State::STARTED) {
    return;
  }

  request_end_time_ = base::Time::Now();
  state_ = State::COMPLETED;
  response_ = std::move(response);
  if (response_callback_) {
    std::move(response_callback_).Run(response_.get());
  }
}

void TabOrganizationRequest::FailRequest() {
  CHECK(state_ != State::COMPLETED);
  state_ = State::FAILED;
  if (response_callback_) {
    std::move(response_callback_).Run(response_.get());
  }
}

void TabOrganizationRequest::CancelRequest() {
  CHECK(state_ == State::STARTED);
  CHECK(backend_cancel_request_lambda_);

  std::move(backend_cancel_request_lambda_).Run(this);
  if (response_callback_) {
    std::move(response_callback_).Run(response_.get());
  }
  state_ = State::CANCELED;
}

void TabOrganizationRequest::LogResults(const TabOrganizationSession* session) {
  // Log metrics about the response.
  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Response.Succeeded",
                        state_ == State::COMPLETED);
  if (!response_ || state_ != State::COMPLETED) {
    return;
  }

  UMA_HISTOGRAM_COUNTS_1000("Tab.Organization.Response.TabCount",
                            response_->GetTabCount());

  if (response_->log_results_callback) {
    std::move(response_->log_results_callback).Run(session);
  }

  CHECK(request_start_time_.has_value() && request_end_time_.has_value());
  UMA_HISTOGRAM_TIMES("Tab.Organization.Response.Latency",
                      request_end_time_.value() - request_start_time_.value());
}
