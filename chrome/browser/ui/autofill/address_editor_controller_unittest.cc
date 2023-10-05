// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_editor_controller.h"

#include <memory>

#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

// TODO(crbug.com/1470459): write more unit tests for AddressEditorController.
class AddressEditorControllerTest : public testing::Test {
 public:
  AddressEditorControllerTest() = default;

  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillProfileEnabled, true);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillCreditCardEnabled, true);
    pdm_.SetPrefService(&pref_service_);
    pdm_.set_default_country_code("US");
  }

 protected:
  void CreateController(bool is_validatable) {
    controller_ = std::make_unique<AddressEditorController>(profile_, &pdm_,
                                                            is_validatable);
  }

  TestingPrefServiceSimple pref_service_;
  TestPersonalDataManager pdm_;
  AutofillProfile profile_ = test::GetFullProfile();
  std::unique_ptr<AddressEditorController> controller_;
};

TEST_F(AddressEditorControllerTest, SmokeTest) {
  CreateController(/*is_validatable=*/false);
  EXPECT_FALSE(controller_->get_is_validatable());
}

}  // namespace autofill
