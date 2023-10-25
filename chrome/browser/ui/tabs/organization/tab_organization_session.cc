// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/request_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"

namespace {
int kNextSessionID = 1;
}  // anonymous namespace

TabOrganizationSession::TabOrganizationSession()
    : TabOrganizationSession(nullptr,
                             std::make_unique<TabOrganizationRequest>()) {}

TabOrganizationSession::TabOrganizationSession(
    const TabOrganizationService* service,
    std::unique_ptr<TabOrganizationRequest> request)
    : service_(service),
      request_(std::move(request)),
      session_id_(kNextSessionID) {
  kNextSessionID++;
}

TabOrganizationSession::~TabOrganizationSession() = default;

// static
std::unique_ptr<TabOrganizationSession>
TabOrganizationSession::CreateSessionForBrowser(
    const Browser* browser,
    const TabOrganizationService* service) {
  std::unique_ptr<TabOrganizationRequest> request =
      TabOrganizationRequestFactory::Get()->CreateRequest(browser->profile());

  // iterate through the tabstripmodel building the tab data.
  std::vector<std::unique_ptr<TabData>> tab_datas;
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  for (int index = 0; index < tab_strip_model->count(); index++) {
    request->AddTabData(std::make_unique<TabData>(
        tab_strip_model, tab_strip_model->GetWebContentsAt(index)));
  }

  return std::make_unique<TabOrganizationSession>(service, std::move(request));
}

const TabOrganization* TabOrganizationSession::GetNextTabOrganization() const {
  for (const TabOrganization& tab_organization : tab_organizations_) {
    if (!tab_organization.choice().has_value()) {
      return &tab_organization;
    }
  }
  return nullptr;
}

TabOrganization* TabOrganizationSession::GetNextTabOrganization() {
  for (TabOrganization& tab_organization : tab_organizations_) {
    if (!tab_organization.choice().has_value()) {
      return &tab_organization;
    }
  }
  return nullptr;
}

bool TabOrganizationSession::IsComplete() const {
  // If the request isnt completed, then the Session isnt completed.
  if (request_->state() == TabOrganizationRequest::State::STARTED ||
      request_->state() == TabOrganizationRequest::State::NOT_STARTED) {
    return false;
  }

  // If there are still tab organizations that havent been acted on, then the
  // session is still not completed.
  return GetNextTabOrganization();
}

void TabOrganizationSession::StartRequest() {
  CHECK(request_);
  request_->SetResponseCallback(base::BindOnce(
      &TabOrganizationSession::PopulateAndCreate, base::Unretained(this)));
  request_->StartRequest();
  if (service_) {
    service_->OnStartRequest(session_id_);
  }
}

void TabOrganizationSession::PopulateAndCreate(
    const TabOrganizationResponse* response) {
  PopulateOrganizations(response);
  TabOrganization* organization = GetNextTabOrganization();
  if (organization->IsValidForOrganizing()) {
    organization->Accept();
  }
}

void TabOrganizationSession::PopulateOrganizations(
    const TabOrganizationResponse* response) {
  // for each of the organizations, make sure that the TabData is valid for
  // grouping.
  for (const TabOrganizationResponse::Organization& response_organization :
       response->organizations) {
    std::vector<std::unique_ptr<TabData>> tab_datas_for_org;

    for (const TabData::TabID& tab_id : response_organization.tab_ids) {
      // TODO for now we can't use the TabID directly, we instead need to use
      // the webcontents ptr to refer to the tab.
      const auto matching_tab = std::find_if(
          request()->tab_datas().begin(), request()->tab_datas().end(),
          [tab_id](const std::unique_ptr<TabData>& tab_data) {
            return tab_id == tab_data->tab_id();
          });

      // If the tab was removed or bad data was returned, do not include it in
      // the organization.
      if (matching_tab == request()->tab_datas().end()) {
        continue;
      }

      // If the tab is no longer valid, do not include it in the organization.
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
