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
      bool is_off_the_record = false);
  ~MockAimEligibilityService() override;

  MOCK_METHOD(bool, IsServerEligibilityEnabled, (), (const, override));
  MOCK_METHOD(bool, IsAimLocallyEligible, (), (const, override));
  MOCK_METHOD(bool, IsAimEligible, (), (const, override));
  MOCK_METHOD(bool, IsDeepSearchEligible, (), (const, override));
  MOCK_METHOD(bool, IsCreateImagesEligible, (), (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterEligibilityChangedCallback,
              (base::RepeatingClosure),
              (override));
  MOCK_METHOD(std::string, GetCountryCode, (), (const, override));
  MOCK_METHOD(std::string, GetLocale, (), (const, override));
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOCK_AIM_ELIGIBILITY_SERVICE_H_
