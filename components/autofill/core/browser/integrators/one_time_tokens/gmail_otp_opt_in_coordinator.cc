// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_coordinator.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace autofill {

GmailOtpOptInCoordinator::GmailOtpOptInCoordinator(
    AutofillClient& client,
    one_time_tokens::OneTimeTokenService* one_time_token_service)
    : client_(client), one_time_token_service_(one_time_token_service) {
  if (OtpFieldDetector* detector = client_->GetOtpFieldDetector()) {
    otp_fields_detected_subscription_ =
        detector->RegisterOtpFieldsDetectedCallback(
            base::BindRepeating(&GmailOtpOptInCoordinator::OnOtpFieldsDetected,
                                weak_ptr_factory_.GetWeakPtr()));
    if (detector->IsOtpFieldPresent()) {
      OnOtpFieldsDetected();
    }
  }
}

GmailOtpOptInCoordinator::~GmailOtpOptInCoordinator() = default;

void GmailOtpOptInCoordinator::OnOtpFieldsDetected() {
  if (!one_time_token_service_) {
    return;
  }
  const base::Time expiration = base::Time::Now() + kTickleSubscriptionDuration;
  if (tickle_subscription_.IsAlive()) {
    tickle_subscription_.SetExpirationTime(expiration);
    return;
  }
  tickle_subscription_ = one_time_token_service_->SubscribeToTickles(
      one_time_tokens::OneTimeTokenSource::kGmail, expiration,
      base::BindRepeating(&GmailOtpOptInCoordinator::OnTickleReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GmailOtpOptInCoordinator::OnTickleReceived(
    one_time_tokens::OneTimeTokenSource source) {
  if (is_bubble_showing_) {
    return;
  }

  const PrefService* prefs = client_->GetPrefs();
  if (!prefs || prefs::IsAutofillGmailOtpFillingEnabled(prefs)) {
    return;
  }

  const signin::IdentityManager* identity_manager =
      client_->GetIdentityManager();
  if (!identity_manager) {
    return;
  }
  const std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  if (email.empty()) {
    return;
  }

  is_bubble_showing_ = true;
  client_->ShowGmailOtpOptInBubble(
      base::UTF8ToUTF16(email),
      base::BindOnce(&GmailOtpOptInCoordinator::OnOptInBubbleClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GmailOtpOptInCoordinator::OnOptInBubbleClosed(GmailOtpOptInResult result) {
  is_bubble_showing_ = false;
  // Outcome handling is implemented in a follow-up CL.
}

}  // namespace autofill
