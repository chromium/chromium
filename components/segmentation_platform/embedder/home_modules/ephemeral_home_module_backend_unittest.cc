// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class EphemeralHomeModuleBackendTest : public DefaultModelTestBase {
 public:
  EphemeralHomeModuleBackendTest()
      : DefaultModelTestBase(
            std::make_unique<EphemeralHomeModuleBackend>(nullptr)),
        registry_(nullptr) {
    static_cast<EphemeralHomeModuleBackend*>(model_.get())
        ->set_home_modules_card_registry_for_testing(&registry_);
  }
  ~EphemeralHomeModuleBackendTest() override = default;

 protected:
  // TODO(ssid): Maybe use mock class here;
  HomeModulesCardRegistry registry_;
};

TEST_F(EphemeralHomeModuleBackendTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(EphemeralHomeModuleBackendTest, ExecuteModelWithInput) {
  ExpectExecutionWithInput(/*inputs=*/{}, /*expected_error=*/false,
                           /*expected_result=*/{});
}

}  // namespace segmentation_platform::home_modules
