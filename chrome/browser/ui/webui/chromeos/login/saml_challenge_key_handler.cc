// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/saml_challenge_key_handler.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace chromeos {

const char kSamlChallengeKeyHandlerResultMetric[] =
    "ChromeOS.SAML.SamlChallengeKeyHandlerResult";

namespace {

const char kSuccessField[] = "success";
const char kResponseField[] = "response";

const size_t kPatternsSizeWarningLevel = 500;

void RecordChallengeKeyResult(
    attestation::TpmChallengeKeyResultCode result_code) {
  base::UmaHistogramEnumeration(kSamlChallengeKeyHandlerResultMetric,
                                result_code);
}

// Checks if `url` matches one of the `patterns`.
bool IsDeviceWebBasedAttestationEnabledForUrl(const GURL& url,
                                              const base::ListValue* patterns) {
  if (!patterns) {
    return false;
  }

  if (!url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  if (patterns->GetSize() >= kPatternsSizeWarningLevel) {
    LOG(WARNING) << "Allowed urls list size is " << patterns->GetSize()
                 << ". Check may be slow.";
  }

  for (const base::Value& cur_pattern : *patterns) {
    if (ContentSettingsPattern::FromString(cur_pattern.GetString())
            .Matches(url)) {
      return true;
    }
  }
  return false;
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

  const base::ListValue* patterns = nullptr;
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

  if (!IsDeviceWebBasedAttestationEnabledForUrl(url, patterns)) {
    ReturnResult(attestation::TpmChallengeKeyResult::MakeError(
        attestation::TpmChallengeKeyResultCode::
            kDeviceWebBasedAttestationUrlError));
    return;
  }

  BuildChallengeResponse();
}

void SamlChallengeKeyHandler::BuildChallengeResponse() {
  tpm_key_challenger_ =
      std::make_unique<attestation::TpmChallengeKeyWithTimeout>();
  tpm_key_challenger_->BuildResponse(
      GetTpmResponseTimeout(), attestation::KEY_DEVICE, profile_,
      base::BindOnce(&SamlChallengeKeyHandler::ReturnResult,
                     weak_factory_.GetWeakPtr()),
      decoded_challenge_, /*register_key=*/false, /*key_name_for_spkac=*/"");
}

base::TimeDelta SamlChallengeKeyHandler::GetTpmResponseTimeout() const {
  if (tpm_response_timeout_for_testing_.has_value()) {
    return tpm_response_timeout_for_testing_.value();
  }
  return default_tpm_response_timeout_;
}

void SamlChallengeKeyHandler::ReturnResult(
    const attestation::TpmChallengeKeyResult& result) {
  RecordChallengeKeyResult(result.result_code);

  base::Value js_result(base::Value::Type::DICTIONARY);
  if (!result.IsSuccess()) {
    LOG(WARNING) << "Device attestation error: " << result.GetErrorMessage();
  }

  std::string encoded_result_data;
  base::Base64Encode(result.challenge_response, &encoded_result_data);

  js_result.SetKey(kSuccessField, base::Value(result.IsSuccess()));
  js_result.SetKey(kResponseField, base::Value(encoded_result_data));

  std::move(callback_).Run(std::move(js_result));
  tpm_key_challenger_.reset();
}

}  // namespace chromeos
