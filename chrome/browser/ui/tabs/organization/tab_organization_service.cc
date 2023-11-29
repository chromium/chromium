// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"

#include <memory>

#include "base/containers/contains.h"
#include "chrome/browser/ui/tabs/organization/request_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"

TabOrganizationService::TabOrganizationService(
    content::BrowserContext* browser_context) {
  auto trigger_backoff =
      std::make_unique<ProfilePrefBackoffLevelProvider>(browser_context);
  trigger_backoff_ = trigger_backoff.get();
  trigger_observer_ = std::make_unique<TabOrganizationTriggerObserver>(
      base::BindRepeating(&TabOrganizationService::OnTriggerOccured,
                          base::Unretained(this)),
      browser_context, MakeMVPTrigger(std::move(trigger_backoff)));
}
TabOrganizationService::~TabOrganizationService() = default;

void TabOrganizationService::OnTriggerOccured(const Browser* browser) {
  if (base::Contains(browser_session_map_, browser)) {
    // If the organizations havent been fully accepted or rejected, then it does
    // not need to be reset.
    if (!GetSessionForBrowser(browser)->IsComplete()) {
      return;
    } else {
      browser_session_map_.erase(browser);
    }
  }
  CreateSessionForBrowser(browser);

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnToggleActionUIState(browser, true);
  }
}

const TabOrganizationSession* TabOrganizationService::GetSessionForBrowser(
    const Browser* browser) const {
  if (base::Contains(browser_session_map_, browser)) {
    return browser_session_map_.at(browser).get();
  }
  return nullptr;
}

TabOrganizationSession* TabOrganizationService::GetSessionForBrowser(
    const Browser* browser) {
  if (base::Contains(browser_session_map_, browser)) {
    return browser_session_map_.at(browser).get();
  }
  return nullptr;
}

TabOrganizationSession* TabOrganizationService::CreateSessionForBrowser(
    const Browser* browser) {
  CHECK(!base::Contains(browser_session_map_, browser));

  std::pair<BrowserSessionMap::iterator, bool> pair =
      browser_session_map_.emplace(
          browser, TabOrganizationSession::CreateSessionForBrowser(browser));

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnSessionCreated(browser, pair.first->second.get());
  }

  return pair.first->second.get();
}

TabOrganizationSession* TabOrganizationService::ResetSessionForBrowser(
    const Browser* browser) {
  if (base::Contains(browser_session_map_, browser)) {
    browser_session_map_.erase(browser);
  }

  return CreateSessionForBrowser(browser);
}

void TabOrganizationService::StartRequest(const Browser* browser) {
  TabOrganizationSession* session = GetSessionForBrowser(browser);
  if (!session || session->IsComplete()) {
    session = ResetSessionForBrowser(browser);
  }
  if (session->request()->state() ==
      TabOrganizationRequest::State::NOT_STARTED) {
    session->StartRequest();
  }
  for (TabOrganizationObserver& observer : observers_) {
    observer.OnUserInvokedFeature(browser);
  }
}

void TabOrganizationService::AcceptTabOrganization(
    Browser* browser,
    TabOrganization::ID session_id,
    TabOrganization::ID organization_id) {
  TabOrganizationSession* session = GetSessionForBrowser(browser);
  if (!session || session->session_id() != session_id) {
    return;
  }

  TabOrganization* organization = nullptr;
  for (const std::unique_ptr<TabOrganization>& maybe_organization :
       session->tab_organizations()) {
    if (maybe_organization->organization_id() == organization_id) {
      organization = maybe_organization.get();
      break;
    }
  }

  if (!organization) {
    return;
  }

  organization->Accept();

  // if the session is completed, then destroy it.
  if (session->IsComplete()) {
    browser_session_map_.erase(browser);
  }
}

void TabOrganizationService::OnActionUIAccepted(const Browser* browser) {
  StartRequest(browser);
  trigger_backoff_->Decrement();
}

void TabOrganizationService::OnActionUIDismissed(const Browser* browser) {
  trigger_backoff_->Increment();
  browser_session_map_.erase(browser);
}
