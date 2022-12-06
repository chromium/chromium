// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"

namespace password_manager {

MockAffiliationService::MockAffiliationService() = default;

MockAffiliationService::~MockAffiliationService() = default;

void MockAffiliationService::
    ExpectCallToInjectAffiliationAndBrandingInformation(
        const std::vector<AffiliationAndBrandingInformation>&
            results_to_inject) {
  EXPECT_CALL(*this, OnInjectAffiliationAndBrandingInformationCalled())
      .WillOnce(testing::Return(results_to_inject));
}

void MockAffiliationService::InjectAffiliationAndBrandingInformation(
    std::vector<std::unique_ptr<PasswordForm>> forms,
    AffiliationService::StrategyOnCacheMiss strategy_on_cache_miss,
    PasswordFormsOrErrorCallback result_callback) {
  const std::vector<AffiliationAndBrandingInformation>& information =
      OnInjectAffiliationAndBrandingInformationCalled();
  if (information.empty()) {
    std::move(result_callback).Run(std::move(forms));
    return;
  }

  ASSERT_EQ(information.size(), forms.size());
  for (size_t i = 0; i < forms.size(); ++i) {
    forms[i]->affiliated_web_realm = information[i].affiliated_web_realm;
    forms[i]->app_display_name = information[i].app_display_name;
    forms[i]->app_icon_url = information[i].app_icon_url;
  }
  std::move(result_callback).Run(std::move(forms));
}

}  // namespace password_manager
