// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"

namespace web_app {
namespace {

enum class ParsingContext { kIntegrityBlock, kMetadata };

UnusableSwbnFileError::Error ConvertError(
    const web_package::mojom::BundleParseErrorType& error,
    ParsingContext parsing_context) {
  switch (parsing_context) {
    case ParsingContext::kIntegrityBlock:
      switch (error) {
        case web_package::mojom::BundleParseErrorType::kParserInternalError:
          return UnusableSwbnFileError::Error::
              kIntegrityBlockParserInternalError;
        case web_package::mojom::BundleParseErrorType::kFormatError:
          return UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError;
        case web_package::mojom::BundleParseErrorType::kVersionError:
          return UnusableSwbnFileError::Error::
              kIntegrityBlockParserVersionError;
      }
    case ParsingContext::kMetadata: {
      switch (error) {
        case web_package::mojom::BundleParseErrorType::kParserInternalError:
          return UnusableSwbnFileError::Error::kMetadataParserInternalError;
        case web_package::mojom::BundleParseErrorType::kFormatError:
          return UnusableSwbnFileError::Error::kMetadataParserFormatError;
        case web_package::mojom::BundleParseErrorType::kVersionError:
          return UnusableSwbnFileError::Error::kMetadataParserVersionError;
      }
    }
  }
}

}  // namespace

UnusableSwbnFileError::UnusableSwbnFileError(UnusableSwbnFileError::Error error,
                                             std::string message)
    : error_(error), message_(std::move(message)) {}

UnusableSwbnFileError::UnusableSwbnFileError(
    const web_package::mojom::BundleIntegrityBlockParseErrorPtr& error)
    : error_(ConvertError(error->type, ParsingContext::kIntegrityBlock)),
      message_(error->message) {}

UnusableSwbnFileError::UnusableSwbnFileError(
    const web_package::mojom::BundleMetadataParseErrorPtr& error)
    : error_(ConvertError(error->type, ParsingContext::kMetadata)),
      message_(error->message) {}

UnusableSwbnFileError::UnusableSwbnFileError(
    const web_package::SignedWebBundleSignatureVerifier::Error& error)
    : error_(UnusableSwbnFileError::Error::kSignatureVerificationError),
      message_(error.message) {}

bool operator==(const UnusableSwbnFileError& lhs,
                const UnusableSwbnFileError& rhs) {
  return (lhs.value() == rhs.value()) && (lhs.message() == rhs.message());
}

// static
UnusableSwbnFileError::Error ToErrorEnum(const UnusableSwbnFileError& err) {
  return err.value();
}

}  // namespace web_app
