// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/trusted_vault/trusted_vault_error_suggestion_generator.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/password_manager/mock_password_manager_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ReturnedSuggestions = SuggestionGenerator::ReturnedSuggestions;
using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

class TrustedVaultErrorSuggestionGeneratorTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient> {
 public:
  void SetUp() override {
    InitAutofillClient();
    generator_ = std::make_unique<TrustedVaultErrorSuggestionGenerator>();
    form_ = test::CreateTestAddressFormData();
    autofill_client().set_password_manager_delegate(
        std::make_unique<testing::NiceMock<MockPasswordManagerDelegate>>());
  }

  void TearDown() override { DestroyAutofillClient(); }

  MockPasswordManagerDelegate& password_delegate() {
    return static_cast<MockPasswordManagerDelegate&>(
        *client().GetPasswordManagerDelegate(FieldGlobalId()));
  }

  TrustedVaultErrorSuggestionGenerator& generator() { return *generator_; }

  AutofillClient& client() { return autofill_client(); }

  FormData& form() { return form_; }

  const FormFieldData& field() { return form_.fields()[0]; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment test_environment_;
  std::unique_ptr<TrustedVaultErrorSuggestionGenerator> generator_;
  FormData form_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(TrustedVaultErrorSuggestionGeneratorTest,
       FetchCreatesValidSuggestionForGenerate) {
  Suggestion suggestion(SuggestionType::kPendingStateTrustedVaultError);
  EXPECT_CALL(password_delegate(), GetTrustedVaultUnlockSuggestion)
      .WillRepeatedly(Return(suggestion));

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb, Run(Pair(SuggestionDataSource::kTrustedVaultError,
                                    ElementsAre(suggestion))));
  generator().GenerateSuggestions(form(), field(), /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  generate_cb.Get());
}

TEST_F(TrustedVaultErrorSuggestionGeneratorTest,
       FetchReturnsEmptyWhenNoSuggestionAvailable) {
  EXPECT_CALL(password_delegate(), GetTrustedVaultUnlockSuggestion)
      .WillRepeatedly(Return(std::nullopt));

  base::MockOnceCallback<void(ReturnedSuggestions)> generate_cb;
  EXPECT_CALL(generate_cb,
              Run(Pair(SuggestionDataSource::kTrustedVaultError, IsEmpty())));
  generator().GenerateSuggestions(form(), field(), /*form_structure=*/nullptr,
                                  /*trigger_autofill_field=*/nullptr, client(),
                                  generate_cb.Get());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace autofill
