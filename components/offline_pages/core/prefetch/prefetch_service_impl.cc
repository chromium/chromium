// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl(
    std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
    std::unique_ptr<PrefetchDispatcher> dispatcher,
    std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory,
    OfflinePageModel* offline_page_model,
    std::unique_ptr<PrefetchStore> prefetch_store,
    std::unique_ptr<PrefetchDownloader> prefetch_downloader,
    std::unique_ptr<PrefetchImporter> prefetch_importer,
    std::unique_ptr<PrefetchGCMHandler> gcm_handler,
    std::unique_ptr<PrefetchBackgroundTaskHandler>
        prefetch_background_task_handler,
    image_fetcher::ImageFetcher* image_fetcher,
    PrefService* prefs)
    : offline_metrics_collector_(std::move(offline_metrics_collector)),
      prefetch_dispatcher_(std::move(dispatcher)),
      network_request_factory_(std::move(network_request_factory)),
      offline_page_model_(offline_page_model),
      prefetch_store_(std::move(prefetch_store)),
      prefetch_downloader_(std::move(prefetch_downloader)),
      prefetch_importer_(std::move(prefetch_importer)),
      prefetch_gcm_handler_(std::move(gcm_handler)),
      prefetch_background_task_handler_(
          std::move(prefetch_background_task_handler)),
      prefs_(prefs),
      image_fetcher_(image_fetcher) {
  prefetch_dispatcher_->SetService(this);
  prefetch_downloader_->SetPrefetchService(this);
  prefetch_gcm_handler_->SetService(this);
}

PrefetchServiceImpl::~PrefetchServiceImpl() {
  // The dispatcher needs to be disposed first because it may need to
  // communicate with other members owned by the service at destruction time.
  prefetch_dispatcher_.reset();
}

void PrefetchServiceImpl::ForceRefreshSuggestions() {
  if (suggestions_provider_) {
    NewSuggestionsAvailable();
  }
}

std::string PrefetchServiceImpl::GetCachedGCMToken() const {
  return prefetch_prefs::GetCachedPrefetchGCMToken(prefs_);
}

void PrefetchServiceImpl::GCMTokenReceived(
    const std::string& gcm_token,
    instance_id::InstanceID::Result result) {
  // TODO(dimich): Add UMA reporting on instance_id::InstanceID::Result.
  if (result == instance_id::InstanceID::Result::SUCCESS) {
    // Keep the cached token fresh
    prefetch_prefs::SetCachedPrefetchGCMToken(prefs_, gcm_token);
  }
}

void PrefetchServiceImpl::SetSuggestionProvider(
    SuggestionsProvider* suggestions_provider) {
  DCHECK(image_fetcher_);
  suggestions_provider_ = suggestions_provider;
}

void PrefetchServiceImpl::SetEnabledByServer(PrefService* pref_service,
                                             bool enabled) {
  if (enabled == prefetch_prefs::IsEnabledByServer(pref_service))
    return;

  prefetch_prefs::SetEnabledByServer(pref_service, enabled);
  if (enabled)
    ForceRefreshSuggestions();
}

void PrefetchServiceImpl::NewSuggestionsAvailable() {
  DCHECK(suggestions_provider_);
  prefetch_dispatcher_->NewSuggestionsAvailable(suggestions_provider_);
}

void PrefetchServiceImpl::RemoveSuggestion(GURL url) {
  DCHECK(suggestions_provider_);
  prefetch_dispatcher_->RemoveSuggestion(std::move(url));
}

OfflineMetricsCollector* PrefetchServiceImpl::GetOfflineMetricsCollector() {
  return offline_metrics_collector_.get();
}

PrefetchDispatcher* PrefetchServiceImpl::GetPrefetchDispatcher() {
  return prefetch_dispatcher_.get();
}

PrefetchGCMHandler* PrefetchServiceImpl::GetPrefetchGCMHandler() {
  DCHECK(prefetch_gcm_handler_);
  return prefetch_gcm_handler_.get();
}

PrefetchNetworkRequestFactory*
PrefetchServiceImpl::GetPrefetchNetworkRequestFactory() {
  return network_request_factory_.get();
}

OfflinePageModel* PrefetchServiceImpl::GetOfflinePageModel() {
  return offline_page_model_;
}

PrefetchStore* PrefetchServiceImpl::GetPrefetchStore() {
  return prefetch_store_.get();
}

OfflineEventLogger* PrefetchServiceImpl::GetLogger() {
  return &logger_;
}

PrefetchDownloader* PrefetchServiceImpl::GetPrefetchDownloader() {
  return prefetch_downloader_.get();
}

PrefetchImporter* PrefetchServiceImpl::GetPrefetchImporter() {
  return prefetch_importer_.get();
}

PrefetchBackgroundTaskHandler*
PrefetchServiceImpl::GetPrefetchBackgroundTaskHandler() {
  return prefetch_background_task_handler_.get();
}

image_fetcher::ImageFetcher* PrefetchServiceImpl::GetImageFetcher() {
  return image_fetcher_;
}

base::WeakPtr<PrefetchServiceImpl> PrefetchServiceImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PrefetchServiceImpl::Shutdown() {
  prefetch_gcm_handler_.reset();
  prefetch_downloader_.reset();
  image_fetcher_ = nullptr;
}

void PrefetchServiceImpl::ReplaceImageFetcher(
    image_fetcher::ImageFetcher* image_fetcher) {
  DCHECK(image_fetcher_);
  image_fetcher_ = image_fetcher;
}

}  // namespace offline_pages
