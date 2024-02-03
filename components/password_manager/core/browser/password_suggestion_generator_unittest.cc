// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_suggestion_generator.h"

#include <vector>

#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace password_manager {

using autofill::PasswordFormFillData;
using autofill::Suggestion;

// TODO(b/323316649): Write more tests for password suggestion generation.
class PasswordSuggestionGeneratorTest : public testing::Test {
 public:
  PasswordSuggestionGeneratorTest() : generator_(&driver(), &client()) {}

  const gfx::Image& favicon() const { return favicon_; }

  StubPasswordManagerDriver& driver() { return driver_; }

  StubPasswordManagerClient& client() { return client_; }

  PasswordSuggestionGenerator& generator() { return generator_; }

 private:
  gfx::Image favicon_;

  StubPasswordManagerClient client_;
  StubPasswordManagerDriver driver_;
  PasswordSuggestionGenerator generator_;
};

// Test that no suggestions are generated from an empty `PasswordFormFillData`.
TEST_F(PasswordSuggestionGeneratorTest, NoPasswordFormFillData) {
  std::vector<Suggestion> suggestions = generator().GetSuggestionsForDomain(
      {}, favicon(), /*username_filter=*/u"", ForPasswordField(false),
      ShowAllPasswords(false), OffersGeneration(false),
      ShowPasswordSuggestions(true), ShowWebAuthnCredentials(false));

  EXPECT_TRUE(suggestions.empty());
}

}  // namespace password_manager
