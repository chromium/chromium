// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/saml_challenge_key_handler.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/prefs/pref_service.h"

using enterprise_connectors::kUserContextAwareAccessSignalsAllowlistPref;

namespace ash {

namespace {

const char kSuccessField[] = "success";
const char kResponseField[] = "response";

const size_t kPatternsSizeWarningLevel = 500;

bool UrlMatchesPattern(const GURL& url, const base::Value::List& patterns) {
  if (!url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  if (patterns.size() >= kPatternsSizeWarningLevel) {
    LOG(WARNING) << "Allowed urls list size is " << patterns.size()
                 << ". Check may be slow.";
  }

  for (const base::Value& cur_pattern : patterns) {
    if (ContentSettingsPattern::FromString(cur_pattern.GetString())
            .Matches(url)) {
      return true;
    }
  }
  return false;
}

bool AreContextAwareAccessSignalsEnabledForUrl(const GURL& url,
                                               const Profile* profile) {
  const PrefService* prefs = profile->GetPrefs();
  if (!prefs ||
      !prefs->HasPrefPath(kUserContextAwareAccessSignalsAllowlistPref)) {
    return false;
  }

  return prefs->IsManagedPreference(
             kUserContextAwareAccessSignalsAllowlistPref) &&
         UrlMatchesPattern(
             url, prefs->GetList(kUserContextAwareAccessSignalsAllowlistPref));
}

void LogVerifiedAccessForSAMLDeviceTrustMatchesEndpoints(bool is_matching) {
  base::UmaHistogramBoolean(
      "Enterprise.VerifiedAccess.SAML.DeviceTrustMatchesEndpoints",
      is_matching);
}

}  // namespace

SamlChallengeKeyHandler::SamlChallengeKeyHandler() = default;
SamlChallengeKeyHandler::~SamlChallengeKeyHandler() = default;

void SamlChallengeKeyHandler::Run(Profile* profile,
                                  CallbackType callback,
                                  const GURL& url,
                                  const std::string& challenge) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  profile_ = profile;

  // Device attestation is currently allowed only on the OOBE screen.
  if (LoginState::Get()->IsUserLoggedIn()) {
    ReturnResult(attestation::TpmChallengeKeyResult::MakeError(
        attestation::TpmChallengeKeyResultCode::
            kDeviceWebBasedAttestationNotOobeError));
    return;
  }

  if (!base::Base64Decode(challenge, &decoded_challenge_)) {
    ReturnResult(attestation::TpmChallengeKeyResult::MakeError(
        attestation::TpmChallengeKeyResultCode::kChallengeBadBase64Error));
    return;
  }

  BuildResponseForAllowlistedUrl(url);
}

void SamlChallengeKeyHandler::SetTpmResponseTimeoutForTesting(
    base::TimeDelta timeout) {
  tpm_response_timeout_for_testing_ = timeout;
}

void SamlChallengeKeyHandler::BuildResponseForAllowlistedUrl(const GURL& url) {
  CrosSettings* settings = CrosSettings::Get();
  CrosSettingsProvider::TrustedStatus status = settings->PrepareTrustedValues(
      base::BindOnce(&SamlChallengeKeyHandler::BuildResponseForAllowlistedUrl,
                     weak_factory_.GetWeakPtr(), url));

  const base::Value::List* patterns = nullptr;
  switch (status) {
    case CrosSettingsProvider::TRUSTED:
      if (!settings->GetList(kDeviceWebBasedAttestationAllowedUrls,
                             &patterns)) {
        patterns = nullptr;
      }
      break;
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Do nothing. This function will be called again when the values are
      // ready.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      patterns = nullptr;
      break;
  }

  if (!patterns || !UrlMatchesPattern(url, *patterns)) {
    ReturnResult(attestation::TpmChallengeKeyResult::MakeError(
        attestation::TpmChallengeKeyResultCode::
            kDeviceWebBasedAttestationUrlError));
    return;
  }

  // Prioritize Context Aware Signals over VerifiedAccess if both are defined
  // for the same endpoint, since they are both reacting to the same VA
  // Challenge
  if (AreContextAwareAccessSignalsEnabledForUrl(url, profile_)) {
    LogVerifiedAccessForSAMLDeviceTrustMatchesEndpoints(true);
    ReturnResult(attestation::TpmChallengeKeyResult::MakeError(
        attestation::TpmChallengeKeyResultCode::kDeviceTrustURLConflictError));
    return;
  }

  LogVerifiedAccessForSAMLDeviceTrustMatchesEndpoints(false);
  BuildChallengeResponse();
}

void SamlChallengeKeyHandler::BuildChallengeResponse() {
  tpm_key_challenger_ =
      std::make_unique<attestation::TpmChallengeKeyWithTimeout>();
  tpm_key_challenger_->BuildResponse(
      GetTpmResponseTimeout(), ::attestation::ENTERPRISE_MACHINE, profile_,
      base::BindOnce(&SamlChallengeKeyHandler::ReturnResult,
                     weak_factory_.GetWeakPtr()),
      decoded_challenge_, /*register_key=*/false,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name_for_spkac=*/"",
      /*signals=*/std::nullopt);
}

base::TimeDelta SamlChallengeKeyHandler::GetTpmResponseTimeout() const {
  if (tpm_response_timeout_for_testing_.has_value()) {
    return tpm_response_timeout_for_testing_.value();
  }
  return default_tpm_response_timeout_;
}

void SamlChallengeKeyHandler::ReturnResult(
    const attestation::TpmChallengeKeyResult& result) {
  base::Value::Dict js_result;
  if (!result.IsSuccess()) {
    LOG(WARNING) << "Device attestation error: " << result.GetErrorMessage();
  }

  std::string encoded_result_data =
      base::Base64Encode(result.challenge_response);

  js_result.Set(kSuccessField, result.IsSuccess());
  js_result.Set(kResponseField, encoded_result_data);

  std::move(callback_).Run(std::move(js_result));
  tpm_key_challenger_.reset();
}

}  // namespace ash
