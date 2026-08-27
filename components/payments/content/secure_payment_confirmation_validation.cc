// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_validation.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"
#include "base/i18n/tag_converters.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "components/webauthn/core/browser/webauthn_security_utils.h"
#include "third_party/blink/public/common/features_generated.h"
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

// Returns true if a single request tag matches against the app tag. A match
// occurs in the following cases:
// - The request tag and app tag both include a region, and the tags are an
// exact match.
// - At least one of the request tag or app tag does not include a region and
// base::i18n:LanguageTagMatcher::Match returns true.
bool MatchLanguageTags(const std::vector<std::string>& request_tags,
                       const std::string& app_tag) {
  if (request_tags.empty()) {
    return true;
  }

  std::optional<base::i18n::LanguageTag> parsed_app_tag =
      base::i18n::LanguageTagConverter::GetInstance().FromString(app_tag);
  if (!parsed_app_tag) {
    return false;
  }

  base::i18n::LanguageTagMatcher matcher =
      base::i18n::LanguageTagMatcher::Create({*parsed_app_tag});

  for (const std::string& tag_str : request_tags) {
    std::optional<base::i18n::LanguageTag> parsed_request_tag =
        base::i18n::LanguageTagConverter::GetInstance().FromString(tag_str);
    if (!parsed_request_tag) {
      continue;
    }

    // If both the request and app tags have region sub tags they must be an
    // exact match.
    if (!parsed_app_tag->region_subtag().empty() &&
        !parsed_request_tag->region_subtag().empty() &&
        !matcher.HasExactMatch(*parsed_request_tag)) {
      continue;
    }

    if (matcher.Match(*parsed_request_tag).has_value()) {
      return true;
    }
  }

  return false;
}

// Returns the outcome of matching the request's provided locales against the
// application locale.
SecurePaymentConfirmationLocaleOutcome GetSpcLocaleOutcome(
    const mojom::SecurePaymentConfirmationRequestPtr& request,
    const std::string& application_locale) {
  if (request->locales.empty()) {
    return SecurePaymentConfirmationLocaleOutcome::kLocaleNotProvided;
  }
  return MatchLanguageTags(request->locales, application_locale)
             ? SecurePaymentConfirmationLocaleOutcome::kMatch
             : SecurePaymentConfirmationLocaleOutcome::kNoMatch;
}

}  // namespace

std::string SecurePaymentConfirmationRequestValidationErrorToString(
    SecurePaymentConfirmationRequestValidationError error) {
  switch (error) {
    case SecurePaymentConfirmationRequestValidationError::kOk:
      return "";
    case SecurePaymentConfirmationRequestValidationError::kSPCMethodMustBeNull:
      return errors::kSpcDisabledMustBeNull;
    case SecurePaymentConfirmationRequestValidationError::
        kSPCMethodMustNotBeNull:
      return errors::kSpcEnabledMustNotBeNull;
    case SecurePaymentConfirmationRequestValidationError::
        kMultiplePaymentMethodsNotAllowed:
      return errors::kSpcMustBeOnlyPaymentMethod;
    case SecurePaymentConfirmationRequestValidationError::kUnsupportedOptions:
      return errors::kSpcUnsupportedOptions;
    case SecurePaymentConfirmationRequestValidationError::
        kCredentialIdsRequired:
      return errors::kCredentialIdsRequired;
    case SecurePaymentConfirmationRequestValidationError::kTimeoutTooLong:
      return errors::kTimeoutTooLong;
    case SecurePaymentConfirmationRequestValidationError::kChallengeRequired:
      return errors::kChallengeRequired;
    case SecurePaymentConfirmationRequestValidationError::kInstrumentRequired:
      return errors::kInstrumentRequired;
    case SecurePaymentConfirmationRequestValidationError::
        kInstrumentDisplayNameRequired:
      return errors::kInstrumentDisplayNameRequired;
    case SecurePaymentConfirmationRequestValidationError::
        kValidInstrumentIconRequired:
      return errors::kValidInstrumentIconRequired;
    case SecurePaymentConfirmationRequestValidationError::
        kNonUtf8InstrumentDetailsString:
      return errors::kNonUtf8InstrumentDetailsString;
    case SecurePaymentConfirmationRequestValidationError::
        kEmptyInstrumentDetailsString:
      return errors::kEmptyInstrumentDetailsString;
    case SecurePaymentConfirmationRequestValidationError::
        kTooLongInstrumentDetailsString:
      return errors::kTooLongInstrumentDetailsString;
    case SecurePaymentConfirmationRequestValidationError::kRpIdRequired:
      return errors::kRpIdRequired;
    case SecurePaymentConfirmationRequestValidationError::
        kPayeeOriginOrPayeeNameRequired:
      return errors::kPayeeOriginOrPayeeNameRequired;
    case SecurePaymentConfirmationRequestValidationError::
        kPayeeOriginMustBeHttps:
      return errors::kPayeeOriginMustBeHttps;
    case SecurePaymentConfirmationRequestValidationError::
        kNonNullPaymentEntityLogoRequired:
      return errors::kNonNullPaymentEntityLogoRequired;
    case SecurePaymentConfirmationRequestValidationError::kValidLogoUrlRequired:
      return errors::kValidLogoUrlRequired;
    case SecurePaymentConfirmationRequestValidationError::
        kValidLogoUrlSchemeRequired:
      return errors::kValidLogoUrlSchemeRequired;
    case SecurePaymentConfirmationRequestValidationError::kLogoLabelRequired:
      return errors::kLogoLabelRequired;
    case SecurePaymentConfirmationRequestValidationError::kInternalError:
      return errors::kInternalError;
    case SecurePaymentConfirmationRequestValidationError::
        kWebAuthnExtensionsNotSupported:
      return errors::kWebAuthnExtensionsNotSupported;
    case SecurePaymentConfirmationRequestValidationError::kLocaleDoesNotMatch:
      return errors::kSpcLocaleDoesNotMatch;
  }
}

