// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

IsolatedWebAppResponseReaderFactory::IsolatedWebAppResponseReaderFactory(
    Profile& profile,
    std::unique_ptr<IsolatedWebAppValidator> validator,
    base::RepeatingCallback<
        std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>()>
        signature_verifier_factory)
    : profile_(profile),
      trust_checker_(profile),
      validator_(std::move(validator)),
      signature_verifier_factory_(std::move(signature_verifier_factory)) {}

IsolatedWebAppResponseReaderFactory::~IsolatedWebAppResponseReaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IsolatedWebAppResponseReaderFactory::CreateResponseReader(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    Flags flags,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!web_bundle_id.is_for_proxy_mode());

  GURL base_url(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    web_bundle_id.id()}));

  std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
      signature_verifier = signature_verifier_factory_.Run();
  std::unique_ptr<SignedWebBundleReader> reader = SignedWebBundleReader::Create(
      web_bundle_path, std::move(base_url), std::move(signature_verifier));

  SignedWebBundleReader& reader_ref = *reader.get();
  reader_ref.ReadIntegrityBlock(base::BindOnce(
      &IsolatedWebAppResponseReaderFactory::OnIntegrityBlockRead,
      weak_ptr_factory_.GetWeakPtr(), std::move(reader), web_bundle_path,
      web_bundle_id, flags, std::move(callback)));
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
    std::unique_ptr<SignedWebBundleReader> reader,
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    Flags flags,
    Callback callback,
    base::expected<web_package::SignedWebBundleIntegrityBlock,
                   UnusableSwbnFileError> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* reader_ptr = reader.get();
  auto proceed_callback = base::BindOnce(
      &IsolatedWebAppResponseReaderFactory::OnIntegrityBlockAndMetadataRead,
      weak_ptr_factory_.GetWeakPtr(), std::move(reader), web_bundle_path,
      web_bundle_id, flags, std::move(callback));

  ASSIGN_OR_RETURN(
      auto integrity_block, std::move(result),
      [&](UnusableSwbnFileError error) {
        // Aborting parsing will trigger a call to
        // `OnIntegrityBlockAndMetadataRead` with a
        // `SignedWebBundleReader::AbortedByCaller` error.
        reader_ptr->ProceedWithAction(
            SignedWebBundleReader::SignatureVerificationAction::Abort(
                std::move(error)),
            std::move(proceed_callback));
      });

  RETURN_IF_ERROR(
      validator_->ValidateIntegrityBlock(web_bundle_id, integrity_block,
                                         flags.Has(Flag::kDevModeBundle),
                                         trust_checker_),
      [&](std::string error) {
        // Aborting parsing will trigger a call to
        // `OnIntegrityBlockAndMetadataRead` with a
        // `SignedWebBundleReader::AbortedByCaller` error.
        reader_ptr->ProceedWithAction(
            SignedWebBundleReader::SignatureVerificationAction::Abort(
                UnusableSwbnFileError(UnusableSwbnFileError::Error::
                                          kIntegrityBlockValidationError,
                                      std::move(error))),
            std::move(proceed_callback));
      });

  reader_ptr->ProceedWithAction(
      flags.Has(Flag::kSkipSignatureVerification)
          ? SignedWebBundleReader::SignatureVerificationAction::
                ContinueAndSkipSignatureVerification()
          : SignedWebBundleReader::SignatureVerificationAction::
                ContinueAndVerifySignatures(),
      std::move(proceed_callback));
}

void IsolatedWebAppResponseReaderFactory::OnIntegrityBlockAndMetadataRead(
    std::unique_ptr<SignedWebBundleReader> reader,
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    Flags flags,
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

  std::move(callback).Run(std::make_unique<IsolatedWebAppResponseReaderImpl>(
      std::move(reader),
      base::BindRepeating(
          &IsolatedWebAppTrustChecker::IsTrusted,
          // Do not re-use `trust_checker_` here, because
          // `IsolatedWebAppResponseReaderImpl` might outlive `this`.
          std::make_unique<IsolatedWebAppTrustChecker>(*profile_),
          web_bundle_id,
          /*is_dev_mode_bundle=*/flags.Has(Flag::kDevModeBundle))));
}

}  // namespace web_app
