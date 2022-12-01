// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

// References to `SignedWebBundleReader`s that are not used for the provided
// time interval will be removed from the cache. This is important so that the
// cache doesn't grow forever, given that each `SignedWebBundleReader` requires
// some memory and an open file handle.
//
// Note: Depending on when during the interval a new `SignedWebBundleReader` is
// accessed, the worst-case time until it is cleaned up can be up to two times
// `kCleanupInterval`, since the logic for cleaning up `SignedWebBundleReader`s
// is as follows: Every `kCleanupInterval`, remove references to all
// `SignedWebBundleReader`s that haven't been accessed for at least
// `kCleanupInterval`.
// We could run a separate timer per `SignedWebBundleReader` to more accurately
// respect `kCleanupInterval`, but this feels like unnecessary overhead.
base::TimeDelta kCleanupInterval = base::Minutes(10);

}  // namespace

IsolatedWebAppReaderRegistry::IsolatedWebAppReaderRegistry(
    std::unique_ptr<IsolatedWebAppValidator> validator,
    base::RepeatingCallback<
        std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>()>
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
  DCHECK_EQ(web_bundle_id.type(),
            web_package::SignedWebBundleId::Type::kEd25519PublicKey);

  {
    auto cache_entry_it = reader_cache_.Find(web_bundle_path);
    bool found = cache_entry_it != reader_cache_.End();

    base::UmaHistogramEnumeration(
        "WebApp.Isolated.ResponseReaderCacheState",
        found ? cache_entry_it->second.AsReaderCacheState()
              : ReaderCacheState::kNotCached);

    if (found) {
      switch (cache_entry_it->second.state) {
        case Cache::Entry::State::kPending:
          // If integrity block and metadata are still being read, then the
          // `SignedWebBundleReader` is not yet ready to be used for serving
          // responses. Queue the request and callback in this case.
          cache_entry_it->second.pending_requests.emplace_back(
              resource_request, std::move(callback));
          return;
        case Cache::Entry::State::kReady:
          // If integrity block and metadata have already been read, read
          // the response from the cached `SignedWebBundleReader`.
          DoReadResponse(cache_entry_it->second.GetReader(), resource_request,
                         std::move(callback));
          return;
      }
    }
  }

  GURL base_url(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    web_bundle_id.id()}));

  std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
      signature_verifier = signature_verifier_factory_.Run();
  std::unique_ptr<SignedWebBundleReader> reader = SignedWebBundleReader::Create(
      web_bundle_path, base_url, std::move(signature_verifier));

  auto [cache_entry_it, was_insertion] =
      reader_cache_.Emplace(web_bundle_path, Cache::Entry(std::move(reader)));
  DCHECK(was_insertion);
  cache_entry_it->second.pending_requests.emplace_back(resource_request,
                                                       std::move(callback));

  cache_entry_it->second.GetReader().StartReading(
      base::BindOnce(
          &IsolatedWebAppReaderRegistry::OnIntegrityBlockRead,
          // `base::Unretained` can be used here since `this` owns `reader`.
          base::Unretained(this), web_bundle_path, web_bundle_id),
      base::BindOnce(
          &IsolatedWebAppReaderRegistry::OnIntegrityBlockAndMetadataRead,
          // `base::Unretained` can be used here since `this` owns `reader`.
          base::Unretained(this), web_bundle_path, web_bundle_id));
}

void IsolatedWebAppReaderRegistry::OnIntegrityBlockRead(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::vector<web_package::Ed25519PublicKey>& public_key_stack,
    base::OnceCallback<void(SignedWebBundleReader::SignatureVerificationAction)>
        integrity_callback) {
  validator_->ValidateIntegrityBlock(
      web_bundle_id, public_key_stack,
      base::BindOnce(&IsolatedWebAppReaderRegistry::OnIntegrityBlockValidated,
                     weak_ptr_factory_.GetWeakPtr(), web_bundle_path,
                     web_bundle_id, std::move(integrity_callback)));
}

void IsolatedWebAppReaderRegistry::OnIntegrityBlockValidated(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::OnceCallback<void(SignedWebBundleReader::SignatureVerificationAction)>
        integrity_callback,
    absl::optional<std::string> integrity_block_error) {
  if (integrity_block_error.has_value()) {
    // Aborting parsing will trigger a call to `OnIntegrityBlockAndMetadataRead`
    // with a `SignedWebBundleReader::AbortedByCaller` error.
    std::move(integrity_callback)
        .Run(SignedWebBundleReader::SignatureVerificationAction::Abort(
            *integrity_block_error));
    return;
  }

  // TODO(crbug.com/1366309): On ChromeOS, we should only verify signatures at
  // install-time. Until this is implemented, we will verify signatures on
  // ChromeOS once per session.
  if (verified_files_.contains(web_bundle_path)) {
    // If we already verified the signatures of this Signed Web Bundle during
    // the current browser session, we trust that the Signed Web Bundle has not
    // been tampered with and don't re-verify signatures.
    std::move(integrity_callback)
        .Run(SignedWebBundleReader::SignatureVerificationAction::
                 ContinueAndSkipSignatureVerification());
  } else {
    std::move(integrity_callback)
        .Run(SignedWebBundleReader::SignatureVerificationAction::
                 ContinueAndVerifySignatures());
  }
}

