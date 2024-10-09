// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/ssl_errors/error_info.h"

#include <stddef.h>

#include "base/i18n/message_formatter.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::UTF8ToUTF16;

namespace ssl_errors {

ErrorInfo::ErrorInfo(const std::u16string& details,
                     const std::u16string& short_description)
    : details_(details), short_description_(short_description) {}

// static
ErrorInfo ErrorInfo::CreateError(ErrorType error_type,
                                 net::X509Certificate* cert,
                                 const GURL& request_url) {
  std::u16string details, short_description;
  switch (error_type) {
    case CERT_COMMON_NAME_INVALID: {
      std::vector<std::string> dns_names;
      cert->GetSubjectAltName(&dns_names, nullptr);

      size_t i = 0;
      if (dns_names.empty()) {
        // The certificate had no DNS names, display an explanatory string.
        details = l10n_util::GetStringFUTF16(
            IDS_CERT_ERROR_NO_SUBJECT_ALTERNATIVE_NAMES_DETAILS,
            UTF8ToUTF16(request_url.host()));
      } else {
        // If the certificate contains multiple DNS names, we choose the most
        // representative one -- either the DNS name that's also in the subject
        // field, or the first one. If this heuristic turns out to be
        // inadequate, we can consider choosing the DNS name that is the
        // "closest match" to the host name in the request URL, or listing all
        // the DNS names with an HTML <ul>.
        for (; i < dns_names.size(); ++i) {
          if (dns_names[i] == cert->subject().common_name)
            break;
        }
        if (i == dns_names.size())
          i = 0;

        details = l10n_util::GetStringFUTF16(
            IDS_CERT_ERROR_COMMON_NAME_INVALID_DETAILS,
            UTF8ToUTF16(request_url.host()),
            base::EscapeForHTML(UTF8ToUTF16(dns_names[i])));
      }

      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_COMMON_NAME_INVALID_DESCRIPTION);
      break;
    }
    case CERT_DATE_INVALID:
      if (cert->HasExpired()) {
        // Make sure to round up to the smallest integer value not less than
        // the expiration value (https://crbug.com/476758).
        int expiration_value =
            (base::Time::Now() - cert->valid_expiry()).InDays() + 1;
        details = base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_CERT_ERROR_EXPIRED_DETAILS),
            request_url.host(), expiration_value, base::Time::Now());
        short_description =
            l10n_util::GetStringUTF16(IDS_CERT_ERROR_EXPIRED_DESCRIPTION);
      } else if (base::Time::Now() < cert->valid_start()) {
        details = base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_CERT_ERROR_NOT_YET_VALID_DETAILS),
            request_url.host(),
            (cert->valid_start() - base::Time::Now()).InDays());
        short_description =
            l10n_util::GetStringUTF16(IDS_CERT_ERROR_NOT_YET_VALID_DESCRIPTION);
      } else {
        // Two possibilities: (1) an intermediate or root certificate has
        // expired, or (2) the certificate has become valid since the error
        // occurred. Both are probably rare cases. To avoid giving the wrong
        // date, remove the information.
        details = l10n_util::GetStringFUTF16(
            IDS_CERT_ERROR_NOT_VALID_AT_THIS_TIME_DETAILS,
            UTF8ToUTF16(request_url.host()));
        short_description = l10n_util::GetStringUTF16(
            IDS_CERT_ERROR_NOT_VALID_AT_THIS_TIME_DESCRIPTION);
      }
      break;
    case CERT_KNOWN_INTERCEPTION_BLOCKED:
    case CERT_AUTHORITY_INVALID:
    case CERT_SYMANTEC_LEGACY:
      details =
          l10n_util::GetStringFUTF16(IDS_CERT_ERROR_AUTHORITY_INVALID_DETAILS,
                                     UTF8ToUTF16(request_url.host()));
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_AUTHORITY_INVALID_DESCRIPTION);
      break;
    case CERT_CONTAINS_ERRORS:
      details =
          l10n_util::GetStringFUTF16(IDS_CERT_ERROR_CONTAINS_ERRORS_DETAILS,
                                     UTF8ToUTF16(request_url.host()));
      short_description =
          l10n_util::GetStringUTF16(IDS_CERT_ERROR_CONTAINS_ERRORS_DESCRIPTION);
      break;
    case CERT_NO_REVOCATION_MECHANISM:
      details = l10n_util::GetStringFUTF16(
          IDS_CERT_ERROR_NO_REVOCATION_MECHANISM_DETAILS,
          UTF8ToUTF16(request_url.host()));
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_NO_REVOCATION_MECHANISM_DESCRIPTION);
      break;
    case CERT_REVOKED:
      details = l10n_util::GetStringFUTF16(IDS_CERT_ERROR_REVOKED_CERT_DETAILS,
                                           UTF8ToUTF16(request_url.host()));
      short_description =
          l10n_util::GetStringUTF16(IDS_CERT_ERROR_REVOKED_CERT_DESCRIPTION);
      break;
    case CERT_INVALID:
      details = l10n_util::GetStringFUTF16(IDS_CERT_ERROR_INVALID_CERT_DETAILS,
                                           UTF8ToUTF16(request_url.host()));
      short_description =
          l10n_util::GetStringUTF16(IDS_CERT_ERROR_INVALID_CERT_DESCRIPTION);
      break;
    case CERT_WEAK_SIGNATURE_ALGORITHM:
      details = l10n_util::GetStringFUTF16(
          IDS_CERT_ERROR_WEAK_SIGNATURE_ALGORITHM_DETAILS,
          UTF8ToUTF16(request_url.host()));
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_WEAK_SIGNATURE_ALGORITHM_DESCRIPTION);
      break;
    case CERT_WEAK_KEY:
      details = l10n_util::GetStringFUTF16(IDS_CERT_ERROR_WEAK_KEY_DETAILS,
                                           UTF8ToUTF16(request_url.host()));
      short_description =
          l10n_util::GetStringUTF16(IDS_CERT_ERROR_WEAK_KEY_DESCRIPTION);
      break;
    case CERT_NAME_CONSTRAINT_VIOLATION:
      details = l10n_util::GetStringFUTF16(
          IDS_CERT_ERROR_NAME_CONSTRAINT_VIOLATION_DETAILS,
          UTF8ToUTF16(request_url.host()));
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_NAME_CONSTRAINT_VIOLATION_DESCRIPTION);
      break;
    case CERT_VALIDITY_TOO_LONG:
      details =
          l10n_util::GetStringFUTF16(IDS_CERT_ERROR_VALIDITY_TOO_LONG_DETAILS,
                                     UTF8ToUTF16(request_url.host()));
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_VALIDITY_TOO_LONG_DESCRIPTION);
      break;
    case CERT_PINNED_KEY_MISSING:
      details = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_SUMMARY_PINNING_FAILURE_DETAILS);
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_SUMMARY_PINNING_FAILURE_DESCRIPTION);
      break;
    case CERT_UNABLE_TO_CHECK_REVOCATION:
      details = l10n_util::GetStringFUTF16(
          IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_DETAILS,
          UTF8ToUTF16(request_url.host()));
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_UNABLE_TO_CHECK_REVOCATION_DESCRIPTION);
      break;
    case CERTIFICATE_TRANSPARENCY_REQUIRED:
      details = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_CERTIFICATE_TRANSPARENCY_REQUIRED_DETAILS);
      short_description = l10n_util::GetStringUTF16(
          IDS_CERT_ERROR_CERTIFICATE_TRANSPARENCY_REQUIRED_DESCRIPTION);
      break;
    case CERT_NON_UNIQUE_NAME:
      details =
          l10n_util::GetStringFUTF16(IDS_CERT_ERROR_NON_UNIQUE_NAME_DETAILS,
                                     UTF8ToUTF16(request_url.host()));
      short_description =
          l10n_util::GetStringUTF16(IDS_CERT_ERROR_NON_UNIQUE_NAME_DESCRIPTION);
      break;
    case UNKNOWN:
      details = l10n_util::GetStringUTF16(IDS_CERT_ERROR_UNKNOWN_ERROR_DETAILS);
      short_description =
          l10n_util::GetStringUTF16(IDS_CERT_ERROR_UNKNOWN_ERROR_DESCRIPTION);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return ErrorInfo(details, short_description);
}

