// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"

#include <memory>
#include <string>

#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

IsolatedWebAppResponseReaderFactory::IsolatedWebAppResponseReaderFactory(
    std::unique_ptr<IsolatedWebAppValidator> validator,
    base::RepeatingCallback<
        std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>()>
        signature_verifier_factory)
    : validator_(std::move(validator)),
      signature_verifier_factory_(std::move(signature_verifier_factory)) {}

IsolatedWebAppResponseReaderFactory::~IsolatedWebAppResponseReaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IsolatedWebAppResponseReaderFactory::CreateResponseReader(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    bool skip_signature_verification,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(web_bundle_id.type(),
            web_package::SignedWebBundleId::Type::kEd25519PublicKey);

  GURL base_url(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    web_bundle_id.id()}));

  std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
      signature_verifier = signature_verifier_factory_.Run();
  std::unique_ptr<SignedWebBundleReader> reader = SignedWebBundleReader::Create(
      web_bundle_path, std::move(base_url), std::move(signature_verifier));

  SignedWebBundleReader& reader_ref = *reader.get();
  reader_ref.StartReading(
      base::BindOnce(&IsolatedWebAppResponseReaderFactory::OnIntegrityBlockRead,
                     weak_ptr_factory_.GetWeakPtr(), web_bundle_id,
                     skip_signature_verification),
      base::BindOnce(
          &IsolatedWebAppResponseReaderFactory::OnIntegrityBlockAndMetadataRead,
          weak_ptr_factory_.GetWeakPtr(), std::move(reader), web_bundle_path,
          web_bundle_id, std::move(callback)));
}

// static
std::string IsolatedWebAppResponseReaderFactory::ErrorToString(
    const UnusableSwbnFileError& error) {
  switch (error.value()) {
    case UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError:
    case UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError:
    case UnusableSwbnFileError::Error::kIntegrityBlockParserVersionError:
      return base::StringPrintf("Failed to parse integrity block: %s",
                                error.message().c_str());
    case UnusableSwbnFileError::Error::kIntegrityBlockValidationError:
      return base::StringPrintf("Failed to validate integrity block: %s",
                                error.message().c_str());
    case UnusableSwbnFileError::Error::kSignatureVerificationError:
      return base::StringPrintf("Failed to verify signatures: %s",
                                error.message().c_str());
    case UnusableSwbnFileError::Error::kMetadataParserInternalError:
    case UnusableSwbnFileError::Error::kMetadataParserFormatError:
    case UnusableSwbnFileError::Error::kMetadataParserVersionError:
      return base::StringPrintf("Failed to parse metadata: %s",
                                error.message().c_str());
    case UnusableSwbnFileError::Error::kMetadataValidationError:
      return base::StringPrintf("Failed to validate metadata: %s",
                                error.message().c_str());
  }
}

void IsolatedWebAppResponseReaderFactory::OnIntegrityBlockRead(
    const web_package::SignedWebBundleId& web_bundle_id,
    bool skip_signature_verification,
    const web_package::SignedWebBundleIntegrityBlock integrity_block,
    base::OnceCallback<void(SignedWebBundleReader::SignatureVerificationAction)>
        integrity_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  validator_->ValidateIntegrityBlock(
      web_bundle_id, integrity_block,
      base::BindOnce(
          &IsolatedWebAppResponseReaderFactory::OnIntegrityBlockValidated,
          weak_ptr_factory_.GetWeakPtr(), skip_signature_verification,
          std::move(integrity_callback)));
}

void IsolatedWebAppResponseReaderFactory::OnIntegrityBlockValidated(
    bool skip_signature_verification,
    base::OnceCallback<void(SignedWebBundleReader::SignatureVerificationAction)>
        integrity_callback,
    absl::optional<std::string> integrity_block_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (integrity_block_error.has_value()) {
    // Aborting parsing will trigger a call to `OnIntegrityBlockAndMetadataRead`
    // with a `SignedWebBundleReader::AbortedByCaller` error.
    std::move(integrity_callback)
        .Run(SignedWebBundleReader::SignatureVerificationAction::Abort(
            *integrity_block_error));
    return;
  }

  if (skip_signature_verification) {
    std::move(integrity_callback)
        .Run(SignedWebBundleReader::SignatureVerificationAction::
                 ContinueAndSkipSignatureVerification());
  } else {
    std::move(integrity_callback)
        .Run(SignedWebBundleReader::SignatureVerificationAction::
                 ContinueAndVerifySignatures());
  }
}

void IsolatedWebAppResponseReaderFactory::OnIntegrityBlockAndMetadataRead(
    std::unique_ptr<SignedWebBundleReader> reader,
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    Callback callback,
    base::expected<void, UnusableSwbnFileError> status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.has_value()) {
    status = base::expected(validator_->ValidateMetadata(
        web_bundle_id, reader->GetPrimaryURL(), reader->GetEntries()));
  }

  UmaLogExpectedStatus("WebApp.Isolated.SwbnFileUsability", status);

  if (!status.has_value()) {
    std::move(callback).Run(base::unexpected(status.error()));
    return;
  }

  std::move(callback).Run(
      std::make_unique<IsolatedWebAppResponseReader>(std::move(reader)));
}

}  // namespace web_app
