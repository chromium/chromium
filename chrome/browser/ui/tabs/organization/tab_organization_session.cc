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
    : request_(std::move(request)) {}

TabOrganizationSession::~TabOrganizationSession() = default;

TabOrganization* TabOrganizationSession::GetNextTabOrganization() {
  for (TabOrganization& tab_organization : tab_organizations_) {
    if (!tab_organization.choice().has_value()) {
      return &tab_organization;
    }
  }
  return nullptr;
}

void TabOrganizationSession::StartRequest() {
  CHECK(request_);
  request_->SetResponseCallback(base::BindOnce(
      &TabOrganizationSession::PopulateOrganizations, base::Unretained(this)));
  request_->StartRequest();
}

void TabOrganizationSession::PopulateOrganizations(
    const TabOrganizationResponse* response) {
  // for each of the organizations, make sure that the TabData is valid for
  // grouping.
  for (const TabOrganizationResponse::Organization& response_organization :
       response->organizations) {
    std::vector<std::unique_ptr<TabData>> tab_datas_for_org;

    for (const TabData::TabID& tab_id : response_organization.tab_ids) {
      // TODO for now we cant use the TabID directly, we instead need to use
      // the webcontents ptr to refer to the tab.
      const auto matching_tab = std::find_if(
          request()->tab_datas().begin(), request()->tab_datas().end(),
          [tab_id](const std::unique_ptr<TabData>& tab_data) {
            return tab_id == tab_data->tab_id();
          });
      if (matching_tab == request()->tab_datas().end()) {
        continue;
      }
      if (!(*matching_tab)->IsValidForOrganizing()) {
        continue;
      }

      std::unique_ptr<TabData> tab_data_for_org =
          std::make_unique<TabData>((*matching_tab)->original_tab_strip_model(),
                                    (*matching_tab)->web_contents());
      tab_datas_for_org.emplace_back(std::move(tab_data_for_org));
    }

    TabOrganization tab_organization(std::move(tab_datas_for_org),
                                     {response_organization.label}, 0,
                                     absl::nullopt);
    tab_organizations_.emplace_back(std::move(tab_organization));
  }
}