ErrorInfo::~ErrorInfo() = default;

// static
ErrorInfo::ErrorType ErrorInfo::NetErrorToErrorType(int net_error) {
  switch (net_error) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
      return CERT_COMMON_NAME_INVALID;
    case net::ERR_CERT_DATE_INVALID:
      return CERT_DATE_INVALID;
    case net::ERR_CERT_AUTHORITY_INVALID:
      return CERT_AUTHORITY_INVALID;
    case net::ERR_CERT_CONTAINS_ERRORS:
      return CERT_CONTAINS_ERRORS;
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
      return CERT_NO_REVOCATION_MECHANISM;
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      return CERT_UNABLE_TO_CHECK_REVOCATION;
    case net::ERR_CERT_REVOKED:
      return CERT_REVOKED;
    case net::ERR_CERT_INVALID:
      return CERT_INVALID;
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
      return CERT_WEAK_SIGNATURE_ALGORITHM;
    case net::ERR_CERT_NON_UNIQUE_NAME:
      return CERT_NON_UNIQUE_NAME;
    case net::ERR_CERT_WEAK_KEY:
      return CERT_WEAK_KEY;
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
      return CERT_NAME_CONSTRAINT_VIOLATION;
    case net::ERR_CERT_VALIDITY_TOO_LONG:
      return CERT_VALIDITY_TOO_LONG;
    case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return CERT_PINNED_KEY_MISSING;
    case net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
      return CERTIFICATE_TRANSPARENCY_REQUIRED;
    case net::ERR_CERT_SYMANTEC_LEGACY:
      return CERT_SYMANTEC_LEGACY;
    case net::ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
      return CERT_KNOWN_INTERCEPTION_BLOCKED;
    default:
      NOTREACHED_IN_MIGRATION();
      return UNKNOWN;
  }
}

