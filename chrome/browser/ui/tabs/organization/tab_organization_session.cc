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
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"

namespace {
int kNextSessionID = 1;
}  // anonymous namespace

TabOrganizationSession::TabOrganizationSession()
    : TabOrganizationSession(std::make_unique<TabOrganizationRequest>()) {}

TabOrganizationSession::TabOrganizationSession(
    std::unique_ptr<TabOrganizationRequest> request,
    TabOrganizationEntryPoint entrypoint,
    const tabs::TabModel* base_session_tab)
    : request_(std::move(request)),
      session_id_(kNextSessionID),
      entrypoint_(entrypoint),
      base_session_tab_(base_session_tab) {
  kNextSessionID++;
}

TabOrganizationSession::~TabOrganizationSession() {
  const int group_count = tab_organizations_.size();
  switch (entrypoint_) {
    case TabOrganizationEntryPoint::kProactive: {
      UMA_HISTOGRAM_COUNTS_100("Tab.Organization.Proactive.GroupCount",
                               group_count);
      break;
    }
    case TabOrganizationEntryPoint::kTabContextMenu: {
      UMA_HISTOGRAM_COUNTS_100("Tab.Organization.TabContextMenu.GroupCount",
                               group_count);
      break;
    }
    case TabOrganizationEntryPoint::kThreeDotMenu: {
      UMA_HISTOGRAM_COUNTS_100("Tab.Organization.ThreeDotMenu.GroupCount",
                               group_count);
      break;
    }
    case TabOrganizationEntryPoint::kTabSearch: {
      UMA_HISTOGRAM_COUNTS_100("Tab.Organization.TabSearch.GroupCount",
                               group_count);
      break;
    }
    case TabOrganizationEntryPoint::kNone: {
    }
  }
  UMA_HISTOGRAM_COUNTS_100("Tab.Organization.AllEntrypoints.GroupCount",
                           group_count);

  for (auto& organization : tab_organizations_) {
    organization->RemoveObserver(this);

    switch (entrypoint_) {
      case TabOrganizationEntryPoint::kProactive: {
        UMA_HISTOGRAM_ENUMERATION("Tab.Organization.Proactive.UserChoice",
                                  organization->choice());
        break;
      }
      case TabOrganizationEntryPoint::kTabContextMenu: {
        UMA_HISTOGRAM_ENUMERATION("Tab.Organization.TabContextMenu.UserChoice",
                                  organization->choice());
        break;
      }
      case TabOrganizationEntryPoint::kThreeDotMenu: {
        UMA_HISTOGRAM_ENUMERATION("Tab.Organization.ThreeDotMenu.UserChoice",
                                  organization->choice());
        break;
      }
      case TabOrganizationEntryPoint::kTabSearch: {
        UMA_HISTOGRAM_ENUMERATION("Tab.Organization.TabSearch.UserChoice",
                                  organization->choice());
        break;
      }
      case TabOrganizationEntryPoint::kNone: {
      }
    }

    UMA_HISTOGRAM_ENUMERATION("Tab.Organization.AllEntrypoints.UserChoice",
                              organization->choice());

    if (organization->choice() == TabOrganization::UserChoice::kAccepted) {
      UMA_HISTOGRAM_COUNTS_100("Tab.Organization.Organization.TabRemovedCount",
                               organization->user_removed_tab_ids().size());

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
    // The request may contain a callback which should happen before the
    // destructor goes out of scope
    request_.reset();
  }
}

// static
std::unique_ptr<TabOrganizationSession>
TabOrganizationSession::CreateSessionForBrowser(
    const Browser* browser,
    const TabOrganizationEntryPoint entrypoint,
    const tabs::TabModel* base_session_tab) {
  std::unique_ptr<TabOrganizationRequest> request =
      TabOrganizationRequestFactory::GetForProfile(browser->profile())
          ->CreateRequest(browser->profile());

  // iterate through the tabstripmodel building the tab data.
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  for (int index = 0; index < tab_strip_model->count(); index++) {
    tabs::TabModel* tab = tab_strip_model->GetTabAtIndex(index);
    std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);
    if (!tab_data->IsValidForOrganizing()) {
      continue;
    }

    if (base_session_tab && tab == base_session_tab) {
      request->SetBaseTabID(tab_data->tab_id());
    }

    request->AddTabData(std::move(tab_data));
  }

  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  for (tab_groups::TabGroupId group_id : tab_group_model->ListTabGroups()) {
    TabGroup* group = tab_group_model->GetTabGroup(group_id);
    std::u16string title = group->visual_data()->title();
    std::vector<std::unique_ptr<TabData>> tabs;
    const gfx::Range tab_indices = group->ListTabs();
    for (size_t index = tab_indices.start(); index < tab_indices.end();
         index++) {
      tabs.push_back(
          std::make_unique<TabData>(tab_strip_model->GetTabAtIndex(index)));
    }
    request->AddGroupData(group_id, title, std::move(tabs));
  }

  return std::make_unique<TabOrganizationSession>(std::move(request),
                                                  entrypoint, base_session_tab);
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

    // Add grouped tabs
    std::optional<tab_groups::TabGroupId> group_id;
    if (response_organization.group_id.has_value()) {
      TabStripModel* tab_strip_model =
          request()->tab_datas()[0]->original_tab_strip_model();

      // Replace group id with an id with a matching token, as determined by
      // string equality. This is required because the token has been
      // translated to a string and back as part of the TabOrganizationRequest,
      // and is no longer the same object as the token referenced by the
      // TabGroupId in the model.
      std::vector<tab_groups::TabGroupId> all_group_ids =
          tab_strip_model->group_model()->ListTabGroups();
      const auto matching_id_iter = std::find_if(
          all_group_ids.begin(), all_group_ids.end(),
          [&](const tab_groups::TabGroupId& id) {
            return response_organization.group_id.value().ToString() ==
                   id.ToString();
          });
      if (matching_id_iter == all_group_ids.end()) {
        // Do not include organizations which reference tab groups that no
        // longer exist.
        continue;
      }
      group_id = std::make_optional(*matching_id_iter);

      TabGroupModel* tab_group_model = tab_strip_model->group_model();
      TabGroup* group = tab_group_model->GetTabGroup(group_id.value());
      const gfx::Range tab_indices = group->ListTabs();
      for (size_t index = tab_indices.start(); index < tab_indices.end();
           index++) {
        tab_datas_for_org.emplace_back(
            std::make_unique<TabData>(tab_strip_model->GetTabAtIndex(index)));
      }
    }
    const int first_new_tab_index = tab_datas_for_org.size();

    // Add ungrouped tabs
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
          std::make_unique<TabData>((*matching_tab)->tab());
      tab_datas_for_org.emplace_back(std::move(tab_data_for_org));
    }

    std::vector<std::u16string> names;
    names.emplace_back(response_organization.label);

    std::unique_ptr<TabOrganization> organization =
        std::make_unique<TabOrganization>(std::move(tab_datas_for_org),
                                          std::move(names),
                                          first_new_tab_index);

    organization->SetTabGroupId(group_id);
    response_organization.organization_id = organization->organization_id();

    organization->AddObserver(this);
    tab_organizations_.emplace_back(std::move(organization));
  }
}
