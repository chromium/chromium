// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SSL_ERRORS_ERROR_INFO_H_
#define COMPONENTS_SSL_ERRORS_ERROR_INFO_H_

#include <string>
#include <vector>

#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

class GURL;

namespace ssl_errors {

// This class describes an error that happened while showing a page over SSL.
// An ErrorInfo object only exists on the UI thread and only contains
// information about an error (type of error and text details).
// Note no DISALLOW_COPY_AND_ASSIGN as we want the copy constructor.
class ErrorInfo {
 public:
  // This enum is being histogrammed; please only add new values at the end.
  enum ErrorType {
    CERT_COMMON_NAME_INVALID = 0,
    CERT_DATE_INVALID = 1,
    CERT_AUTHORITY_INVALID = 2,
    CERT_CONTAINS_ERRORS = 3,
    CERT_NO_REVOCATION_MECHANISM = 4,
    CERT_UNABLE_TO_CHECK_REVOCATION = 5,
    CERT_REVOKED = 6,
    CERT_INVALID = 7,
    CERT_WEAK_SIGNATURE_ALGORITHM = 8,
    CERT_WEAK_KEY = 9,
    CERT_NAME_CONSTRAINT_VIOLATION = 10,
    UNKNOWN = 11,
    // CERT_WEAK_KEY_DH = 12,
    CERT_PINNED_KEY_MISSING = 13,
    CERT_VALIDITY_TOO_LONG = 14,
    CERTIFICATE_TRANSPARENCY_REQUIRED = 15,
    CERT_SYMANTEC_LEGACY = 16,
    CERT_KNOWN_INTERCEPTION_BLOCKED = 17,
    LEGACY_TLS = 18,
    CERT_NON_UNIQUE_NAME = 19,
    END_OF_ENUM
  };

  virtual ~ErrorInfo();

  // Converts a network error code to an ErrorType.
  static ErrorType NetErrorToErrorType(int net_error);

  static ErrorInfo CreateError(ErrorType error_type,
                               net::X509Certificate* cert,
                               const GURL& request_url);

  // Populates the specified |errors| vector with the errors contained in
  // |cert_status| for |cert|.  Returns the number of errors found.
  // Callers only interested in the error count can pass NULL for |errors|.
  static void GetErrorsForCertStatus(
      const scoped_refptr<net::X509Certificate>& cert,
      net::CertStatus cert_status,
      const GURL& url,
      std::vector<ErrorInfo>* errors);

  // A description of the error.
  const std::u16string& details() const { return details_; }

  // A short message describing the error (1 line).
  const std::u16string& short_description() const { return short_description_; }

 private:
  ErrorInfo(const std::u16string& details,
            const std::u16string& short_description);

  std::u16string details_;
  std::u16string short_description_;
};

}  // namespace ssl_errors

#endif  // COMPONENTS_SSL_ERRORS_ERROR_INFO_H_
