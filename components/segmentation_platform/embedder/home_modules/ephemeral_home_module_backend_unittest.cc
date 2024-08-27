// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"

#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class EphemeralHomeModuleBackendTest : public DefaultModelTestBase {
 public:
  EphemeralHomeModuleBackendTest()
      : DefaultModelTestBase(
            std::make_unique<EphemeralHomeModuleBackend>(nullptr)) {
    feature_list_.InitWithFeatures({commerce::kPriceTrackingPromo}, {});
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
    registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);
    static_cast<EphemeralHomeModuleBackend*>(model_.get())
        ->set_home_modules_card_registry_for_testing(registry_.get());
  }
  ~EphemeralHomeModuleBackendTest() override = default;

 protected:
  // TODO(ssid): Maybe use mock class here;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<HomeModulesCardRegistry> registry_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(EphemeralHomeModuleBackendTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(EphemeralHomeModuleBackendTest, ExecuteModelWithInput) {
#if BUILDFLAG(IS_IOS)
  ExpectExecutionWithInput(
      {0, 0, 0}, /*expected_error=*/false,
      /*expected_result=*/
      {EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kNotShown)});
#else
  ExpectExecutionWithInput(/*inputs=*/{}, /*expected_error=*/false,
                           /*expected_result=*/{});
#endif
}

}  // namespace segmentation_platform::home_modules
