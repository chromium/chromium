// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/password_manager_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

const char kPasswordManagerUserNegativeLabel[] = "Not_PasswordManagerUser";

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
  ExpectClassifierResults(/*input=*/{2, 0, 0, 0, 0, 0, 0},
                          {kPasswordManagerUserUmaName});
  ExpectClassifierResults(/*input=*/{0, 7, 0, 0, 0, 0, 0},
                          {kPasswordManagerUserUmaName});
  ExpectClassifierResults(/*input=*/{0, 0, 1, 0, 0, 0, 0},
                          {kPasswordManagerUserUmaName});
  ExpectClassifierResults(/*input=*/{0, 0, 0, 1, 0, 0, 0},
                          {kPasswordManagerUserUmaName});
  ExpectClassifierResults(/*input=*/{0, 0, 0, 0, 2, 1, 0},
                          {kPasswordManagerUserUmaName});
  ExpectClassifierResults(/*input=*/{0, 0, 0, 0, 0, 0, 1},
                          {kPasswordManagerUserUmaName});

  // Not a PasswordManager user.
  ExpectClassifierResults(/*input=*/{1, 0, 0, 0, 0, 0, 0},
                          {kPasswordManagerUserNegativeLabel});
  ExpectClassifierResults(/*input=*/{0, 6, 0, 0, 0, 0, 0},
                          {kPasswordManagerUserNegativeLabel});
  ExpectClassifierResults(/*input=*/{0, 0, 0, 0, 1, 1, 0},
                          {kPasswordManagerUserNegativeLabel});
  ExpectClassifierResults(/*input=*/{0, 0, 0, 0, 0, 0, 0},
                          {kPasswordManagerUserNegativeLabel});
}

}  // namespace segmentation_platform
