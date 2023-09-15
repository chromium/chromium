// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class TabOrganizationSession {
 public:
  TabOrganizationSession();
  explicit TabOrganizationSession(
      std::unique_ptr<TabOrganizationRequest> request);
  ~TabOrganizationSession();

  const TabOrganizationRequest* request() const { return request_.get(); }
  const std::vector<TabOrganization>& tab_organizations() const {
    return tab_organizations_;
  }

  void StartRequest();

 private:
  void PopulateOrganizations(const TabOrganizationResponse* response);

  std::unique_ptr<TabOrganizationRequest> request_;
  std::vector<TabOrganization> tab_organizations_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SESSION_H_
