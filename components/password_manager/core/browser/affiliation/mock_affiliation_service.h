// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_MOCK_AFFILIATION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_MOCK_AFFILIATION_SERVICE_H_

#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockAffiliationService : public AffiliationService {
 public:
  // This struct mirrors the corresponding affiliation and branding information
  // related fields from PasswordForm.
  struct AffiliationAndBrandingInformation {
    std::string affiliated_web_realm;
    std::string app_display_name;
    GURL app_icon_url;
  };

  MockAffiliationService();
  ~MockAffiliationService() override;

  MOCK_METHOD(void,
              PrefetchChangePasswordURLs,
              (const std::vector<GURL>&, base::OnceClosure),
              (override));
  MOCK_METHOD(void, Clear, (), (override));
  MOCK_METHOD(GURL, GetChangePasswordURL, (const GURL&), (override, const));
  MOCK_METHOD(void,
              GetAffiliationsAndBranding,
              (const FacetURI&,
               AffiliationService::StrategyOnCacheMiss,
               AffiliationService::ResultCallback),
              (override));
  MOCK_METHOD(void, Prefetch, (const FacetURI&, const base::Time&), (override));
  MOCK_METHOD(void,
              CancelPrefetch,
              (const FacetURI&, const base::Time&),
              (override));
  MOCK_METHOD(void, KeepPrefetchForFacets, (std::vector<FacetURI>), (override));
  MOCK_METHOD(void, TrimCacheForFacetURI, (const FacetURI&), (override));
  MOCK_METHOD(void, TrimUnusedCache, (std::vector<FacetURI>), (override));
  MOCK_METHOD(void, GetAllGroups, (GroupsCallback), (override, const));
  MOCK_METHOD(void,
              GetPSLExtensions,
              (base::OnceCallback<void(std::vector<std::string>)>),
              (override, const));
  MOCK_METHOD(void,
              UpdateAffiliationsAndBranding,
              (const std::vector<FacetURI>&, base::OnceClosure),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_MOCK_AFFILIATION_SERVICE_H_
