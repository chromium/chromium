// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_assistant.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace autofill {
namespace {

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(TestAutofillDriver* driver,
                      TestAutofillClient* client,
                      PersonalDataManager* pdm,
                      AutocompleteHistoryManager* ahm)
      // Force to use the constructor designated for unit test.
      : AutofillManager(driver, client, pdm, ahm) {}
  virtual ~MockAutofillManager() {}

  MOCK_METHOD5(FillCreditCardForm,
               void(int query_id,
                    const FormData& form,
                    const FormFieldData& field,
                    const CreditCard& credit_card,
                    const base::string16& cvc));

  using AutofillManager::mutable_form_structures;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

}  // namespace

class AutofillAssistantTest : public testing::Test {
 protected:
  AutofillAssistantTest()
      : task_environment_(),
        autofill_client_(),
        autofill_driver_(),
        pdm_(),
        ahm_() {}

  void SetUp() {
    payments::TestPaymentsClient* payments_client =
        new payments::TestPaymentsClient(autofill_driver_.GetURLLoaderFactory(),
                                         autofill_client_.GetIdentityManager(),
                                         &pdm_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client));
    TestCreditCardSaveManager* credit_card_save_manager =
        new TestCreditCardSaveManager(&autofill_driver_, &autofill_client_,
                                      payments_client, &pdm_);
    autofill::TestFormDataImporter* test_form_data_importer =
        new TestFormDataImporter(
            &autofill_client_, payments_client,
            std::unique_ptr<CreditCardSaveManager>(credit_card_save_manager),
            &pdm_, "en-US");
    autofill_client_.set_test_form_data_importer(
        std::unique_ptr<TestFormDataImporter>(test_form_data_importer));

    autofill_manager_ = std::make_unique<MockAutofillManager>(
        &autofill_driver_, &autofill_client_, &pdm_, &ahm_);

    autofill_assistant_ =
        std::make_unique<AutofillAssistant>(autofill_manager_.get());
  }

  void EnableAutofillCreditCardAssist() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillCreditCardAssist);
  }

  // Returns a valid credit card form.
  FormData CreateValidCreditCardFormData() {
    FormData form;
    form.url = GURL("https://myform.com");
    form.action = GURL("https://myform.com/submit");

    FormFieldData field;
    field.form_control_type = "text";

    field.label = base::ASCIIToUTF16("Name on Card");
    field.name = base::ASCIIToUTF16("name_on_card");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Card Number");
    field.name = base::ASCIIToUTF16("card_number");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Exp Month");
    field.name = base::ASCIIToUTF16("ccmonth");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Exp Year");
    field.name = base::ASCIIToUTF16("ccyear");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Verification");
    field.name = base::ASCIIToUTF16("verification");
    form.fields.push_back(field);

    return form;
  }

  // Returns an initialized FormStructure with credit card form data. To be
  // owned by the caller.
  std::unique_ptr<FormStructure> CreateValidCreditCardForm() {
    std::unique_ptr<FormStructure> form_structure;
    form_structure.reset(new FormStructure(CreateValidCreditCardFormData()));
    form_structure->DetermineHeuristicTypes();
    return form_structure;
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate* full_card_unmask_delegate() {
    payments::FullCardRequest* full_card_request =
        autofill_manager_->credit_card_access_manager_->cvc_authenticator_
            ->full_card_request_.get();
    DCHECK(full_card_request);
    return static_cast<CardUnmaskDelegate*>(full_card_request);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  testing::NiceMock<TestAutofillDriver> autofill_driver_;
  TestPersonalDataManager pdm_;
  MockAutocompleteHistoryManager ahm_;
  std::unique_ptr<MockAutofillManager> autofill_manager_;
  std::unique_ptr<AutofillAssistant> autofill_assistant_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

MATCHER_P(CreditCardMatches, guid, "") {
  return arg.guid() == guid;
}

// If the feature is turned off, CanShowCreditCardAssist() always returns
// false.
TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOff) {
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();

  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_FALSE(autofill_assistant_->CanShowCreditCardAssist());
}

// Tests that with the feature enabled and proper input,
// CanShowCreditCardAssist() behaves as expected.
TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn) {
  EnableAutofillCreditCardAssist();

  EXPECT_FALSE(autofill_assistant_->CanShowCreditCardAssist());

  // With valid input, the function extracts the credit card form properly.
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();
  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_TRUE(autofill_assistant_->CanShowCreditCardAssist());
}

