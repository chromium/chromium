// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_signature_verifier.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

IsolatedWebAppReaderRegistry::IsolatedWebAppReaderRegistry(
    std::unique_ptr<IsolatedWebAppValidator> validator,
    base::RepeatingCallback<std::unique_ptr<SignedWebBundleSignatureVerifier>()>
        signature_verifier_factory)
    : validator_(std::move(validator)),
      signature_verifier_factory_(std::move(signature_verifier_factory)) {}

IsolatedWebAppReaderRegistry::~IsolatedWebAppReaderRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IsolatedWebAppReaderRegistry::ReadResponse(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    const network::ResourceRequest& resource_request,
    ReadResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto cache_entry_it = reader_cache_.find(web_bundle_path);
      cache_entry_it != reader_cache_.end()) {
    switch (cache_entry_it->second.state) {
      case CacheEntry::State::kPending:
        // If integrity block and metadata are still being read, then the
        // `SignedWebBundleReader` is not yet ready to be used for serving
        // responses. Queue the request and callback in this case.
        cache_entry_it->second.pending_requests.emplace_back(
            resource_request, std::move(callback));
        return;
      case CacheEntry::State::kReady:
        // If integrity block and metadata have already been read, read the
        // response from the cached `SignedWebBundleReader`.
        DoReadResponse(*cache_entry_it->second.reader, resource_request,
                       std::move(callback));
        return;
    }
  }

  std::unique_ptr<SignedWebBundleSignatureVerifier> signature_verifier =
      signature_verifier_factory_.Run();
  std::unique_ptr<SignedWebBundleReader> reader =
      SignedWebBundleReader::CreateAndStartReading(
          web_bundle_path,
          base::BindOnce(
              &IsolatedWebAppReaderRegistry::OnIntegrityBlockRead,
              // `base::Unretained` can be used here since `this` owns `reader`.
              base::Unretained(this), web_bundle_path, web_bundle_id),
          base::BindOnce(
              &IsolatedWebAppReaderRegistry::OnIntegrityBlockAndMetadataRead,
              // `base::Unretained` can be used here since `this` owns `reader`.
              base::Unretained(this), web_bundle_path, web_bundle_id),
          std::move(signature_verifier));

  auto [cache_entry_it, was_insertion] =
      reader_cache_.emplace(web_bundle_path, CacheEntry(std::move(reader)));
  DCHECK(was_insertion);
  cache_entry_it->second.pending_requests.emplace_back(resource_request,
                                                       std::move(callback));
}

void IsolatedWebAppReaderRegistry::OnIntegrityBlockRead(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::vector<web_package::Ed25519PublicKey>& public_key_stack,
    base::OnceCallback<void(SignedWebBundleReader::SignatureVerificationAction)>
        integrity_callback) {
  if (auto error =
          validator_->ValidateIntegrityBlock(web_bundle_id, public_key_stack);
      error.has_value()) {
    // Aborting parsing will trigger a call to `OnIntegrityBlockAndMetadataRead`
    // with a `SignedWebBundleReader::AbortedByCaller` error.
    std::move(integrity_callback)
        .Run(SignedWebBundleReader::SignatureVerificationAction::Abort(*error));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, we only verify integrity at install-time. On other OSes,
  // we verify integrity once per session.
  std::move(integrity_callback)
      .Run(SignedWebBundleReader::SignatureVerificationAction::
               ContinueAndSkipSignatureVerification());
#else
  std::move(integrity_callback)
      .Run(SignedWebBundleReader::SignatureVerificationAction::
               ContinueAndVerifySignatures());
#endif
}

void IsolatedWebAppReaderRegistry::OnIntegrityBlockAndMetadataRead(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    absl::optional<SignedWebBundleReader::ReadError> read_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto cache_entry_it = reader_cache_.find(web_bundle_path);
  DCHECK(cache_entry_it != reader_cache_.end());
  DCHECK_EQ(cache_entry_it->second.state, CacheEntry::State::kPending);

  // Get all pending callbacks and set the pending callbacks of the cache
  // entry to an empty vector.
  std::vector<std::pair<network::ResourceRequest, ReadResponseCallback>>
      pending_requests;
  cache_entry_it->second.pending_requests.swap(pending_requests);

  if (read_error.has_value()) {
    std::string error_message = absl::visit(
        base::Overloaded{
            [](const web_package::mojom::BundleIntegrityBlockParseErrorPtr&
                   error) {
              return base::StringPrintf("Failed to parse integrity block: %s",
                                        error->message.c_str());
            },
            [](const SignedWebBundleReader::AbortedByCaller& error) {
              return base::StringPrintf(
                  "Public keys of the Isolated Web App are untrusted: %s",
                  error.message.c_str());
            },
            [](const SignedWebBundleSignatureVerifier::Error& error) {
              return base::StringPrintf("Failed to verify signatures: %s",
                                        error.message.c_str());
            },
            [](const web_package::mojom::BundleMetadataParseErrorPtr& error) {
              return base::StringPrintf("Failed to parse metadata: %s",
                                        error->message.c_str());
            }},
        *read_error);
    for (auto& [resource_request, callback] : pending_requests) {
      std::move(callback).Run(
          base::unexpected(ReadResponseError::ForOtherError(error_message)));
    }
    reader_cache_.erase(cache_entry_it);
    return;
  }

  if (auto error = validator_->ValidateMetadata(
          web_bundle_id, cache_entry_it->second.reader->GetPrimaryURL(),
          cache_entry_it->second.reader->GetEntries());
      error.has_value()) {
    for (auto& [resource_request, callback] : pending_requests) {
      std::move(callback).Run(
          base::unexpected(ReadResponseError::ForOtherError(*error)));
    }
    reader_cache_.erase(cache_entry_it);
    return;
  }

  // The `SignedWebBundleReader` is now ready to read responses. Inform all
  // consumers that were waiting for this `SignedWebBundleReader` to become
  // available.
  cache_entry_it->second.state = CacheEntry::State::kReady;
  for (auto& [resource_request, callback] : pending_requests) {
    DoReadResponse(*cache_entry_it->second.reader, resource_request,
                   std::move(callback));
  }
}

void IsolatedWebAppReaderRegistry::DoReadResponse(
    SignedWebBundleReader& reader,
    network::ResourceRequest resource_request,
    ReadResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Remove query parameters from the request URL, if it has any.
  // Resources within Signed Web Bundles used for Isolated Web Apps never have
  // username, password, or fragment, just like resources within Signed Web
  // Bundles and normal Web Bundles. Removing these from request URLs is done by
  // the `SignedWebBundleReader`. However, in addition, resources in Signed Web
  // Bundles used for Isolated Web Apps can also never have query parameters,
  // which we need to remove here.
  //
  // Conceptually, we treat the resources in Signed Web Bundles for Isolated Web
  // Apps more like files served by a file server (which also strips query
  // parameters before looking up the file), and not like HTTP exchanges as they
  // are used for Signed Exchanges (SXG).
  if (resource_request.url.has_query()) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    resource_request.url = resource_request.url.ReplaceComponents(replacements);
  }

  reader.ReadResponse(
      resource_request,
      base::BindOnce(
          &IsolatedWebAppReaderRegistry::OnResponseRead,
          // `base::Unretained` can be used here since `this` owns `reader`.
          base::Unretained(this), std::ref(reader), std::move(callback)));
}

