// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOCK_AIM_ELIGIBILITY_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOCK_AIM_ELIGIBILITY_SERVICE_H_

#include "components/omnibox/browser/aim_eligibility_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockAimEligibilityService : public AimEligibilityService {
 public:
  MockAimEligibilityService(
      PrefService& pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      Configuration configuration = {});
  ~MockAimEligibilityService() override;

  MOCK_METHOD(bool, IsServerEligibilityEnabled, (), (const, override));
  MOCK_METHOD(bool, IsAimLocallyEligible, (), (const, override));
  MOCK_METHOD(bool, IsAimEligible, (), (const, override));
  MOCK_METHOD(bool, IsCanvasEligible, (), (const, override));
  MOCK_METHOD(bool, IsCobrowseEligible, (), (const, override));
  MOCK_METHOD(bool, IsDeepSearchEligible, (), (const, override));
  MOCK_METHOD(bool, IsCreateImagesEligible, (), (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterEligibilityChangedCallback,
              (base::RepeatingClosure),
              (override));
  MOCK_METHOD(std::string, GetCountryCode, (), (const, override));
  MOCK_METHOD(std::string, GetLocale, (), (const, override));
  MOCK_METHOD(bool, HasAimUrlParams, (const GURL& url), (const, override));
  MOCK_METHOD(const omnibox::AimEligibilityResponse&,
              GetMostRecentResponse,
              (),
              (const, override));
  MOCK_METHOD(void, FetchEligibility, (RequestSource), (override));
  MOCK_METHOD(const omnibox::SearchboxConfig*,
              GetSearchboxConfig,
              (),
              (const, override));

  omnibox::SearchboxConfig& config() { return mock_config; }

 private:
  // Mock searchbox config object.
  mutable omnibox::SearchboxConfig mock_config;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOCK_AIM_ELIGIBILITY_SERVICE_H_
