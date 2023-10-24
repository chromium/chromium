// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Browser;
class TabOrganizationService;

class TabOrganizationSession {
 public:
  // TODO(dpenning): make this a base::Token.
  using ID = int;

  TabOrganizationSession();
  explicit TabOrganizationSession(
      const TabOrganizationService* service,
      std::unique_ptr<TabOrganizationRequest> request);
  ~TabOrganizationSession();

  const TabOrganizationRequest* request() const { return request_.get(); }
  const std::vector<TabOrganization>& tab_organizations() const {
    return tab_organizations_;
  }
  ID session_id() const { return session_id_; }

  static std::unique_ptr<TabOrganizationSession> CreateSessionForBrowser(
      const Browser* browser,
      const TabOrganizationService* service);

  const TabOrganization* GetNextTabOrganization() const;
  TabOrganization* GetNextTabOrganization();

  void StartRequest();

  void AddOrganizationForTesting(TabOrganization tab_organization) {
    tab_organizations_.emplace_back(std::move(tab_organization));
  }

  // Returns true if the request is not completed or there are still actions
  // that need to be taken on organizations.
  bool IsComplete() const;

 private:
  // TODO: Remove once the full UI flow is implemented.
  void PopulateAndCreate(const TabOrganizationResponse* response);

  // Fills in the organizations from the request. Called when the request
  // completes.
  void PopulateOrganizations(const TabOrganizationResponse* response);

  raw_ptr<const TabOrganizationService> service_;
  std::unique_ptr<TabOrganizationRequest> request_;
  std::vector<TabOrganization> tab_organizations_;
  ID session_id_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
