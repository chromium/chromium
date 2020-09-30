// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_service.h"

#include <utility>

#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/feed_network_impl.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace feed {
namespace {
class EulaObserver : public web_resource::EulaAcceptedNotifier::Observer {
 public:
  explicit EulaObserver(FeedStream* feed_stream) : feed_stream_(feed_stream) {}
  EulaObserver(EulaObserver&) = delete;
  EulaObserver& operator=(const EulaObserver&) = delete;

  // web_resource::EulaAcceptedNotifier::Observer.
  void OnEulaAccepted() override { feed_stream_->OnEulaAccepted(); }

 private:
  FeedStream* feed_stream_;
};

}  // namespace

namespace internal {
bool ShouldClearFeed(bool is_signed_in,
                     const history::DeletionInfo& deletion_info) {
  // Only clear the feed if all history is deleted while a user is signed-in.
  // Clear history events happen between sign-in events, and those should be
  // ignored.
  return is_signed_in && deletion_info.IsAllHistory();
}
}  // namespace internal

class FeedService::HistoryObserverImpl
    : public history::HistoryServiceObserver {
 public:
  HistoryObserverImpl(history::HistoryService* history_service,
                      FeedStream* feed_stream,
                      signin::IdentityManager* identity_manager)
      : feed_stream_(feed_stream), identity_manager_(identity_manager) {
    // May be null for some profiles.
    if (history_service)
      history_service->AddObserver(this);
  }
  HistoryObserverImpl(const HistoryObserverImpl&) = delete;
  HistoryObserverImpl& operator=(const HistoryObserverImpl&) = delete;

  // history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override {
    if (internal::ShouldClearFeed(identity_manager_->HasPrimaryAccount(),
                                  deletion_info))
      feed_stream_->OnAllHistoryDeleted();
  }

 private:
  FeedStream* feed_stream_;
  signin::IdentityManager* identity_manager_;
};

class FeedService::NetworkDelegateImpl : public FeedNetworkImpl::Delegate {
 public:
  explicit NetworkDelegateImpl(FeedService::Delegate* service_delegate)
      : service_delegate_(service_delegate) {}
  NetworkDelegateImpl(const NetworkDelegateImpl&) = delete;
  NetworkDelegateImpl& operator=(const NetworkDelegateImpl&) = delete;

  // FeedNetworkImpl::Delegate.
  std::string GetLanguageTag() override {
    return service_delegate_->GetLanguageTag();
  }

 private:
  FeedService::Delegate* service_delegate_;
};

class FeedService::StreamDelegateImpl : public FeedStream::Delegate {
 public:
  StreamDelegateImpl(PrefService* local_state,
                     FeedService::Delegate* service_delegate,
                     signin::IdentityManager* identity_manager)
      : service_delegate_(service_delegate),
        eula_notifier_(local_state),
        identity_manager_(identity_manager) {}
  StreamDelegateImpl(const StreamDelegateImpl&) = delete;
  StreamDelegateImpl& operator=(const StreamDelegateImpl&) = delete;

  void Initialize(FeedStream* feed_stream) {
    eula_observer_ = std::make_unique<EulaObserver>(feed_stream);
    eula_notifier_.Init(eula_observer_.get());
  }

  // FeedStream::Delegate.
  bool IsEulaAccepted() override { return eula_notifier_.IsEulaAccepted(); }
  bool IsOffline() override { return net::NetworkChangeNotifier::IsOffline(); }
  DisplayMetrics GetDisplayMetrics() override {
    return service_delegate_->GetDisplayMetrics();
  }
  std::string GetLanguageTag() override {
    return service_delegate_->GetLanguageTag();
  }
  void ClearAll() override { service_delegate_->ClearAll(); }
  bool IsSignedIn() override { return identity_manager_->HasPrimaryAccount(); }

 private:
  FeedService::Delegate* service_delegate_;
  web_resource::EulaAcceptedNotifier eula_notifier_;
  std::unique_ptr<EulaObserver> eula_observer_;
  std::unique_ptr<HistoryObserverImpl> history_observer_;
  signin::IdentityManager* identity_manager_;
};

