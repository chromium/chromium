// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"

#include <memory>

#include "base/containers/contains.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"

TabOrganizationService::TabOrganizationService() = default;
TabOrganizationService::~TabOrganizationService() = default;

void TabOrganizationService::OnTriggerOccured(Browser* browser) {
  if (base::Contains(browser_session_map_, browser)) {
    return;
  }

  browser_session_map_.emplace(browser,
                               std::make_unique<TabOrganizationSession>());

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnToggleActionUIState(browser, true);
  }
}

const TabOrganizationSession* TabOrganizationService::GetSessionForBrowser(
    Browser* browser) {
  CHECK(base::Contains(browser_session_map_, browser));
  return browser_session_map_.at(browser).get();
}
