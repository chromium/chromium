// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/request_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"

namespace {
int kNextSessionID = 1;
}  // anonymous namespace

TabOrganizationSession::TabOrganizationSession()
    : TabOrganizationSession(std::make_unique<TabOrganizationRequest>()) {}

TabOrganizationSession::TabOrganizationSession(
    std::unique_ptr<TabOrganizationRequest> request,
    TabOrganizationEntryPoint entrypoint)
    : request_(std::move(request)),
      session_id_(kNextSessionID),
      entrypoint_(entrypoint) {
  kNextSessionID++;
}

TabOrganizationSession::~TabOrganizationSession() {
  for (auto& organization : tab_organizations_) {
    organization->RemoveObserver(this);

    switch (entrypoint_) {
      case TabOrganizationEntryPoint::PROACTIVE: {
        UMA_HISTOGRAM_ENUMERATION("Tab.Organization.Proactive.UserChoice",
                                  organization->choice());
        break;
      }
      case TabOrganizationEntryPoint::TAB_CONTEXT_MENU: {
        UMA_HISTOGRAM_ENUMERATION("Tab.Organization.TabContextMenu.UserChoice",
                                  organization->choice());
        break;
      }
      case TabOrganizationEntryPoint::THREE_DOT_MENU: {
        UMA_HISTOGRAM_ENUMERATION("Tab.Organization.ThreeDotMenu.UserChoice",
                                  organization->choice());
        break;
      }

      case TabOrganizationEntryPoint::NONE: {
      }
    }

    UMA_HISTOGRAM_ENUMERATION("Tab.Organization.AllEntrypoints.UserChoice",
                              organization->choice());

    if (organization->choice() == TabOrganization::UserChoice::kAccepted) {
      UMA_HISTOGRAM_COUNTS_100("Tab.Organization.Organization.TabRemovedCount",
                               organization->GetTabRemovedCount());

      UMA_HISTOGRAM_BOOLEAN(
          "Tab.Organization.Organization.LabelEdited",
          organization->names()[0] != organization->GetDisplayName());
    }
  }

  for (auto& observer : observers_) {
    observer.OnTabOrganizationSessionDestroyed(session_id());
  }

  if (request_) {
    request_->LogResults(this);
  }
}

// static
std::unique_ptr<TabOrganizationSession>
TabOrganizationSession::CreateSessionForBrowser(
    const Browser* browser,
    const content::WebContents* base_session_webcontents) {
  std::unique_ptr<TabOrganizationRequest> request =
      TabOrganizationRequestFactory::GetForProfile(browser->profile())
          ->CreateRequest(browser->profile());

  // iterate through the tabstripmodel building the tab data.
  std::vector<std::unique_ptr<TabData>> tab_datas;
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  for (int index = 0; index < tab_strip_model->count(); index++) {
    content::WebContents* web_contents =
        tab_strip_model->GetWebContentsAt(index);
    std::unique_ptr<TabData> tab_data =
        std::make_unique<TabData>(tab_strip_model, web_contents);
    if (!tab_data->IsValidForOrganizing()) {
      continue;
    }

    if (base_session_webcontents && web_contents == base_session_webcontents) {
      request->SetBaseTabID(tab_data->tab_id());
    }

    request->AddTabData(std::move(tab_data));
  }

  return std::make_unique<TabOrganizationSession>(std::move(request));
}

const TabOrganization* TabOrganizationSession::GetNextTabOrganization() const {
  for (auto& tab_organization : tab_organizations_) {
    if (tab_organization->IsValidForOrganizing() &&
        tab_organization->choice() == TabOrganization::UserChoice::kNoChoice) {
      return tab_organization.get();
    }
  }
  return nullptr;
}

TabOrganization* TabOrganizationSession::GetNextTabOrganization() {
  for (auto& tab_organization : tab_organizations_) {
    if (tab_organization->IsValidForOrganizing() &&
        tab_organization->choice() == TabOrganization::UserChoice::kNoChoice) {
      return tab_organization.get();
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
  return !(GetNextTabOrganization());
}

void TabOrganizationSession::AddObserver(
    TabOrganizationSession::Observer* observer) {
  observers_.AddObserver(observer);
}

void TabOrganizationSession::RemoveObserver(
    TabOrganizationSession::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TabOrganizationSession::OnTabOrganizationUpdated(
    const TabOrganization* organization) {
  NotifyObserversOfUpdate();
}

void TabOrganizationSession::OnTabOrganizationDestroyed(
    TabOrganization::ID organization_id) {
  NotifyObserversOfUpdate();
}

void TabOrganizationSession::StartRequest() {
  CHECK(request_);
  request_->SetResponseCallback(base::BindOnce(
      &TabOrganizationSession::OnRequestResponse, base::Unretained(this)));
  request_->StartRequest();
  NotifyObserversOfUpdate();
}

void TabOrganizationSession::NotifyObserversOfUpdate() {
  for (auto& observer : observers_) {
    observer.OnTabOrganizationSessionUpdated(this);
  }
}

void TabOrganizationSession::OnRequestResponse(
    TabOrganizationResponse* response) {
  if (response) {
    PopulateOrganizations(response);
  }
  NotifyObserversOfUpdate();
}

void TabOrganizationSession::PopulateAndCreate(
    TabOrganizationResponse* response) {
  PopulateOrganizations(response);
  TabOrganization* organization = GetNextTabOrganization();
  if (organization->IsValidForOrganizing()) {
    organization->Accept();
  }
}

void TabOrganizationSession::PopulateOrganizations(
    TabOrganizationResponse* response) {
  feedback_id_ = response->feedback_id;
  // for each of the organizations, make sure that the TabData is valid for
  // grouping.
  for (TabOrganizationResponse::Organization& response_organization :
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

      // Reconstruct the tab data in for the organization.
      std::unique_ptr<TabData> tab_data_for_org =
          std::make_unique<TabData>((*matching_tab)->original_tab_strip_model(),
                                    (*matching_tab)->web_contents());
      tab_datas_for_org.emplace_back(std::move(tab_data_for_org));
    }

    std::vector<std::u16string> names;
    names.emplace_back(response_organization.label);

    std::unique_ptr<TabOrganization> organization =
        std::make_unique<TabOrganization>(std::move(tab_datas_for_org),
                                          std::move(names));

    response_organization.organization_id = organization->organization_id();

    organization->AddObserver(this);
    tab_organizations_.emplace_back(std::move(organization));
  }
}
