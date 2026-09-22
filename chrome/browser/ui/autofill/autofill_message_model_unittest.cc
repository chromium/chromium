// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_message_model.h"

#include <string>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/autofill/autofill_message_model_test_api.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableWalletBrandingV2);

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
  EXPECT_EQ(test_api(*message_model).GetMessage().GetIconResourceId(), 0);
  EXPECT_EQ(message_model->GetType(),
            AutofillMessageModel::Type::kSaveCardFailure);
  EXPECT_EQ(message_model->GetTypeAsString(),
            AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kSaveCardFailure));
}

TEST(AutofillMessageModelTest,
     VerifySaveCardFailureAttributes_WalletBrandingV2Disabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillEnableWalletBrandingV2);

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

TEST(AutofillMessageModelTest, VerifyResurrectChurnedUsersAttributes) {
  base::MockCallback<base::OnceClosure> action_callback;
  base::MockCallback<messages::MessageWrapper::DismissCallback>
      dismiss_callback;

  std::unique_ptr<AutofillMessageModel> message_model =
      AutofillMessageModel::CreateForResurrectChurnedUsers(
          action_callback.Get(), dismiss_callback.Get());

  EXPECT_EQ(
      test_api(*message_model).GetMessage().GetTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CHURNED_USERS_MESSAGE_TITLE));
  EXPECT_EQ(test_api(*message_model).GetMessage().GetDescription(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CHURNED_USERS_MESSAGE_DESCRIPTION));
  EXPECT_EQ(test_api(*message_model).GetMessage().GetPrimaryButtonText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CHURNED_USERS_BUBBLE_ACCEPT_BUTTON_LABEL));
  EXPECT_EQ(test_api(*message_model).GetMessage().GetIconResourceId(),
            ResourceMapper::MapToJavaDrawableId(
                IDR_ANDROID_AUTOFILL_ID_CHROME_PRODUCT));
  EXPECT_EQ(message_model->GetType(),
            AutofillMessageModel::Type::kResurrectChurnedUsers);
  EXPECT_EQ(message_model->GetTypeAsString(),
            AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kResurrectChurnedUsers));

  EXPECT_CALL(action_callback, Run);
  message_model->OnActionClicked();

  EXPECT_CALL(dismiss_callback, Run(messages::DismissReason::PRIMARY_ACTION));
  message_model->OnDismissed(messages::DismissReason::PRIMARY_ACTION);
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
  EXPECT_EQ(AutofillMessageModel::TypeToString(
                AutofillMessageModel::Type::kResurrectChurnedUsers),
            "ResurrectChurnedUsers");
}
}  // namespace autofill
