// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

using autofill_metrics::LogMandatoryReauthOfferOptInDecision;
using autofill_metrics::MandatoryReauthOfferOptInDecision;

MandatoryReauthManager::MandatoryReauthManager(AutofillClient* client)
    : client_(client) {
  if (client_) {
    device_authenticator_ = client_->GetDeviceAuthenticator();
  }
}

MandatoryReauthManager::~MandatoryReauthManager() = default;

void MandatoryReauthManager::Authenticate(
    device_reauth::DeviceAuthenticator::AuthenticateCallback callback) {
  CHECK(device_authenticator_);
  device_authenticator_->AuthenticateWithMessage(
      u"", base::BindOnce(&MandatoryReauthManager::OnAuthenticationCompleted,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MandatoryReauthManager::AuthenticateWithMessage(
    const std::u16string& message,
    device_reauth::DeviceAuthenticator::AuthenticateCallback callback) {
  CHECK(device_authenticator_);
  device_authenticator_->AuthenticateWithMessage(
      message,
      base::BindOnce(&MandatoryReauthManager::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MandatoryReauthManager::StartDeviceAuthentication(
    base::OnceCallback<void(bool)> authentication_complete_callback) {
  MandatoryReauthAuthenticationMethod authentication_method =
      GetAuthenticationMethod();

  // If there is no supported auth method on the device, we should skip re-auth
  // and fill the form. Otherwise the user removing authentication on the
  // device will prevent them from using payments autofill. In the settings
  // page, we signal to the user through various means that they need to turn
  // the device's authentication on in order to use re-auth.
  if (authentication_method ==
          payments::MandatoryReauthAuthenticationMethod::kUnknown ||
      authentication_method ==
          payments::MandatoryReauthAuthenticationMethod::kUnsupportedMethod) {
    std::move(authentication_complete_callback).Run(true);
    return;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  AuthenticateWithMessage(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_AUTOFILL_FILLING_MANDATORY_REAUTH),
      std::move(authentication_complete_callback));
#elif BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1427216): Convert this to
  // DeviceAuthenticator::AuthenticateWithMessage() with the correct message
  // once it is supported. Currently, the message is "Verify it's you".
  Authenticate(std::move(authentication_complete_callback));
#else
  NOTREACHED_NORETURN();
#endif
}

void MandatoryReauthManager::OnAuthenticationCompleted(
    device_reauth::DeviceAuthenticator::AuthenticateCallback callback,
    bool success) {
  std::move(callback).Run(success);
}

bool MandatoryReauthManager::ShouldOfferOptin(
    std::optional<CreditCard::RecordType>
        card_record_type_if_non_interactive_authentication_flow_completed) {
  opt_in_source_ = autofill_metrics::MandatoryReauthOptInOrOutSource::kUnknown;
  // We should not offer to update a user pref in off the record mode.
  if (client_->IsOffTheRecord()) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kIncognitoMode);
    return false;
  }

  // If the user prefs denote that we should not display the re-auth opt-in
  // bubble, return that we should not offer mandatory re-auth opt-in.
  // Pref-related decision logging also occurs within this function call.
  if (!client_->GetPersonalDataManager()
           ->ShouldShowPaymentMethodsMandatoryReauthPromo()) {
    return false;
  }

  // If the device authenticator is not present or we can not authenticate with
  // biometric or screen lock, there will be no way to re-auth if the user
  // enrolls, so return that we should not offer mandatory re-auth opt-in.
  if (!device_authenticator_ ||
      !device_authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kNoSupportedReauthMethod);
    return false;
  }

  // If `card_record_type_if_non_interactive_authentication_flow_completed` is
  // not present, this can mean one of two things: 1) No card was autofilled 2)
  // All autofilled cards went through an interactive authentication flow. In
  // the first case it makes no sense to show a reauth proposal because this is
  // not an autofill moment. In the second case, we don't want to show an opt-in
  // prompt because the user never experienced non-interactive authentication,
  // and actually just went through an interactive authentication. Displaying a
  // prompt to enable re-authentication could be confusing.
  if (!card_record_type_if_non_interactive_authentication_flow_completed
           .has_value()) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::
            kWentThroughInteractiveAuthentication);
    return false;
  }

  // At this point, we know the most recent payments autofill had a
  // non-interactive authentication. Set `opt_in_source_` based on the record
  // type of the last non-interactive authentication, and return that we should
  // offer re-auth opt-in.
  switch (card_record_type_if_non_interactive_authentication_flow_completed
              .value()) {
    case CreditCard::RecordType::kLocalCard:
      opt_in_source_ =
          autofill_metrics::MandatoryReauthOptInOrOutSource::kCheckoutLocalCard;
      break;
    case CreditCard::RecordType::kFullServerCard:
      opt_in_source_ = autofill_metrics::MandatoryReauthOptInOrOutSource::
          kCheckoutFullServerCard;
      break;
    case CreditCard::RecordType::kVirtualCard:
      opt_in_source_ = autofill_metrics::MandatoryReauthOptInOrOutSource::
          kCheckoutVirtualCard;
      break;
    case CreditCard::RecordType::kMaskedServerCard:
      opt_in_source_ = autofill_metrics::MandatoryReauthOptInOrOutSource::
          kCheckoutMaskedServerCard;
      break;
  }
  LogMandatoryReauthOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kOffered);
  return true;
}

