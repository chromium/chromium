// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
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

base::expected<void, IsolatedWebAppReaderRegistry::ReadResponseHeadError>
ToReadResponseHeadError(
    const base::expected<IsolatedWebAppResponseReader::Response,
                         IsolatedWebAppResponseReader::Error>& response) {
  if (response.has_value()) {
    return base::ok();
  }
  switch (response.error().type) {
    case IsolatedWebAppResponseReader::Error::Type::kParserInternalError:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::
              kResponseHeadParserInternalError);
    case IsolatedWebAppResponseReader::Error::Type::kFormatError:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::
              kResponseHeadParserFormatError);
    case IsolatedWebAppResponseReader::Error::Type::kResponseNotFound:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::
              kResponseNotFoundError);
    case IsolatedWebAppResponseReader::Error::Type::kNotTrusted:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::kAppNotTrusted);
  }
}

void CloseReader(std::unique_ptr<IsolatedWebAppResponseReader> reader,
                 base::OnceClosure callback) {
  IsolatedWebAppResponseReader* raw_reader = reader.get();
  base::OnceClosure delete_callback =
      base::DoNothingWithBoundArgs(std::move(reader));
  raw_reader->Close(std::move(callback).Then(std::move(delete_callback)));
}

}  // namespace

IsolatedWebAppReaderRegistry::IsolatedWebAppReaderRegistry(
    Profile& profile,
    std::unique_ptr<IsolatedWebAppResponseReaderFactory> reader_factory)
    : profile_(profile), reader_factory_(std::move(reader_factory)) {
  key_distribution_info_observation_.Observe(
      IwaKeyDistributionInfoProvider::GetInstance());
}

IsolatedWebAppReaderRegistry::~IsolatedWebAppReaderRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IsolatedWebAppReaderRegistry::ReadResponse(
    const base::FilePath& web_bundle_path,
    bool dev_mode,
    const web_package::SignedWebBundleId& web_bundle_id,
    const network::ResourceRequest& resource_request,
    ReadResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!web_bundle_id.is_for_proxy_mode());

  Cache::Key cache_key{.path = web_bundle_path, .dev_mode = dev_mode};

  {
    auto cache_entry_it = reader_cache_.Find(cache_key);
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
      reader_cache_.Emplace(cache_key, Cache::Entry());
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

  IsolatedWebAppResponseReaderFactory::Flags flags;
  if (dev_mode) {
    flags.Put(IsolatedWebAppResponseReaderFactory::Flag::kDevModeBundle);
  }
  if (skip_signature_verification) {
    flags.Put(
        IsolatedWebAppResponseReaderFactory::Flag::kSkipSignatureVerification);
  }

  reader_factory_->CreateResponseReader(
      web_bundle_path, web_bundle_id, flags,
      base::BindOnce(&IsolatedWebAppReaderRegistry::OnResponseReaderCreated,
                     // `base::Unretained` can be used here since `this` owns
                     // `reader_factory`.
                     base::Unretained(this), web_bundle_path, dev_mode,
                     web_bundle_id));
}

// Processes a component update event and queues close requests for readers
// corresponding to bundles that might be affected by key rotation. These
// requests will be fulfilled once the app closes.
void IsolatedWebAppReaderRegistry::OnComponentUpdateSuccess(
    const base::Version& component_version) {
  auto* provider = WebAppProvider::GetForWebApps(&profile_.get());
  base::flat_map<web_package::SignedWebBundleId,
                 std::reference_wrapper<const WebApp>>
      installed_iwas = GetInstalledIwas(provider->registrar_unsafe());

  for (const auto& [web_bundle_id, iwa] : installed_iwas) {
    const auto& isolation_data = *iwa.get().isolation_data();
    auto result = LookupRotatedKey(web_bundle_id);
    switch (result) {
      case KeyRotationLookupResult::kNoKeyRotation:
        continue;
      case KeyRotationLookupResult::kKeyBlocked:
        break;
      case KeyRotationLookupResult::kKeyFound: {
        if (GetKeyRotationData(web_bundle_id, isolation_data)
                .current_installation_has_rk) {
          continue;
        }
      } break;
    }

    auto iwa_source = IwaSourceWithMode::FromStorageLocation(
        profile_->GetPath(), isolation_data.location());
    WebAppUiManager& ui_manager = provider->ui_manager();
    absl::visit(base::Overloaded{
                    [&](const IwaSourceBundle& bundle) {
                      const auto& app_id = iwa.get().app_id();
                      if (ui_manager.GetNumWindowsForApp(app_id) == 0) {
                        ClearCacheForPath(bundle.path(), base::DoNothing());
                        return;
                      }
                      ui_manager.NotifyOnAllAppWindowsClosed(
                          app_id,
                          base::BindOnce(
                              &IsolatedWebAppReaderRegistry::ClearCacheForPath,
                              weak_ptr_factory_.GetWeakPtr(), bundle.path(),
                              base::DoNothing()));
                    },
                    [](const IwaSourceProxy&) {}},
                iwa_source.variant());
  }
}

