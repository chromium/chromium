// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_FACTORY_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_package {
class SignedWebBundleId;
class SignedWebBundleIntegrityBlock;
}  // namespace web_package

namespace web_app {

class IsolatedWebAppValidator;

// Simple struct used to represent errors related to the integrity block of the
// Signed Web Bundle.
struct IntegrityBlockError {
  explicit IntegrityBlockError(std::string message)
      : message(std::move(message)) {}

  std::string message;
};

// Simple struct used to represent errors related to the metadata of the Signed
// Web Bundle.
struct MetadataError {
  explicit MetadataError(std::string message) : message(std::move(message)) {}

  std::string message;
};

// Factory for creating instances of `IsolatedWebAppResponseReader` that are
// ready to read responses from the bundle. Instances returned by this class are
// guaranteed to have previously read a valid integrity block and metadata, as
// well as to have verified that the signatures are valid (unless
// `skip_signature_verification` is set).
class IsolatedWebAppResponseReaderFactory {
 public:
  explicit IsolatedWebAppResponseReaderFactory(
      std::unique_ptr<IsolatedWebAppValidator> validator,
      base::RepeatingCallback<
          std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>()>
          signature_verifier_factory = base::BindRepeating([]() {
            return std::make_unique<
                web_package::SignedWebBundleSignatureVerifier>();
          }));
  virtual ~IsolatedWebAppResponseReaderFactory();

  IsolatedWebAppResponseReaderFactory(
      const IsolatedWebAppResponseReaderFactory&) = delete;
  IsolatedWebAppResponseReaderFactory& operator=(
      const IsolatedWebAppResponseReaderFactory&) = delete;

  using Error = absl::variant<
      // Triggered when the integrity block of the Signed Web
      // Bundle does not exist or parsing it fails.
      web_package::mojom::BundleIntegrityBlockParseErrorPtr,
      // Triggered when the integrity block is not valid for this Isolated Web
      // App or when the user agent does not trust the Isolated Web App.
      IntegrityBlockError,
      // Triggered when signature verification fails.
      web_package::SignedWebBundleSignatureVerifier::Error,
      // Triggered when metadata parsing fails.
      web_package::mojom::BundleMetadataParseErrorPtr,
      // Triggered when the metadata is not valid for this Isolated Web App.
      MetadataError>;

  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<IsolatedWebAppResponseReader>, Error>)>;

  virtual void CreateResponseReader(
      const base::FilePath& web_bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      bool skip_signature_verification,
      Callback callback);

  static std::string ErrorToString(const Error& error);

  // This enum represents every error type that can occur during integrity block
  // and metadata parsing, before responses are read from Signed Web Bundles.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReadIntegrityBlockAndMetadataStatus {
    kSuccess = 0,
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

 private:
  void OnIntegrityBlockRead(
      const web_package::SignedWebBundleId& web_bundle_id,
      bool skip_signature_verification,
      const web_package::SignedWebBundleIntegrityBlock integrity_block,
      base::OnceCallback<
          void(SignedWebBundleReader::SignatureVerificationAction)> callback);

  void OnIntegrityBlockValidated(
      bool skip_signature_verification,
      base::OnceCallback<
          void(SignedWebBundleReader::SignatureVerificationAction)>
          integrity_callback,
      absl::optional<std::string> integrity_block_error);

  void OnIntegrityBlockAndMetadataRead(
      std::unique_ptr<SignedWebBundleReader> reader,
      const base::FilePath& web_bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      Callback callback,
      absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>
          read_integrity_block_and_metadata_error);

  ReadIntegrityBlockAndMetadataStatus GetStatusFromError(
      const SignedWebBundleReader::ReadIntegrityBlockAndMetadataError& error);

  std::unique_ptr<IsolatedWebAppValidator> validator_;
  base::RepeatingCallback<
      std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>()>
      signature_verifier_factory_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IsolatedWebAppResponseReaderFactory> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_FACTORY_H_
