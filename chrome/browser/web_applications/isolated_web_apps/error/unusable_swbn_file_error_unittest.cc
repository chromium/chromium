// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

TEST(UnusableSwbnFileError, General) {
  {
    UnusableSwbnFileError one_parameter_error_ctor = UnusableSwbnFileError(
        UnusableSwbnFileError::Error::kSignatureVerificationError);
    EXPECT_EQ(one_parameter_error_ctor.value(),
              UnusableSwbnFileError::Error::kSignatureVerificationError);
    EXPECT_TRUE(one_parameter_error_ctor.message().empty());
  }

  {
    UnusableSwbnFileError two_parameters_error_ctor = UnusableSwbnFileError(
        UnusableSwbnFileError::Error::kSignatureVerificationError,
        "Unknown error message");
    EXPECT_EQ(two_parameters_error_ctor.value(),
              UnusableSwbnFileError::Error::kSignatureVerificationError);
    EXPECT_EQ(two_parameters_error_ctor.message(), "Unknown error message");
  }

  {
    auto ib_parse_error =
        web_package::mojom::BundleIntegrityBlockParseError::New();
    ib_parse_error->type =
        web_package::mojom::BundleParseErrorType::kVersionError;
    ib_parse_error->message = "Error message";
    UnusableSwbnFileError error = UnusableSwbnFileError(ib_parse_error);
    EXPECT_EQ(error.value(),
              UnusableSwbnFileError::Error::kIntegrityBlockParserVersionError);
    EXPECT_EQ(error.message(), "Error message");
  }

  {
    auto metadata_error = web_package::mojom::BundleMetadataParseError::New();
    metadata_error->type =
        web_package::mojom::BundleParseErrorType::kVersionError;
    metadata_error->message = "Error message";
    UnusableSwbnFileError error = UnusableSwbnFileError(metadata_error);
    EXPECT_EQ(error.value(),
              UnusableSwbnFileError::Error::kMetadataParserVersionError);
    EXPECT_EQ(error.message(), "Error message");
  }

  {
    auto signature_error = web_package::SignedWebBundleSignatureVerifier::
        Error::ForInvalidSignature("Error message");
    UnusableSwbnFileError error = UnusableSwbnFileError(signature_error);
    EXPECT_EQ(error.value(),
              UnusableSwbnFileError::Error::kSignatureVerificationError);
    EXPECT_EQ(error.message(), "Error message");
  }
}

}  // namespace
}  // namespace web_app
