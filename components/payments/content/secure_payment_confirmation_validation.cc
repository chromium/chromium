// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_validation.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/payments/core/native_error_strings.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {

namespace {

// Arbitrarily chosen limit of 1 hour.
//
// TODO(crbug.com/516452538): Validate if we need to keep this or if we can
// rely on WebAuthn to validate the timeout for us.
constexpr int64_t kMaxTimeoutInMilliseconds = 1000 * 60 * 60;

// The maximum size of the payment instrument details string. Arbitrarily chosen
// while being much larger than any reasonable input.
constexpr size_t kMaxInstrumentDetailsSize = 4096;

// Determine whether an RP ID is a 'valid domain' as per the URL spec:
// https://url.spec.whatwg.org/#valid-domain
//
// TODO(crbug.com/40858925): This is a workaround to a lack of support for
// 'valid domain's in the //url code.
bool IsValidDomain(const std::string& rp_id) {
  // A valid domain, such as 'site.example', should be a URL host (and nothing
  // more of the URL!) that is not an IP address.
  GURL url("https://" + rp_id);
  return url.is_valid() && url.GetHost() == rp_id && !url.HostIsIPAddress();
}

}  // namespace

bool IsValidSecurePaymentConfirmationRequest(
    const mojom::SecurePaymentConfirmationRequestPtr& request,
    std::string* error_message) {
  CHECK(request);

  if (request->credential_ids.empty()) {
    *error_message = errors::kCredentialIdsRequired;
    return false;
  }

  for (const auto& credential_id : request->credential_ids) {
    if (credential_id.empty()) {
      *error_message = errors::kCredentialIdsRequired;
      return false;
    }
  }

  if (request->timeout.has_value() &&
      request->timeout.value().InMilliseconds() > kMaxTimeoutInMilliseconds) {
    *error_message = errors::kTimeoutTooLong;
    return false;
  }

  if (request->challenge.empty()) {
    *error_message = errors::kChallengeRequired;
    return false;
  }

  if (!request->instrument) {
    *error_message = errors::kInstrumentRequired;
    return false;
  }

  if (request->instrument->display_name.empty()) {
    *error_message = errors::kInstrumentDisplayNameRequired;
    return false;
  }

  if (!request->instrument->icon.is_valid()) {
    *error_message = errors::kValidInstrumentIconRequired;
    return false;
  }

  if (request->instrument->details.has_value()) {
    if (!base::IsStringUTF8(*request->instrument->details)) {
      *error_message = errors::kNonUtf8InstrumentDetailsString;
      return false;
    }

    if (request->instrument->details->empty()) {
      *error_message = errors::kEmptyInstrumentDetailsString;
      return false;
    }

    if (request->instrument->details->size() > kMaxInstrumentDetailsSize) {
      *error_message = errors::kTooLongInstrumentDetailsString;
      return false;
    }
  }

  if (!IsValidDomain(request->rp_id)) {
    *error_message = errors::kRpIdRequired;
    return false;
  }

  if ((!request->payee_origin.has_value() &&
       !request->payee_name.has_value()) ||
      (request->payee_name.has_value() && request->payee_name->empty())) {
    *error_message = errors::kPayeeOriginOrPayeeNameRequired;
    return false;
  }

  if (request->payee_origin.has_value() &&
      request->payee_origin->scheme() != url::kHttpsScheme) {
    *error_message = errors::kPayeeOriginMustBeHttps;
    return false;
  }

  if (!request->payment_entities_logos.empty()) {
    for (const mojom::PaymentEntityLogoPtr& logo :
         request->payment_entities_logos) {
      if (logo.is_null()) {
        *error_message = errors::kNonNullPaymentEntityLogoRequired;
        return false;
      }

      if (!logo->url.is_valid()) {
        *error_message = errors::kValidLogoUrlRequired;
        return false;
      }
      if (!logo->url.SchemeIsHTTPOrHTTPS() &&
          !logo->url.SchemeIs(url::kDataScheme)) {
        *error_message = errors::kValidLogoUrlSchemeRequired;
        return false;
      }
      if (logo->label.empty()) {
        *error_message = errors::kLogoLabelRequired;
        return false;
      }
    }
  }

  return true;
}

}  // namespace payments