void IsolatedWebAppReaderRegistry::OnResponseRead(
    SignedWebBundleReader& reader,
    ReadResponseCallback callback,
    base::expected<web_package::mojom::BundleResponsePtr,
                   SignedWebBundleReader::ReadResponseError> response_head) {
  if (!response_head.has_value()) {
    switch (response_head.error().type) {
      case SignedWebBundleReader::ReadResponseError::Type::kParserInternalError:
      case SignedWebBundleReader::ReadResponseError::Type::kFormatError:
        std::move(callback).Run(
            base::unexpected(ReadResponseError::ForOtherError(
                base::StringPrintf("Failed to parse response head: %s",
                                   response_head.error().message.c_str()))));
        return;
      case SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound:
        std::move(callback).Run(
            base::unexpected(ReadResponseError::ForResponseNotFound(
                response_head.error().message)));
        return;
    }
  }
  // Since `this` owns `reader`, we only pass a weak reference to it to the
  // `Response` object. If `this` deletes `reader`, it makes sense that the
  // reference contained in `Response` also becomes invalid.
  std::move(callback).Run(
      Response(std::move(*response_head), reader.AsWeakPtr()));
}

IsolatedWebAppReaderRegistry::Response::Response(
    web_package::mojom::BundleResponsePtr head,
    base::WeakPtr<SignedWebBundleReader> reader)
    : head_(std::move(head)), reader_(std::move(reader)) {}

IsolatedWebAppReaderRegistry::Response::Response(Response&&) = default;

IsolatedWebAppReaderRegistry::Response&
IsolatedWebAppReaderRegistry::Response::operator=(Response&&) = default;

IsolatedWebAppReaderRegistry::Response::~Response() = default;

void IsolatedWebAppReaderRegistry::Response::ReadBody(
    mojo::ScopedDataPipeProducerHandle producer_handle,
    base::OnceCallback<void(net::Error net_error)> callback) {
  if (!reader_) {
    // The weak pointer to `reader_` might no longer be valid when this is
    // called.
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  reader_->ReadResponseBody(head_->Clone(), std::move(producer_handle),
                            std::move(callback));
}

IsolatedWebAppReaderRegistry::CacheEntry::CacheEntry(
    std::unique_ptr<SignedWebBundleReader> reader)
    : reader(std::move(reader)) {}

IsolatedWebAppReaderRegistry::CacheEntry::~CacheEntry() = default;

IsolatedWebAppReaderRegistry::CacheEntry::CacheEntry(CacheEntry&& other) =
    default;

IsolatedWebAppReaderRegistry::CacheEntry&
IsolatedWebAppReaderRegistry::CacheEntry::operator=(CacheEntry&& other) =
    default;

}  // namespace web_app