void IsolatedWebAppReaderRegistry::OnIntegrityBlockAndMetadataRead(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    absl::optional<SignedWebBundleReader::ReadIntegrityBlockAndMetadataError>
        read_integrity_block_and_metadata_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto cache_entry_it = reader_cache_.Find(web_bundle_path);
  DCHECK(cache_entry_it != reader_cache_.End());
  DCHECK_EQ(cache_entry_it->second.state, Cache::Entry::State::kPending);

  absl::optional<
      std::pair<ReadResponseError, ReadIntegrityBlockAndMetadataStatus>>
      error_and_status;
  if (read_integrity_block_and_metadata_error.has_value()) {
    error_and_status = std::make_pair(
        ReadResponseError::ForError(*read_integrity_block_and_metadata_error),
        GetStatusFromError(*read_integrity_block_and_metadata_error));
  }

  SignedWebBundleReader& reader = cache_entry_it->second.GetReader();

  if (!error_and_status.has_value()) {
    if (auto error_message = validator_->ValidateMetadata(
            web_bundle_id, reader.GetPrimaryURL(), reader.GetEntries());
        error_message.has_value()) {
      error_and_status = std::make_pair(
          ReadResponseError::ForMetadataValidationError(*error_message),
          ReadIntegrityBlockAndMetadataStatus::kMetadataValidationError);
    }
  }

  base::UmaHistogramEnumeration(
      "WebApp.Isolated.ReadIntegrityBlockAndMetadataStatus",
      error_and_status.has_value()
          ? error_and_status->second
          : ReadIntegrityBlockAndMetadataStatus::kSuccess);

  std::vector<std::pair<network::ResourceRequest, ReadResponseCallback>>
      pending_requests =
          std::exchange(cache_entry_it->second.pending_requests, {});

  if (error_and_status.has_value()) {
    for (auto& [resource_request, callback] : pending_requests) {
      std::move(callback).Run(base::unexpected(error_and_status->first));
    }
    reader_cache_.Erase(cache_entry_it);
    return;
  }

  // The `SignedWebBundleReader` is now ready to read responses. Inform all
  // consumers that were waiting for this `SignedWebBundleReader` to become
  // available.
  verified_files_.insert(cache_entry_it->first);
  cache_entry_it->second.state = Cache::Entry::State::kReady;
  for (auto& [resource_request, callback] : pending_requests) {
    DoReadResponse(reader, resource_request, std::move(callback));
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
          base::Unretained(this), reader.AsWeakPtr(), std::move(callback)));
}

void IsolatedWebAppReaderRegistry::OnResponseRead(
    base::WeakPtr<SignedWebBundleReader> reader,
    ReadResponseCallback callback,
    base::expected<web_package::mojom::BundleResponsePtr,
                   SignedWebBundleReader::ReadResponseError> response_head) {
  base::UmaHistogramEnumeration(
      "WebApp.Isolated.ReadResponseHeadStatus",
      response_head.has_value() ? ReadResponseHeadStatus::kSuccess
                                : GetStatusFromError(response_head.error()));

  if (!response_head.has_value()) {
    std::move(callback).Run(
        base::unexpected(ReadResponseError::ForError(response_head.error())));
    return;
  }
  // Since `this` owns `reader`, we only pass a weak reference to it to the
  // `Response` object. If `this` deletes `reader`, it makes sense that the
  // reference contained in `Response` also becomes invalid.
  std::move(callback).Run(Response(std::move(*response_head), reader));
}

