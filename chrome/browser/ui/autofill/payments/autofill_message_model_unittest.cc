// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_message_model.h"

#include <string>

#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/grit/components_scaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillMessageModelTest : public testing::Test {
 public:
  AutofillMessageModelTest() = default;

  messages::MessageWrapper& GetMessage(const AutofillMessageModel& model) {
    return *model.message_;
  }

  std::string_view TypeToString(
      const AutofillMessageModel::Type& message_type) {
    return AutofillMessageModel::TypeToString(message_type);
  }
};

TEST_F(AutofillMessageModelTest, VerifySaveCardFailureAttributes) {
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForSaveCardFailure();

  std::unique_ptr<AutofillMessageModel> message_model =
      AutofillMessageModel::CreateForSaveCardFailure();

  EXPECT_EQ(GetMessage(*message_model).GetTitle(), ui_params.title_text);
  EXPECT_EQ(GetMessage(*message_model).GetDescription(),
            ui_params.description_text);
  EXPECT_EQ(GetMessage(*message_model).GetPrimaryButtonText(),
            ui_params.failure_ok_button_text);
  EXPECT_EQ(GetMessage(*message_model).GetIconResourceId(),
            ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC));
  EXPECT_EQ(message_model->GetType(),
            AutofillMessageModel::Type::kSaveCardFailure);
  EXPECT_EQ(message_model->GetTypeAsString(),
            TypeToString(AutofillMessageModel::Type::kSaveCardFailure));
}

TEST_F(AutofillMessageModelTest, VerifyVirtualCardEnrollFailureAttributes) {
  std::u16string card_label = u"Visa ****1234";
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForVirtualCardFailure(card_label);

  std::unique_ptr<AutofillMessageModel> message_model =
      AutofillMessageModel::CreateForVirtualCardEnrollFailure(card_label);

  EXPECT_EQ(GetMessage(*message_model).GetTitle(), ui_params.title_text);
  EXPECT_EQ(GetMessage(*message_model).GetDescription(),
            ui_params.description_text);
  EXPECT_EQ(GetMessage(*message_model).GetPrimaryButtonText(),
            ui_params.failure_ok_button_text);
  EXPECT_EQ(GetMessage(*message_model).GetIconResourceId(),
            ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC));
  EXPECT_EQ(message_model->GetType(),
            AutofillMessageModel::Type::kVirtualCardEnrollFailure);
  EXPECT_EQ(
      message_model->GetTypeAsString(),
      TypeToString(AutofillMessageModel::Type::kVirtualCardEnrollFailure));
}

TEST_F(AutofillMessageModelTest, AutofillMessageTypeToString) {
  EXPECT_EQ(TypeToString(AutofillMessageModel::Type::kUnspecified),
            "Unspecified");
  EXPECT_EQ(TypeToString(AutofillMessageModel::Type::kSaveCardFailure),
            "SaveCardFailure");
  EXPECT_EQ(TypeToString(AutofillMessageModel::Type::kVirtualCardEnrollFailure),
            "VirtualCardEnrollFailure");
}
}  // namespace autofill
