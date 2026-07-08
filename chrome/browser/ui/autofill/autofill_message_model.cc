// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_message_model.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/grit/components_scaled_resources.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillMessageModel::AutofillMessageModel(
    std::unique_ptr<messages::MessageWrapper> message,
    Type type)
    : AutofillMessageModel(std::move(message),
                           type,
                           base::DoNothing(),
                           base::DoNothing()) {}

AutofillMessageModel::AutofillMessageModel(
    std::unique_ptr<messages::MessageWrapper> message,
    Type type,
    base::OnceClosure action_callback,
    messages::MessageWrapper::DismissCallback dismiss_callback)
    : message_(std::move(message)),
      type_(type),
      action_callback_(std::move(action_callback)),
      dismiss_callback_(std::move(dismiss_callback)) {}

AutofillMessageModel::~AutofillMessageModel() = default;

AutofillMessageModel::AutofillMessageModel(AutofillMessageModel&&) = default;

AutofillMessageModel& AutofillMessageModel::operator=(AutofillMessageModel&&) =
    default;

std::unique_ptr<AutofillMessageModel>
AutofillMessageModel::CreateForSaveCardFailure() {
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForSaveCardFailure(/*is_for_save_and_fill=*/false);

  std::unique_ptr<messages::MessageWrapper> message =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::SAVE_CARD_FAILURE);
  message->SetTitle(ui_params.title_text);
  message->SetDescription(ui_params.description_text);
  message->SetPrimaryButtonText(ui_params.failure_ok_button_text);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableWalletBrandingV2)) {
    message->SetIconResourceId(
        ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC_OLD));
  }

  return base::WrapUnique(
      new AutofillMessageModel(std::move(message), Type::kSaveCardFailure));
}

std::unique_ptr<AutofillMessageModel>
AutofillMessageModel::CreateForVirtualCardEnrollFailure(
    std::u16string card_label) {
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForVirtualCardFailure(card_label);

  std::unique_ptr<messages::MessageWrapper> message =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::VIRTUAL_CARD_ENROLL_FAILURE);
  message->SetTitle(ui_params.title_text);
  message->SetDescription(ui_params.description_text);
  message->SetPrimaryButtonText(ui_params.failure_ok_button_text);
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC_OLD));

  return base::WrapUnique(new AutofillMessageModel(
      std::move(message), Type::kVirtualCardEnrollFailure));
}

std::unique_ptr<AutofillMessageModel>
AutofillMessageModel::CreateForPersonalContextFetchingFailure() {
  std::unique_ptr<messages::MessageWrapper> message =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::PERSONAL_CONTEXT_FETCHING_FAILURE);
  message->SetTitle(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_PRE_FETCH_ERROR_MESSAGE));
  message->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRE_FETCH_ERROR_MESSAGE_BUTTON_TEXT));
  message->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDROID_AUTOFILL_ID_CHROME_PRODUCT));

  return base::WrapUnique(new AutofillMessageModel(
      std::move(message), Type::kPersonalContextFetchingFailure));
}

std::unique_ptr<AutofillMessageModel>
AutofillMessageModel::CreateForPrivateInferenceNotice(
    content::WebContents* web_contents) {
  std::unique_ptr<messages::MessageWrapper> message =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::PRIVATE_INFERENCE_NOTICE);
  message->SetTitle(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_TITLE));
  message->SetDescription(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_DESCRIPTION));
  message->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_PRIMARY_BUTTON_TEXT));
  message->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDROID_AUTOFILL_ID_CHROME_PRODUCT));
  message->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  // TODO(crbug.com/530174611): Record user consent when the positive button is
  // clicked.
  message->SetSecondaryActionCallback(
      base::BindRepeating(&ShowAutofillSettings, web_contents));

  return base::WrapUnique(new AutofillMessageModel(
      std::move(message), Type::kPrivateInferenceNotice));
}

std::string_view AutofillMessageModel::TypeToString(Type message_type) {
  switch (message_type) {
    case Type::kUnspecified:
      return "Unspecified";
    case Type::kSaveCardFailure:
      return "SaveCardFailure";
    case Type::kVirtualCardEnrollFailure:
      return "VirtualCardEnrollFailure";
    case Type::kEntitySaveUpdateFlow:
      return "EntitySaveUpdateFlow";
    case Type::kAddressSaveUpdateFlow:
      return "AddressSaveUpdateFlow";
    case Type::kPersonalContextFetchingFailure:
      return "PersonalContextFetchingFailure";
    case Type::kPrivateInferenceNotice:
      return "PrivateInferenceNotice";
  }
}

messages::MessageWrapper& AutofillMessageModel::GetMessage(
    base::PassKey<AutofillMessageControllerImpl> pass_key) {
  return *message_;
}

const AutofillMessageModel::Type& AutofillMessageModel::GetType() const {
  return type_;
}

std::string_view AutofillMessageModel::GetTypeAsString() const {
  return TypeToString(type_);
}

void AutofillMessageModel::OnActionClicked() {
  if (action_callback_) {
    std::move(action_callback_).Run();
  }
}

void AutofillMessageModel::OnDismissed(messages::DismissReason reason) {
  if (dismiss_callback_) {
    std::move(dismiss_callback_).Run(reason);
  }
}

}  // namespace autofill