// Tests that with the feature enabled and proper input,
// CanShowCreditCardAssist() behaves as expected for secure contexts.
TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn_Secure) {
  EnableAutofillCreditCardAssist();

  // Can be shown if the context is secure.
  FormData form = CreateValidCreditCardFormData();
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_TRUE(autofill_assistant_->CanShowCreditCardAssist());
}

// Tests that with the feature enabled and proper input,
// CanShowCreditCardAssist() behaves as expected for insecure contexts.
TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn_NotSecure) {
  EnableAutofillCreditCardAssist();

  // Cannot be shown if the context is not secure.
  FormData form = CreateValidCreditCardFormData();
  form.url = GURL("http://myform.com");
  form.action = GURL("http://myform.com/submit");
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_FALSE(autofill_assistant_->CanShowCreditCardAssist());
}

TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn_Javascript) {
  EnableAutofillCreditCardAssist();

  // Can be shown if the context is secure and the form action is a javascript
  // function (which is a valid url).
  FormData form = CreateValidCreditCardFormData();
  form.action = GURL("javascript:alert('hello');");
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_TRUE(autofill_assistant_->CanShowCreditCardAssist());
}

TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn_WeirdJs) {
  EnableAutofillCreditCardAssist();

  // Can be shown if the context is secure and the form action is a javascript
  // function that may or may not be valid.
  FormData form = CreateValidCreditCardFormData();
  form.action = GURL("javascript:myFunc");
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_TRUE(autofill_assistant_->CanShowCreditCardAssist());
}

TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn_EmptyAction) {
  EnableAutofillCreditCardAssist();

  // Can be shown if the context is secure and the form action is empty.
  FormData form = CreateValidCreditCardFormData();
  form.action = GURL();
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_TRUE(autofill_assistant_->CanShowCreditCardAssist());
}

TEST_F(AutofillAssistantTest, ShowAssistForCreditCard_ValidCard_CancelCvc) {
  EnableAutofillCreditCardAssist();
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();

  // Will extract the credit card form data.
  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_TRUE(autofill_assistant_->CanShowCreditCardAssist());

  // Create a valid card for the assist.
  CreditCard card;
  test::SetCreditCardInfo(&card, "John Doe", "4111111111111111", "05", "2999",
                          "1");

  // FillCreditCardForm should not be called if the user cancelled the CVC.
  EXPECT_CALL(*autofill_manager_, FillCreditCardForm(_, _, _, _, _)).Times(0);

  autofill_assistant_->ShowAssistForCreditCard(card);
  full_card_unmask_delegate()->OnUnmaskPromptClosed();
}

TEST_F(AutofillAssistantTest, ShowAssistForCreditCard_ValidCard_SubmitCvc) {
  EnableAutofillCreditCardAssist();
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();

  // Will extract the credit card form data.
  auto& form_structures = *autofill_manager_->mutable_form_structures();
  auto signature = form_structure->form_signature();
  form_structures[signature] = std::move(form_structure);
  EXPECT_TRUE(autofill_assistant_->CanShowCreditCardAssist());

  // Create a valid card for the assist.
  CreditCard card;
  test::SetCreditCardInfo(&card, "John Doe", "4111111111111111", "05", "2999",
                          "1");

  // FillCreditCardForm ends up being called after user has accepted the
  // prompt.
  EXPECT_CALL(
      *autofill_manager_,
      FillCreditCardForm(kNoQueryId, _, _, CreditCardMatches(card.guid()),
                         base::ASCIIToUTF16("123")));

  autofill_assistant_->ShowAssistForCreditCard(card);

  CardUnmaskDelegate::UserProvidedUnmaskDetails unmask_details;
  unmask_details.cvc = base::ASCIIToUTF16("123");
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(unmask_details);
}

}  // namespace autofill
