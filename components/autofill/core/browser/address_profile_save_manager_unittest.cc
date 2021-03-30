// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profile_save_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
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

class AddressProfileSaveManagerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  MockPersonalDataManager mock_personal_data_manager_;
};

}  // namespace

TEST_F(AddressProfileSaveManagerTest, SaveProfileWhenNoSavePrompt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillAddressProfileSavePrompt);

  AddressProfileSaveManager save_manager(&autofill_client_,
                                         &mock_personal_data_manager_);
  AutofillProfile test_profile = test::GetFullProfile();
  EXPECT_CALL(mock_personal_data_manager_, SaveImportedProfile(test_profile));
  save_manager.SaveProfile(test_profile);
}
}  // namespace autofill
