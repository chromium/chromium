// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ERROR_UNUSABLE_SWBN_FILE_ERROR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ERROR_UNUSABLE_SWBN_FILE_ERROR_H_

#include <string>

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"

namespace web_app {

// The status provides information about if the Signed Web Bundle (.swbn) file
// can be used at all. If this error occurs then we should not read any
// response that this .swbn file contains. This error means that the file
// has critical errors (bad signature, wrong format, etc). If such an error
// occurs we can't do much with it and most probably we should delete
// the file.
class UnusableSwbnFileError {
 public:
  // So far this enum represents every error type that can occur during
  // integrity block and metadata parsing, before responses are read from
  // Signed Web Bundles. In future we may add more errors here.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Error {
    // Integrity Block-related errors
    kIntegrityBlockParserInternalError = 1,
    kIntegrityBlockParserFormatError = 2,
    kIntegrityBlockParserVersionError = 3,
    kIntegrityBlockValidationError = 4,

    // Signature verification errors
    kSignatureVerificationError = 5,

    // Metadata-related errors
    kMetadataParserInternalError = 6,
    kMetadataParserFormatError = 7,
    kMetadataParserVersionError = 8,
    kMetadataValidationError = 9,

    kMaxValue = kMetadataValidationError
  };

  explicit UnusableSwbnFileError(Error error, std::string message = "");
  explicit UnusableSwbnFileError(
      const web_package::mojom::BundleIntegrityBlockParseErrorPtr& error);
  explicit UnusableSwbnFileError(
      const web_package::mojom::BundleMetadataParseErrorPtr& error);
  explicit UnusableSwbnFileError(
      const web_package::SignedWebBundleSignatureVerifier::Error& error);

  UnusableSwbnFileError(const UnusableSwbnFileError& other) = default;
  UnusableSwbnFileError& operator=(const UnusableSwbnFileError& other) =
      default;
  UnusableSwbnFileError(UnusableSwbnFileError&& other) = default;
  UnusableSwbnFileError& operator=(UnusableSwbnFileError&& other) = default;

  Error value() const { return error_; }
  const std::string& message() const { return message_; }
  static void UmaLogStatus(UnusableSwbnFileError);

 private:
  Error error_;
  std::string message_;
};

bool operator==(const UnusableSwbnFileError& lhs,
                const UnusableSwbnFileError& rhs);
UnusableSwbnFileError::Error ToErrorEnum(const UnusableSwbnFileError& err);
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ERROR_UNUSABLE_SWBN_FILE_ERROR_H_