class FeedService::IdentityManagerObserverImpl
    : public signin::IdentityManager::Observer {
 public:
  IdentityManagerObserverImpl(signin::IdentityManager* identity_manager,
                              FeedStream* stream)
      : identity_manager_(identity_manager), feed_stream_(stream) {}
  IdentityManagerObserverImpl(const IdentityManagerObserverImpl&) = delete;
  IdentityManagerObserverImpl& operator=(const IdentityManagerObserverImpl&) =
      delete;
  ~IdentityManagerObserverImpl() override {
    identity_manager_->RemoveObserver(this);
  }
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override {
    feed_stream_->OnSignedIn();
  }
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override {
    feed_stream_->OnSignedOut();
  }

 private:
  signin::IdentityManager* identity_manager_;
  FeedStream* feed_stream_;
};

FeedService::FeedService(std::unique_ptr<FeedStream> stream)
    : stream_(std::move(stream)) {}

FeedService::FeedService(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<RefreshTaskScheduler> refresh_task_scheduler,
    PrefService* profile_prefs,
    PrefService* local_state,
    std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database,
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service,
    offline_pages::PrefetchService* prefetch_service,
    offline_pages::OfflinePageModel* offline_page_model,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const std::string& api_key,
    const ChromeInfo& chrome_info)
    : delegate_(std::move(delegate)),
      refresh_task_scheduler_(std::move(refresh_task_scheduler)) {
  stream_delegate_ = std::make_unique<StreamDelegateImpl>(
      local_state, delegate_.get(), identity_manager);
  network_delegate_ = std::make_unique<NetworkDelegateImpl>(delegate_.get());
  metrics_reporter_ = std::make_unique<MetricsReporter>(
      base::DefaultTickClock::GetInstance(), profile_prefs);
  feed_network_ = std::make_unique<FeedNetworkImpl>(
      network_delegate_.get(), identity_manager, api_key, url_loader_factory,
      base::DefaultTickClock::GetInstance(), profile_prefs);
  image_fetcher_ = std::make_unique<ImageFetcher>(url_loader_factory);
  store_ = std::make_unique<FeedStore>(std::move(database));

  stream_ = std::make_unique<FeedStream>(
      refresh_task_scheduler_.get(), metrics_reporter_.get(),
      stream_delegate_.get(), profile_prefs, feed_network_.get(),
      image_fetcher_.get(), store_.get(), prefetch_service, offline_page_model,
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      chrome_info);

  history_observer_ = std::make_unique<HistoryObserverImpl>(
      history_service, static_cast<FeedStream*>(stream_.get()),
      identity_manager);
  stream_delegate_->Initialize(static_cast<FeedStream*>(stream_.get()));

  identity_manager_observer_ = std::make_unique<IdentityManagerObserverImpl>(
      identity_manager, stream_.get());
  identity_manager->AddObserver(identity_manager_observer_.get());

#if defined(OS_ANDROID)
  application_status_listener_ =
      base::android::ApplicationStatusListener::New(base::BindRepeating(
          &FeedService::OnApplicationStateChange, base::Unretained(this)));
#endif
}

FeedService::~FeedService() = default;

FeedStreamApi* FeedService::GetStream() {
  return stream_.get();
}

void FeedService::ClearCachedData() {
  stream_->OnCacheDataCleared();
}

// static
bool FeedService::IsEnabled(const PrefService& pref_service) {
  return feed::IsV2Enabled() &&
         pref_service.GetBoolean(feed::prefs::kEnableSnippets);
}

#if defined(OS_ANDROID)
void FeedService::OnApplicationStateChange(
    base::android::ApplicationState state) {
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    // If we want to trigger an OnEnterForeground event, we'll need to be
    // careful about the initial state of foregrounded_.
    foregrounded_ = true;
  }
  if (foregrounded_ &&
      state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES) {
    foregrounded_ = false;
    stream_->OnEnterBackground();
  }
}
#endif

}  // namespace feed
