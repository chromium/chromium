// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_message_model.h"

#include <string>

#include "base/test/mock_callback.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/autofill/autofill_message_model_test_api.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/grit/components_scaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(AutofillMessageModelTest, VerifyCallbacks) {
  std::unique_ptr<messages::MessageWrapper> message_wrapper =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::SAVE_CARD_FAILURE);
  base::MockCallback<base::OnceClosure> action_callback;
  base::MockCallback<messages::MessageWrapper::DismissCallback>
      dismiss_callback;
  AutofillMessageModel message_model(
      std::move(message_wrapper), AutofillMessageModel::Type::kSaveCardFailure,
      action_callback.Get(), dismiss_callback.Get());

  EXPECT_CALL(action_callback, Run);
  EXPECT_CALL(dismiss_callback, Run(messages::DismissReason::TIMER));

  // Call `OnActionClicked` twice, the callback should be run only once.
  message_model.OnActionClicked();
  message_model.OnActionClicked();

  // Call `OnDismissed` twice, the callback should be run only once.
  message_model.OnDismissed(messages::DismissReason::TIMER);
  message_model.OnDismissed(messages::DismissReason::TIMER);
}

TEST(AutofillMessageModelTest, VerifySaveCardFailureAttributes) {
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForSaveCardFailure(/*is_for_save_and_fill=*/false);

  std::unique_ptr<AutofillMessageModel> message_model =
      AutofillMessageModel::CreateForSaveCardFailure();

  EXPECT_EQ(test_api(*message_model).GetMessage().GetTitle(),
            ui_params.title_text);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetDescription(),
            ui_params.description_text);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetPrimaryButtonText(),
            ui_params.failure_ok_button_text);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetIconResourceId(),
            ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC_OLD));
  EXPECT_EQ(message_model->GetType(),
            AutofillMessageModel::Type::kSaveCardFailure);
  EXPECT_EQ(message_model->GetTypeAsString(),
            AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kSaveCardFailure));
}

TEST(AutofillMessageModelTest, VerifyVirtualCardEnrollFailureAttributes) {
  std::u16string card_label = u"Visa ****1234";
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForVirtualCardFailure(card_label);

  std::unique_ptr<AutofillMessageModel> message_model =
      AutofillMessageModel::CreateForVirtualCardEnrollFailure(card_label);

  EXPECT_EQ(test_api(*message_model).GetMessage().GetTitle(),
            ui_params.title_text);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetDescription(),
            ui_params.description_text);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetPrimaryButtonText(),
            ui_params.failure_ok_button_text);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetIconResourceId(),
            ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC_OLD));
  EXPECT_EQ(message_model->GetType(),
            AutofillMessageModel::Type::kVirtualCardEnrollFailure);
  EXPECT_EQ(message_model->GetTypeAsString(),
            AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kVirtualCardEnrollFailure));
}

TEST(AutofillMessageModelTest, AutofillMessageTypeToString) {
  EXPECT_EQ(AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kUnspecified),
            "Unspecified");
  EXPECT_EQ(AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kSaveCardFailure),
            "SaveCardFailure");
  EXPECT_EQ(AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kVirtualCardEnrollFailure),
            "VirtualCardEnrollFailure");
  EXPECT_EQ(AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kEntitySaveUpdateFlow),
            "EntitySaveUpdateFlow");
  EXPECT_EQ(AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kAddressSaveUpdateFlow),
            "AddressSaveUpdateFlow");
}
}  // namespace autofill
