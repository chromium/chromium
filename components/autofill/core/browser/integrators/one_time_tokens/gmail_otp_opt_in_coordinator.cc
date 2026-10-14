// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_coordinator.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace autofill {

namespace {

// Key used by `SupportsUserData` on `session_user_data_` (whose address serves
// as the unique `const void*` key) to record whether the Gmail OTP opt-in
// bubble was dismissed during the current browser session.
constexpr int kGmailOtpOptInDismissedInSessionKey = 0;

}  // namespace

GmailOtpOptInCoordinator::GmailOtpOptInCoordinator(
    AutofillClient& client,
    one_time_tokens::GmailOtpBackend* gmail_otp_backend,
    base::SupportsUserData& session_user_data)
    : client_(client),
      gmail_otp_backend_(gmail_otp_backend),
      session_user_data_(session_user_data) {
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
  if (!gmail_otp_backend_) {
    return;
  }
  const base::Time expiration = base::Time::Now() + kTickleSubscriptionDuration;
  if (tickle_subscription_.IsAlive()) {
    tickle_subscription_.SetExpirationTime(expiration);
    return;
  }
  tickle_subscription_ = gmail_otp_backend_->SubscribeToTickles(
      expiration,
      base::BindRepeating(&GmailOtpOptInCoordinator::OnTickleReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GmailOtpOptInCoordinator::OnTickleReceived() {
  if (is_bubble_showing_) {
    return;
  }

  const PrefService* prefs = client_->GetPrefs();
  if (!prefs || prefs::IsAutofillGmailOtpFillingEnabled(prefs)) {
    return;
  }

  const base::Time dismissal_time =
      prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs);
  if (!dismissal_time.is_null() &&
      base::Time::Now() - dismissal_time < kDeclinedCooldownDuration) {
    return;
  }

  if (session_user_data_->GetUserData(&kGmailOtpOptInDismissedInSessionKey)) {
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
  switch (result) {
    case GmailOtpOptInResult::kAccepted:
      if (PrefService* prefs = client_->GetPrefs()) {
        prefs::SetAutofillGmailOtpFillingEnabled(prefs, true);
        prefs::ClearAutofillGmailOtpFillingActivationDismissalTimestamp(prefs);
      }
      break;
    case GmailOtpOptInResult::kDeclined:
      if (PrefService* prefs = client_->GetPrefs()) {
        prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp(
            prefs, base::Time::Now());
      }
      break;
    case GmailOtpOptInResult::kClosed:
      session_user_data_->SetUserData(
          &kGmailOtpOptInDismissedInSessionKey,
          std::make_unique<base::SupportsUserData::Data>());
      break;
    case GmailOtpOptInResult::kLostFocus:
    case GmailOtpOptInResult::kDiscarded:
    case GmailOtpOptInResult::kUnknown:
      break;
  }
}

}  // namespace autofill