void MandatoryReauthManager::StartOptInFlow() {
  client_->ShowMandatoryReauthOptInPrompt(
      base::BindOnce(&MandatoryReauthManager::OnUserAcceptedOptInPrompt,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MandatoryReauthManager::OnUserCancelledOptInPrompt,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&MandatoryReauthManager::OnUserClosedOptInPrompt,
                          weak_ptr_factory_.GetWeakPtr()));
}

void MandatoryReauthManager::OnUserAcceptedOptInPrompt() {
  autofill_metrics::LogMandatoryReauthOptInOrOutUpdateEvent(
      opt_in_source_,
      /*opt_in=*/true,
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  AuthenticateWithMessage(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_AUTOFILL_MANDATORY_REAUTH_PROMPT),
      base::BindOnce(
          &MandatoryReauthManager::OnOptInAuthenticationStepCompleted,
          weak_ptr_factory_.GetWeakPtr()));
#elif BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1427216): Convert this to
  // DeviceAuthenticator::AuthenticateWithMessage() with the correct message
  // once it is supported. Currently, the message is "Verify it's you".
  Authenticate(base::BindOnce(
      &MandatoryReauthManager::OnOptInAuthenticationStepCompleted,
      weak_ptr_factory_.GetWeakPtr()));
#else
  NOTREACHED_NORETURN();
#endif
}

void MandatoryReauthManager::OnOptInAuthenticationStepCompleted(bool success) {
  autofill_metrics::LogMandatoryReauthOptInOrOutUpdateEvent(
      opt_in_source_,
      /*opt_in=*/true,
      success ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowSucceeded
              : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowFailed);
  if (success) {
    client_->GetPersonalDataManager()->SetPaymentMethodsMandatoryReauthEnabled(
        /*enabled=*/true);
    client_->ShowMandatoryReauthOptInConfirmation();
  } else {
    client_->GetPersonalDataManager()
        ->IncrementPaymentMethodsMandatoryReauthPromoShownCounter();
  }
}

void MandatoryReauthManager::OnUserCancelledOptInPrompt() {
  client_->GetPersonalDataManager()->SetPaymentMethodsMandatoryReauthEnabled(
      /*enabled=*/false);
}

void MandatoryReauthManager::OnUserClosedOptInPrompt() {
  client_->GetPersonalDataManager()
      ->IncrementPaymentMethodsMandatoryReauthPromoShownCounter();
}

MandatoryReauthAuthenticationMethod
MandatoryReauthManager::GetAuthenticationMethod() {
  if (!device_authenticator_) {
    return MandatoryReauthAuthenticationMethod::kUnknown;
  }
  // Order matters here.
  if (device_authenticator_->CanAuthenticateWithBiometrics()) {
    return MandatoryReauthAuthenticationMethod::kBiometric;
  }
  if (device_authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    return MandatoryReauthAuthenticationMethod::kScreenLock;
  }
  return MandatoryReauthAuthenticationMethod::kUnsupportedMethod;
}

}  // namespace autofill::payments
