// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SAML_CHALLENGE_KEY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SAML_CHALLENGE_KEY_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_with_timeout.h"

namespace ash {

// This class handles "samlChallengeMachineKey" request for GaiaScreenHandler.
// It calculates response for a challenge from Verified Access server for remote
// attestation during SAML authentication.
class SamlChallengeKeyHandler final {
 public:
  using CallbackType =
      base::OnceCallback<void(const base::Value::Dict response)>;

  SamlChallengeKeyHandler();
  SamlChallengeKeyHandler(const SamlChallengeKeyHandler&) = delete;
  SamlChallengeKeyHandler& operator=(const SamlChallengeKeyHandler&) = delete;
  ~SamlChallengeKeyHandler();

  // Checks that provided `url` is allowlisted and tries to calculate response
  // for the `challenge`.
  void Run(Profile* profile,
           CallbackType callback,
           const GURL& url,
           const std::string& challenge);

  void SetTpmResponseTimeoutForTesting(base::TimeDelta timeout);

 private:
  // Checks if it is allowed for provided `url` to perform device attestation.
  void BuildResponseForAllowlistedUrl(const GURL& url);
  // Starts flow that acutally builds a response.
  void BuildChallengeResponse();
  // Returns current timeout for `tpm_key_challenger_` to response.
  base::TimeDelta GetTpmResponseTimeout() const;
  // Single return point from all checks, `tpm_key_challenger_` and timeout
  // task.
  void ReturnResult(const attestation::TpmChallengeKeyResult& result);

  raw_ptr<Profile> profile_ = nullptr;
  std::string decoded_challenge_;
  // Callback to return a result of ChallengeKey.
  CallbackType callback_;

  // Timeout for `tpm_key_challenger_` to response.
  const base::TimeDelta default_tpm_response_timeout_ = base::Seconds(15);
  std::optional<base::TimeDelta> tpm_response_timeout_for_testing_;

  // Performs attestation flow.
  std::unique_ptr<attestation::TpmChallengeKeyWithTimeout> tpm_key_challenger_;

  base::WeakPtrFactory<SamlChallengeKeyHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SAML_CHALLENGE_KEY_HANDLER_H_
