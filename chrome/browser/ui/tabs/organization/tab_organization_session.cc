// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"

#include <vector>

#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "tab_organization_session.h"

TabOrganizationSession::TabOrganizationSession()
    : TabOrganizationSession(std::make_unique<TabOrganizationRequest>()) {}

TabOrganizationSession::TabOrganizationSession(
    std::unique_ptr<TabOrganizationRequest> request)
    : request_(std::move(request)), tab_organizations_({}) {}

TabOrganizationSession::~TabOrganizationSession() = default;

void TabOrganizationSession::StartRequest() {
  CHECK(request_);
  request_->SetResponseCallback(base::BindOnce(
      &TabOrganizationSession::PopulateOrganizations, base::Unretained(this)));
  request_->StartRequest();
}

void TabOrganizationSession::PopulateOrganizations(
    const TabOrganizationResponse* response) {}