SecurePaymentConfirmationRequestValidationError
IsValidSecurePaymentConfirmationRequest(
    const mojom::SecurePaymentConfirmationRequestPtr& request,
    const url::Origin& initiator_origin,
    const std::string& application_locale) {
  CHECK(request);

  if (request->credential_ids.empty()) {
    return SecurePaymentConfirmationRequestValidationError::
        kCredentialIdsRequired;
  }

  for (const auto& credential_id : request->credential_ids) {
    if (credential_id.empty()) {
      return SecurePaymentConfirmationRequestValidationError::
          kCredentialIdsRequired;
    }
  }

  if (base::FeatureList::IsEnabled(features::kSPCLocaleValidation) &&
      GetSpcLocaleOutcome(request, application_locale) ==
          SecurePaymentConfirmationLocaleOutcome::kNoMatch) {
    return SecurePaymentConfirmationRequestValidationError::kLocaleDoesNotMatch;
  }

  if (request->timeout.has_value() &&
      request->timeout.value().InMilliseconds() > kMaxTimeoutInMilliseconds) {
    return SecurePaymentConfirmationRequestValidationError::kTimeoutTooLong;
  }

  if (request->challenge.empty()) {
    return SecurePaymentConfirmationRequestValidationError::kChallengeRequired;
  }

  if (!request->instrument) {
    return SecurePaymentConfirmationRequestValidationError::kInstrumentRequired;
  }

  if (request->instrument->display_name.empty()) {
    return SecurePaymentConfirmationRequestValidationError::
        kInstrumentDisplayNameRequired;
  }

  if (!request->instrument->icon.is_valid()) {
    return SecurePaymentConfirmationRequestValidationError::
        kValidInstrumentIconRequired;
  }

  if (request->instrument->details.has_value()) {
    if (!base::IsStringUTF8(*request->instrument->details)) {
      return SecurePaymentConfirmationRequestValidationError::
          kNonUtf8InstrumentDetailsString;
    }

    if (request->instrument->details->empty()) {
      return SecurePaymentConfirmationRequestValidationError::
          kEmptyInstrumentDetailsString;
    }

    if (request->instrument->details->size() > kMaxInstrumentDetailsSize) {
      return SecurePaymentConfirmationRequestValidationError::
          kTooLongInstrumentDetailsString;
    }
  }

  if (!IsValidDomain(request->rp_id)) {
    return SecurePaymentConfirmationRequestValidationError::kRpIdRequired;
  }

  if ((!request->payee_origin.has_value() &&
       !request->payee_name.has_value()) ||
      (request->payee_name.has_value() && request->payee_name->empty())) {
    return SecurePaymentConfirmationRequestValidationError::
        kPayeeOriginOrPayeeNameRequired;
  }

  if (request->payee_origin.has_value() &&
      request->payee_origin->scheme() != url::kHttpsScheme) {
    return SecurePaymentConfirmationRequestValidationError::
        kPayeeOriginMustBeHttps;
  }

  if (!request->payment_entities_logos.empty()) {
    for (const mojom::PaymentEntityLogoPtr& logo :
         request->payment_entities_logos) {
      if (logo.is_null()) {
        return SecurePaymentConfirmationRequestValidationError::
            kNonNullPaymentEntityLogoRequired;
      }

      if (!logo->url.is_valid()) {
        return SecurePaymentConfirmationRequestValidationError::
            kValidLogoUrlRequired;
      }
      if (!logo->url.SchemeIsHTTPOrHTTPS() &&
          !logo->url.SchemeIs(url::kDataScheme)) {
        return SecurePaymentConfirmationRequestValidationError::
            kValidLogoUrlSchemeRequired;
      }
      if (logo->label.empty()) {
        return SecurePaymentConfirmationRequestValidationError::
            kLogoLabelRequired;
      }
    }
  }

  if (request->extensions &&
      base::FeatureList::IsEnabled(
          blink::features::
              kSecurePaymentConfirmationExtensionsDisallowForThirdParties)) {
    bool is_third_party = !webauthn::OriginIsAllowedToClaimRelyingPartyId(
        request->rp_id, initiator_origin);

    if (is_third_party) {
      auto empty_extensions =
          blink::mojom::AuthenticationExtensionsClientInputs::New();
      if (!request->extensions.Equals(empty_extensions)) {
        return SecurePaymentConfirmationRequestValidationError::
            kWebAuthnExtensionsNotSupported;
      }
    }
  }

  return SecurePaymentConfirmationRequestValidationError::kOk;
}

void RecordSpcLocaleOutcome(
    const mojom::SecurePaymentConfirmationRequestPtr& request,
    const std::string& application_locale) {
  if (!base::FeatureList::IsEnabled(features::kSPCLocaleValidation)) {
    return;
  }

  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.LocaleOutcome",
      GetSpcLocaleOutcome(request, application_locale));
}

}  // namespace payments