void IsolatedWebAppReaderRegistry::ClearCacheForPath(
    const base::FilePath& web_bundle_path,
    base::OnceClosure callback) {
  auto callbacks = base::BarrierClosure(2, std::move(callback));
  ClearCacheForPathImpl(web_bundle_path, /*dev_mode=*/false, callbacks);
  ClearCacheForPathImpl(web_bundle_path, /*dev_mode=*/true, callbacks);
}

void IsolatedWebAppReaderRegistry::ClearCacheForPathImpl(
    const base::FilePath& web_bundle_path,
    bool dev_mode,
    base::OnceClosure callback) {
  auto cache_entry_it =
      reader_cache_.Find({.path = web_bundle_path, .dev_mode = dev_mode});
  const bool found = cache_entry_it != reader_cache_.End();
  if (!found) {
    std::move(callback).Run();
    return;
  }

  switch (cache_entry_it->second.state()) {
    case Cache::Entry::State::kPending:
      cache_entry_it->second.SetCloseReaderCallback(std::move(callback));
      break;
    case Cache::Entry::State::kReady:
      CloseReader(cache_entry_it->second.StealReader(), std::move(callback));
      reader_cache_.Erase(cache_entry_it);
      break;
  }
}

void IsolatedWebAppReaderRegistry::OnResponseReaderCreated(
    const base::FilePath& web_bundle_path,
    bool dev_mode,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                   UnusableSwbnFileError> reader) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto cache_entry_it =
      reader_cache_.Find({.path = web_bundle_path, .dev_mode = dev_mode});
  DCHECK(cache_entry_it != reader_cache_.End());
  DCHECK_EQ(cache_entry_it->second.state(), Cache::Entry::State::kPending);

  std::vector<std::pair<network::ResourceRequest, ReadResponseCallback>>
      pending_requests =
          std::exchange(cache_entry_it->second.pending_requests, {});

  const bool should_close_reader =
      cache_entry_it->second.IsCloseReaderRequested();
  const bool can_use_reader = reader.has_value() && !should_close_reader;

  if (!can_use_reader) {
    const auto error =
        !reader.has_value()
            ? ReadResponseError::ForError(reader.error())
            : ReadResponseError::ForOtherError("The bundle is waiting to close");

    for (auto& [resource_request, callback] : pending_requests) {
      std::move(callback).Run(base::unexpected(error));
    }
    if (should_close_reader) {
      CloseReader(std::move(reader.value()),
                  cache_entry_it->second.GetCloseReaderCallback());
    }
    reader_cache_.Erase(cache_entry_it);
    return;
  }

  // The `SignedWebBundleReader` is now ready to read responses. Inform all
  // consumers that were waiting for this `SignedWebBundleReader` to become
  // available.
  verified_files_.insert(cache_entry_it->first.path);
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
  base::expected<void, IsolatedWebAppReaderRegistry::ReadResponseHeadError>
      response_status = ToReadResponseHeadError(response);
  UmaLogExpectedStatus("WebApp.Isolated.ReadResponseHead", response_status);

  std::move(callback).Run(std::move(response).transform_error(
      static_cast<ReadResponseError (*)(
          const IsolatedWebAppResponseReader::Error&)>(
          &ReadResponseError::ForError)));
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const UnusableSwbnFileError& error) {
  return ForOtherError(
      IsolatedWebAppResponseReaderFactory::ErrorToString(error));
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const IsolatedWebAppResponseReader::Error& error) {
  switch (error.type) {
    case IsolatedWebAppResponseReader::Error::Type::kParserInternalError:
    case IsolatedWebAppResponseReader::Error::Type::kFormatError:
    case IsolatedWebAppResponseReader::Error::Type::kNotTrusted:
      return ForOtherError(base::StringPrintf(
          "Failed to parse response head: %s", error.message.c_str()));
    case IsolatedWebAppResponseReader::Error::Type::kResponseNotFound:
      return ForResponseNotFound(base::StringPrintf(
          "Failed to read response from Signed Web Bundle: %s",
          error.message.c_str()));
  }
}

