// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "services/network/public/cpp/resource_request.h"
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
    : reader_factory_(std::make_unique<IsolatedWebAppResponseReaderFactory>(
          std::move(validator),
          std::move(signature_verifier_factory))) {}

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
      switch (cache_entry_it->second.state()) {
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

  auto [cache_entry_it, was_insertion] =
      reader_cache_.Emplace(web_bundle_path, Cache::Entry());
  DCHECK(was_insertion);
  cache_entry_it->second.pending_requests.emplace_back(resource_request,
                                                       std::move(callback));

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, signatures are only verified at install-time. The location of
  // the installed bundles inside of cryptohome is deemed secure enough to not
  // necessitate re-verification of signatures once per session.
  bool skip_signature_verification = true;
#else
  // If we already verified the signatures of this Signed Web Bundle during
  // the current browser session, we trust that the Signed Web Bundle has not
  // been tampered with and don't re-verify signatures.
  bool skip_signature_verification = verified_files_.contains(web_bundle_path);
#endif

  reader_factory_->CreateResponseReader(
      web_bundle_path, web_bundle_id, skip_signature_verification,
      base::BindOnce(&IsolatedWebAppReaderRegistry::OnResponseReaderCreated,
                     // `base::Unretained` can be used here since `this` owns
                     // `reader_factory`.
                     base::Unretained(this), web_bundle_path, web_bundle_id));
}

void IsolatedWebAppReaderRegistry::OnResponseReaderCreated(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                   IsolatedWebAppResponseReaderFactory::Error> reader) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto cache_entry_it = reader_cache_.Find(web_bundle_path);
  DCHECK(cache_entry_it != reader_cache_.End());
  DCHECK_EQ(cache_entry_it->second.state(), Cache::Entry::State::kPending);

  std::vector<std::pair<network::ResourceRequest, ReadResponseCallback>>
      pending_requests =
          std::exchange(cache_entry_it->second.pending_requests, {});

  if (!reader.has_value()) {
    for (auto& [resource_request, callback] : pending_requests) {
      std::move(callback).Run(
          base::unexpected(ReadResponseError::ForError(reader.error())));
    }
    reader_cache_.Erase(cache_entry_it);
    return;
  }

  // The `SignedWebBundleReader` is now ready to read responses. Inform all
  // consumers that were waiting for this `SignedWebBundleReader` to become
  // available.
  verified_files_.insert(cache_entry_it->first);
  cache_entry_it->second.set_reader(std::move(*reader));
  for (auto& [resource_request, callback] : pending_requests) {
    DoReadResponse(cache_entry_it->second.GetReader(), resource_request,
                   std::move(callback));
  }
}

void IsolatedWebAppReaderRegistry::DoReadResponse(
    IsolatedWebAppResponseReader& reader,
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
          base::Unretained(this), std::move(callback)));
}

void IsolatedWebAppReaderRegistry::OnResponseRead(
    ReadResponseCallback callback,
    base::expected<IsolatedWebAppResponseReader::Response,
                   IsolatedWebAppResponseReader::Error> response) {
  base::UmaHistogramEnumeration("WebApp.Isolated.ReadResponseHeadStatus",
                                response.has_value()
                                    ? ReadResponseHeadStatus::kSuccess
                                    : GetStatusFromError(response.error()));

  if (!response.has_value()) {
    std::move(callback).Run(
        base::unexpected(ReadResponseError::ForError(response.error())));
    return;
  }
  std::move(callback).Run(std::move(*response));
}

IsolatedWebAppReaderRegistry::ReadResponseHeadStatus
IsolatedWebAppReaderRegistry::GetStatusFromError(
    const IsolatedWebAppResponseReader::Error& error) {
  switch (error.type) {
    case IsolatedWebAppResponseReader::Error::Type::kParserInternalError:
      return ReadResponseHeadStatus::kResponseHeadParserInternalError;
    case IsolatedWebAppResponseReader::Error::Type::kFormatError:
      return ReadResponseHeadStatus::kResponseHeadParserFormatError;
    case IsolatedWebAppResponseReader::Error::Type::kResponseNotFound:
      return ReadResponseHeadStatus::kResponseNotFoundError;
  }
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const IsolatedWebAppResponseReaderFactory::Error& error) {
  return ForOtherError(
      IsolatedWebAppResponseReaderFactory::ErrorToString(error));
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const IsolatedWebAppResponseReader::Error& error) {
  switch (error.type) {
    case IsolatedWebAppResponseReader::Error::Type::kParserInternalError:
      return ForOtherError(base::StringPrintf(
          "Failed to parse response head: %s", error.message.c_str()));
    case IsolatedWebAppResponseReader::Error::Type::kFormatError:
      return ForOtherError(base::StringPrintf(
          "Failed to parse response head: %s", error.message.c_str()));
    case IsolatedWebAppResponseReader::Error::Type::kResponseNotFound:
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
            return cache_entry.state() == Entry::State::kReady &&
                   now - cache_entry.last_access() > kCleanupInterval;
          },
          [](const std::pair<base::FilePath, Entry>& entry) -> const Entry& {
            return entry.second;
          }),
      cache_.end());
  StopCleanupTimerIfCacheIsEmpty();
}

IsolatedWebAppReaderRegistry::Cache::Entry::Entry() = default;

IsolatedWebAppReaderRegistry::Cache::Entry::~Entry() = default;

IsolatedWebAppReaderRegistry::Cache::Entry::Entry(Entry&& other) = default;

IsolatedWebAppReaderRegistry::Cache::Entry&
IsolatedWebAppReaderRegistry::Cache::Entry::operator=(Entry&& other) = default;

}  // namespace web_app
