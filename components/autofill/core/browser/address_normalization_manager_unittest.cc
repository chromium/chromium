// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_normalization_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AddressNormalizationManagerTest : public testing::Test {
 protected:
  AddressNormalizationManagerTest() = default;

  void Initialize(const std::string& app_locale) {
    manager_ = std::make_unique<AddressNormalizationManager>(
        &address_normalizer_, app_locale);
  }

  void Finalize() {
    manager_->FinalizeWithCompletionCallback(
        base::BindOnce(&AddressNormalizationManagerTest::CompletionCallback,
                       base::Unretained(this)));
  }

  void CompletionCallback() { completion_callback_called_ = true; }

  TestAddressNormalizer address_normalizer_;
  std::unique_ptr<AddressNormalizationManager> manager_;
  bool completion_callback_called_ = false;
};

TEST_F(AddressNormalizationManagerTest, SynchronousResult) {
  Initialize("en-US");

  AutofillProfile profile_to_normalize(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  manager_->NormalizeAddressUntilFinalized(&profile_to_normalize);

  EXPECT_FALSE(completion_callback_called_);
  Finalize();
  EXPECT_TRUE(completion_callback_called_);
}

TEST_F(AddressNormalizationManagerTest, AsynchronousResult) {
  Initialize("en-US");
  address_normalizer_.DelayNormalization();

  AutofillProfile profile_to_normalize(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  manager_->NormalizeAddressUntilFinalized(&profile_to_normalize);

  EXPECT_FALSE(completion_callback_called_);
  Finalize();
  EXPECT_FALSE(completion_callback_called_);
  address_normalizer_.CompleteAddressNormalization();
  EXPECT_TRUE(completion_callback_called_);
}

}  // namespace autofill
