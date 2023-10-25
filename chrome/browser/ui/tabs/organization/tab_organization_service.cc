// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"

#include <memory>

#include "base/containers/contains.h"
#include "chrome/browser/ui/tabs/organization/request_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"

TabOrganizationService::TabOrganizationService(
    content::BrowserContext* browser_context)
    : trigger_observer_(
          base::BindRepeating(&TabOrganizationService::OnTriggerOccured,
                              base::Unretained(this)),
          browser_context) {}
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

  browser_session_map_.emplace(
      browser, TabOrganizationSession::CreateSessionForBrowser(browser, this));

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnToggleActionUIState(browser, true);
  }
}

void TabOrganizationService::OnStartRequest(
    TabOrganizationSession::ID session_id) const {
  const Browser* browser = nullptr;
  for (const auto& it : browser_session_map_) {
    if (it.second->session_id() == session_id) {
      browser = it.first;
      break;
    }
  }
  if (!browser) {
    return;
  }
  for (TabOrganizationObserver& observer : observers_) {
    observer.OnStartRequest(browser);
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
          browser,
          TabOrganizationSession::CreateSessionForBrowser(browser, this));

  return pair.first->second.get();
}

void TabOrganizationService::StartRequest(const Browser* browser) {
  TabOrganizationSession* session = GetSessionForBrowser(browser);
  if (!session) {
    session = CreateSessionForBrowser(browser);
  }
  if (session->request()->state() ==
      TabOrganizationRequest::State::NOT_STARTED) {
    session->StartRequest();
  }
}