// static
void ErrorInfo::GetErrorsForCertStatus(
    const scoped_refptr<net::X509Certificate>& cert,
    net::CertStatus cert_status,
    const GURL& url,
    std::vector<ErrorInfo>* errors) {
  const net::CertStatus kErrorFlags[] = {
      net::CERT_STATUS_COMMON_NAME_INVALID,
      net::CERT_STATUS_DATE_INVALID,
      net::CERT_STATUS_AUTHORITY_INVALID,
      net::CERT_STATUS_NO_REVOCATION_MECHANISM,
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
      net::CERT_STATUS_REVOKED,
      net::CERT_STATUS_INVALID,
      net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
      net::CERT_STATUS_NON_UNIQUE_NAME,
      net::CERT_STATUS_WEAK_KEY,
      net::CERT_STATUS_NAME_CONSTRAINT_VIOLATION,
      net::CERT_STATUS_VALIDITY_TOO_LONG,
      net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED,
      net::CERT_STATUS_SYMANTEC_LEGACY,
      net::CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED,
  };

  const ErrorType kErrorTypes[] = {
      CERT_COMMON_NAME_INVALID,
      CERT_DATE_INVALID,
      CERT_AUTHORITY_INVALID,
      CERT_NO_REVOCATION_MECHANISM,
      CERT_UNABLE_TO_CHECK_REVOCATION,
      CERT_REVOKED,
      CERT_INVALID,
      CERT_WEAK_SIGNATURE_ALGORITHM,
      CERT_NON_UNIQUE_NAME,
      CERT_WEAK_KEY,
      CERT_NAME_CONSTRAINT_VIOLATION,
      CERT_VALIDITY_TOO_LONG,
      CERTIFICATE_TRANSPARENCY_REQUIRED,
      CERT_SYMANTEC_LEGACY,
      CERT_KNOWN_INTERCEPTION_BLOCKED,
  };
  DCHECK(std::size(kErrorFlags) == std::size(kErrorTypes));

  for (size_t i = 0; i < std::size(kErrorFlags); ++i) {
    if ((cert_status & kErrorFlags[i]) && errors) {
      errors->push_back(
          ErrorInfo::CreateError(kErrorTypes[i], cert.get(), url));
    }
  }
}

}  // namespace ssl_errors