IsolatedWebAppReaderRegistry::ReadIntegrityBlockAndMetadataStatus
IsolatedWebAppReaderRegistry::GetStatusFromError(
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

IsolatedWebAppReaderRegistry::ReadResponseHeadStatus
IsolatedWebAppReaderRegistry::GetStatusFromError(
    const SignedWebBundleReader::ReadResponseError& error) {
  switch (error.type) {
    case SignedWebBundleReader::ReadResponseError::Type::kParserInternalError:
      return ReadResponseHeadStatus::kResponseHeadParserInternalError;
    case SignedWebBundleReader::ReadResponseError::Type::kFormatError:
      return ReadResponseHeadStatus::kResponseHeadParserFormatError;
    case SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound:
      return ReadResponseHeadStatus::kResponseNotFoundError;
  }
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

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const SignedWebBundleReader::ReadIntegrityBlockAndMetadataError& error) {
  return ForOtherError(absl::visit(
      base::Overloaded{
          [](const web_package::mojom::BundleIntegrityBlockParseErrorPtr&
                 error) {
            return base::StringPrintf("Failed to parse integrity block: %s",
                                      error->message.c_str());
          },
          [](const SignedWebBundleReader::AbortedByCaller& error) {
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
          }},
      error));
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForMetadataValidationError(
    const std::string& error) {
  return ForOtherError(
      base::StringPrintf("Failed to validate metadata: %s", error.c_str()));
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const SignedWebBundleReader::ReadResponseError& error) {
  switch (error.type) {
    case SignedWebBundleReader::ReadResponseError::Type::kParserInternalError:
      return ForOtherError(base::StringPrintf(
          "Failed to parse response head: %s", error.message.c_str()));
    case SignedWebBundleReader::ReadResponseError::Type::kFormatError:
      return ForOtherError(base::StringPrintf(
          "Failed to parse response head: %s", error.message.c_str()));
    case SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound:
      return ForResponseNotFound(base::StringPrintf(
          "Failed to read response: %s", error.message.c_str()));
  }
}

IsolatedWebAppReaderRegistry::Cache::Cache() = default;
IsolatedWebAppReaderRegistry::Cache::~Cache() = default;

base::flat_map<base::FilePath,
               IsolatedWebAppReaderRegistry::Cache::Entry>::iterator
IsolatedWebAppReaderRegistry::Cache::Find(const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cache_.find(file_path);
}

base::flat_map<base::FilePath,
               IsolatedWebAppReaderRegistry::Cache::Entry>::iterator
IsolatedWebAppReaderRegistry::Cache::End() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cache_.end();
}

template <class... Args>
std::pair<base::flat_map<base::FilePath,
                         IsolatedWebAppReaderRegistry::Cache::Entry>::iterator,
          bool>
IsolatedWebAppReaderRegistry::Cache::Emplace(Args&&... args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = cache_.emplace(std::forward<Args>(args)...);
  StartCleanupTimerIfNotRunning();
  return result;
}

void IsolatedWebAppReaderRegistry::Cache::Erase(
    base::flat_map<base::FilePath, Entry>::iterator iterator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cache_.erase(iterator);
  StopCleanupTimerIfCacheIsEmpty();
}

void IsolatedWebAppReaderRegistry::Cache::StartCleanupTimerIfNotRunning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!cache_.empty());
  if (cleanup_timer_.IsRunning()) {
    return;
  }
  cleanup_timer_.Start(
      FROM_HERE, kCleanupInterval,
      base::BindRepeating(&Cache::CleanupOldEntries,
                          // It is safe to use `base::Unretained` here, because
                          // `cache_cleanup_timer_` will be deleted before
                          // `this` is deleted.
                          base::Unretained(this)));
}

void IsolatedWebAppReaderRegistry::Cache::StopCleanupTimerIfCacheIsEmpty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cache_.empty()) {
    cleanup_timer_.AbandonAndStop();
  }
}

void IsolatedWebAppReaderRegistry::Cache::CleanupOldEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();
  cache_.erase(
      base::ranges::remove_if(
          cache_,
          [&now](const Entry& cache_entry) -> bool {
            // If a `SignedWebBundleReader` is ready to read responses and has
            // not been used for at least `kCleanupInterval`, remove it from the
            // cache.
            return cache_entry.state == Entry::State::kReady &&
                   now - cache_entry.last_access() > kCleanupInterval;
          },
          [](const std::pair<base::FilePath, Entry>& entry) -> const Entry& {
            return entry.second;
          }),
      cache_.end());
  StopCleanupTimerIfCacheIsEmpty();
}

IsolatedWebAppReaderRegistry::Cache::Entry::Entry(
    std::unique_ptr<SignedWebBundleReader> reader)
    : reader_(std::move(reader)) {}

IsolatedWebAppReaderRegistry::Cache::Entry::~Entry() = default;

IsolatedWebAppReaderRegistry::Cache::Entry::Entry(Entry&& other) = default;

IsolatedWebAppReaderRegistry::Cache::Entry&
IsolatedWebAppReaderRegistry::Cache::Entry::operator=(Entry&& other) = default;

}  // namespace web_app