IsolatedWebAppReaderRegistry::Cache::Cache() = default;
IsolatedWebAppReaderRegistry::Cache::~Cache() = default;

base::flat_map<IsolatedWebAppReaderRegistry::Cache::Key,
               IsolatedWebAppReaderRegistry::Cache::Entry>::iterator
IsolatedWebAppReaderRegistry::Cache::Find(const Key& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cache_.find(key);
}

base::flat_map<IsolatedWebAppReaderRegistry::Cache::Key,
               IsolatedWebAppReaderRegistry::Cache::Entry>::iterator
IsolatedWebAppReaderRegistry::Cache::End() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cache_.end();
}

template <class... Args>
std::pair<base::flat_map<IsolatedWebAppReaderRegistry::Cache::Key,
                         IsolatedWebAppReaderRegistry::Cache::Entry>::iterator,
          bool>
IsolatedWebAppReaderRegistry::Cache::Emplace(Args&&... args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = cache_.emplace(std::forward<Args>(args)...);
  StartCleanupTimerIfNotRunning();
  return result;
}

void IsolatedWebAppReaderRegistry::Cache::Erase(
    base::flat_map<IsolatedWebAppReaderRegistry::Cache::Key, Entry>::iterator
        iterator) {
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
  base::EraseIf(cache_, [&now](std::pair<Key, Entry>& entry_pair) {
    auto& cache_entry = entry_pair.second;
    // If a `SignedWebBundleReader` is ready to read responses
    // and has not been used for at least `kCleanupInterval`,
    // close it and remove it from the cache.
    if (cache_entry.state() == Entry::State::kReady &&
        now - cache_entry.last_access() > kCleanupInterval) {
      CloseReader(cache_entry.StealReader(), base::DoNothing());
      return true;
    }
    return false;
  });
  StopCleanupTimerIfCacheIsEmpty();
}

bool IsolatedWebAppReaderRegistry::Cache::Key::operator<(
    const Key& other) const {
  return std::tie(path, dev_mode) < std::tie(other.path, other.dev_mode);
}

void IsolatedWebAppReaderRegistry::Cache::Entry::SetCloseReaderCallback(
    base::OnceClosure callback) {
  CHECK(pending_closed_callback_.is_null());
  pending_closed_callback_ = std::move(callback);
}

base::OnceClosure
IsolatedWebAppReaderRegistry::Cache::Entry::GetCloseReaderCallback() {
  CHECK(!pending_closed_callback_.is_null());
  return std::move(pending_closed_callback_);
}

std::unique_ptr<IsolatedWebAppResponseReader>
IsolatedWebAppReaderRegistry::Cache::Entry::StealReader() {
  CHECK(reader_);
  return std::move(reader_);
}

bool IsolatedWebAppReaderRegistry::Cache::Entry::IsCloseReaderRequested()
    const {
  return !pending_closed_callback_.is_null();
}

IsolatedWebAppReaderRegistry::Cache::Entry::Entry() = default;

IsolatedWebAppReaderRegistry::Cache::Entry::~Entry() = default;

IsolatedWebAppReaderRegistry::Cache::Entry::Entry(Entry&& other) = default;

IsolatedWebAppReaderRegistry::Cache::Entry&
IsolatedWebAppReaderRegistry::Cache::Entry::operator=(Entry&& other) = default;

}  // namespace web_app
