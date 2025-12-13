// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/password_manager_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

const char kPasswordManagerUserNegativeLabel[] = "Not_PasswordManagerUser";

using Feature = PasswordManagerUserModel::Feature;

class PasswordManagerUserModelTest : public DefaultModelTestBase {
 public:
  PasswordManagerUserModelTest()
      : DefaultModelTestBase(std::make_unique<PasswordManagerUserModel>()) {}
  ~PasswordManagerUserModelTest() override = default;
};

TEST_F(PasswordManagerUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(PasswordManagerUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  // PasswordManager user.
  ModelProvider::Request input1(Feature::kFeatureCount, 0);
  input1[Feature::kFeaturePasswordManagerReferrer] = 2;
  ExpectClassifierResults(input1, {kPasswordManagerUserUmaName});

  ModelProvider::Request input2(Feature::kFeatureCount, 0);
  input2[Feature::kFeatureStoredPasswordCount] = 7;
  ExpectClassifierResults(input2, {kPasswordManagerUserUmaName});

  ModelProvider::Request input3(Feature::kFeatureCount, 0);
  input3[Feature::kFeatureAssistedLoginCount] = 1;
  ExpectClassifierResults(input3, {kPasswordManagerUserUmaName});

  ModelProvider::Request input4(Feature::kFeatureCount, 0);
  input4[Feature::kFeatureGeneratedPasswordCount] = 1;
  ExpectClassifierResults(input4, {kPasswordManagerUserUmaName});

  ModelProvider::Request input5(Feature::kFeatureCount, 0);
  input5[Feature::kFeaturePasswordUIAcceptedCount] = 2;
  input5[Feature::kFeaturePasswordUIDismissedCount] = 1;
  ExpectClassifierResults(input5, {kPasswordManagerUserUmaName});

  ModelProvider::Request input6(Feature::kFeatureCount, 0);
  input6[Feature::kFeatureIOSCredentialExtensionEnabled] = 1;
  ExpectClassifierResults(input6, {kPasswordManagerUserUmaName});

  // Not a PasswordManager user.
  ModelProvider::Request input7(Feature::kFeatureCount, 0);
  input7[Feature::kFeaturePasswordManagerReferrer] = 1;
  ExpectClassifierResults(input7, {kPasswordManagerUserNegativeLabel});

  ModelProvider::Request input8(Feature::kFeatureCount, 0);
  input8[Feature::kFeatureStoredPasswordCount] = 6;
  ExpectClassifierResults(input8, {kPasswordManagerUserNegativeLabel});

  ModelProvider::Request input9(Feature::kFeatureCount, 0);
  input9[Feature::kFeaturePasswordUIAcceptedCount] = 1;
  input9[Feature::kFeaturePasswordUIDismissedCount] = 1;
  ExpectClassifierResults(input9, {kPasswordManagerUserNegativeLabel});

  ModelProvider::Request input10(Feature::kFeatureCount, 0);
  ExpectClassifierResults(input10, {kPasswordManagerUserNegativeLabel});
}

}  // namespace segmentation_platform
