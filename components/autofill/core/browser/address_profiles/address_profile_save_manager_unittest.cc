// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profiles/address_profile_save_manager.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
class MockPersonalDataManager : public TestPersonalDataManager {
 public:
  MockPersonalDataManager() = default;
  ~MockPersonalDataManager() override = default;
  MOCK_METHOD(std::string,
              SaveImportedProfile,
              (const AutofillProfile&),
              (override));
};

}  // namespace

TEST(AddressProfileSaveManager, SaveProfile) {
  MockPersonalDataManager pdm;
  AddressProfileSaveManager save_manager(&pdm);
  AutofillProfile test_profile = test::GetFullProfile();
  EXPECT_CALL(pdm, SaveImportedProfile(test_profile));
  save_manager.SaveProfile(test_profile);
}
}  // namespace autofill
