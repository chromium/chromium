// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"

#include <memory>
#include <string>

#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
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
    const Error& error) {
  return (absl::visit(
      base::Overloaded{
          [](const web_package::mojom::BundleIntegrityBlockParseErrorPtr&
                 error) {
            return base::StringPrintf("Failed to parse integrity block: %s",
                                      error->message.c_str());
          },
          [](const IntegrityBlockError& error) {
            return base::StringPrintf("Failed to validate integrity block: %s",
                                      error.message.c_str());
          },
          [](const web_package::SignedWebBundleSignatureVerifier::Error&
                 error) {
            return base::StringPrintf("Failed to verify signatures: %s",
                                      error.message.c_str());
          },
          [](const web_package::mojom::BundleMetadataParseErrorPtr& error) {
            return base::StringPrintf("Failed to parse metadata: %s",
                                      error->message.c_str());
          },
          [](const MetadataError& error) {
            return base::StringPrintf("Failed to validate metadata: %s",
                                      error.message.c_str());
          }},
      error));
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
    absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>
        read_integrity_block_and_metadata_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  absl::optional<std::pair<Error, ReadIntegrityBlockAndMetadataStatus>>
      error_and_status;
  if (read_integrity_block_and_metadata_error.has_value()) {
    Error error = absl::visit(
        base::Overloaded{
            [](const web_package::mojom::BundleIntegrityBlockParseErrorPtr&
                   error) -> Error { return error->Clone(); },
            [](const SignedWebBundleReader::AbortedByCaller& error) -> Error {
              return IntegrityBlockError(error.message);
            },
            [](const web_package::SignedWebBundleSignatureVerifier::Error&
                   error) -> Error { return error; },
            [](const web_package::mojom::BundleMetadataParseErrorPtr& error)
                -> Error { return error->Clone(); },
        },
        *read_integrity_block_and_metadata_error);
    error_and_status = std::make_pair(
        std::move(error),
        GetStatusFromError(*read_integrity_block_and_metadata_error));
  }

  if (!error_and_status.has_value()) {
    if (auto error_message = validator_->ValidateMetadata(
            web_bundle_id, reader->GetPrimaryURL(), reader->GetEntries());
        error_message.has_value()) {
      error_and_status = std::make_pair(
          MetadataError(*error_message),
          ReadIntegrityBlockAndMetadataStatus::kMetadataValidationError);
    }
  }

  base::UmaHistogramEnumeration(
      "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus",
      error_and_status.has_value()
          ? error_and_status->second
          : ReadIntegrityBlockAndMetadataStatus::kSuccess);

  if (error_and_status.has_value()) {
    std::move(callback).Run(
        base::unexpected(std::move(error_and_status->first)));
    return;
  }
  std::move(callback).Run(
      std::make_unique<IsolatedWebAppResponseReader>(std::move(reader)));
}

IsolatedWebAppResponseReaderFactory::ReadIntegrityBlockAndMetadataStatus
IsolatedWebAppResponseReaderFactory::GetStatusFromError(
    const SignedWebBundleReader::ReadIntegrityBlockAndMetadataError& error) {
  return absl::visit(
      base::Overloaded{
          [](const web_package::mojom::BundleIntegrityBlockParseErrorPtr&
                 error) {
            switch (error->type) {
              case web_package::mojom::BundleParseErrorType::
                  kParserInternalError:
                return ReadIntegrityBlockAndMetadataStatus::
                    kIntegrityBlockParserInternalError;
              case web_package::mojom::BundleParseErrorType::kFormatError:
                return ReadIntegrityBlockAndMetadataStatus::
                    kIntegrityBlockParserFormatError;
              case web_package::mojom::BundleParseErrorType::kVersionError:
                return ReadIntegrityBlockAndMetadataStatus::
                    kIntegrityBlockParserVersionError;
            }
          },
          [](const SignedWebBundleReader::AbortedByCaller& error) {
            return ReadIntegrityBlockAndMetadataStatus::
                kIntegrityBlockValidationError;
          },
          [](const web_package::SignedWebBundleSignatureVerifier::Error&
                 error) {
            return ReadIntegrityBlockAndMetadataStatus::
                kSignatureVerificationError;
          },
          [](const web_package::mojom::BundleMetadataParseErrorPtr& error) {
            switch (error->type) {
              case web_package::mojom::BundleParseErrorType::
                  kParserInternalError:
                return ReadIntegrityBlockAndMetadataStatus::
                    kMetadataParserInternalError;
              case web_package::mojom::BundleParseErrorType::kFormatError:
                return ReadIntegrityBlockAndMetadataStatus::
                    kMetadataParserFormatError;
              case web_package::mojom::BundleParseErrorType::kVersionError:
                return ReadIntegrityBlockAndMetadataStatus::
                    kMetadataParserVersionError;
            }
          }},
      error);
}

}  // namespace web_app
