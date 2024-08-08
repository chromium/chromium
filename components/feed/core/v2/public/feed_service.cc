// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_service.h"

#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/feed_network_impl.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/persistent_key_value_store_impl.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/public/refresh_task_scheduler.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
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
  raw_ptr<FeedStream> feed_stream_;
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
      scoped_history_service_observer_.Observe(history_service);
  }
  HistoryObserverImpl(const HistoryObserverImpl&) = delete;
  HistoryObserverImpl& operator=(const HistoryObserverImpl&) = delete;

  // history::HistoryServiceObserver.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override {
    if (feed_stream_ && identity_manager_ &&
        internal::ShouldClearFeed(
            identity_manager_->HasPrimaryAccount(
                GetConsentLevelNeededForPersonalizedFeed()),
            deletion_info)) {
      feed_stream_->OnAllHistoryDeleted();
    }
  }

  void Shutdown() {
    feed_stream_ = nullptr;
    identity_manager_ = nullptr;
    scoped_history_service_observer_.Reset();
  }

 private:
  raw_ptr<FeedStream> feed_stream_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_history_service_observer_{this};
};

class FeedService::NetworkDelegateImpl : public FeedNetworkImpl::Delegate {
 public:
  NetworkDelegateImpl(FeedService::Delegate* service_delegate,
                      signin::IdentityManager* identity_manager)
      : service_delegate_(service_delegate),
        identity_manager_(identity_manager) {}
  NetworkDelegateImpl(const NetworkDelegateImpl&) = delete;
  NetworkDelegateImpl& operator=(const NetworkDelegateImpl&) = delete;

  // FeedNetworkImpl::Delegate.
  std::string GetLanguageTag() override {
    return service_delegate_->GetLanguageTag();
  }

  AccountInfo GetAccountInfo() override {
    return AccountInfo(identity_manager_->GetPrimaryAccountInfo(
        GetConsentLevelNeededForPersonalizedFeed()));
  }

  bool IsOffline() override { return net::NetworkChangeNotifier::IsOffline(); }

 private:
  raw_ptr<FeedService::Delegate> service_delegate_;
  raw_ptr<signin::IdentityManager> identity_manager_;
};

class FeedService::StreamDelegateImpl : public FeedStream::Delegate {
 public:
  StreamDelegateImpl(FeedService::Delegate* service_delegate,
                     signin::IdentityManager* identity_manager,
                     PrefService* profile_prefs)
      : service_delegate_(service_delegate),
        identity_manager_(identity_manager),
        profile_prefs_(profile_prefs) {}
  StreamDelegateImpl(const StreamDelegateImpl&) = delete;
  StreamDelegateImpl& operator=(const StreamDelegateImpl&) = delete;

  void Initialize(FeedStream* feed_stream, PrefService* local_state) {
    eula_notifier_ =
        std::make_unique<web_resource::EulaAcceptedNotifier>(local_state);
    eula_observer_ = std::make_unique<EulaObserver>(feed_stream);
    eula_notifier_->Init(eula_observer_.get());
  }

  // FeedStream::Delegate.
  bool IsEulaAccepted() override {
    if (eula_notifier_) {
      return eula_notifier_->IsEulaAccepted() ||
             base::CommandLine::ForCurrentProcess()->HasSwitch(
                 "feed-screenshot-mode");
    }
    return false;
  }
  bool IsOffline() override { return net::NetworkChangeNotifier::IsOffline(); }

  std::string GetCountry() override { return service_delegate_->GetCountry(); }

  DisplayMetrics GetDisplayMetrics() override {
    return service_delegate_->GetDisplayMetrics();
  }
  std::string GetLanguageTag() override {
    return service_delegate_->GetLanguageTag();
  }
  TabGroupEnabledState GetTabGroupEnabledState() override {
    return service_delegate_->GetTabGroupEnabledState();
  }
  void ClearAll() override { service_delegate_->ClearAll(); }
  void PrefetchImage(const GURL& url) override {
    service_delegate_->PrefetchImage(url);
  }
  AccountInfo GetAccountInfo() override {
    return AccountInfo(identity_manager_->GetPrimaryAccountInfo(
        GetConsentLevelNeededForPersonalizedFeed()));
  }
  bool IsSupervisedAccount() override {
    ::AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
        identity_manager_->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin));
    return account_info.capabilities.is_subject_to_parental_controls() ==
           signin::Tribool::kTrue;
  }
  // Returns if signin is allowed on Android. Return true on other platform so
  // behavior is unchanged there.
  bool IsSigninAllowed() override {
    return profile_prefs_->GetBoolean(::prefs::kSigninAllowed);
  }
  void RegisterExperiments(const Experiments& experiments) override {
    service_delegate_->RegisterExperiments(experiments);
  }
  void RegisterFollowingFeedFollowCountFieldTrial(
      size_t follow_count) override {
    service_delegate_->RegisterFollowingFeedFollowCountFieldTrial(follow_count);
  }
  void RegisterFeedUserSettingsFieldTrial(std::string_view group) override {
    service_delegate_->RegisterFeedUserSettingsFieldTrial(group);
  }

  void Shutdown() {
    eula_notifier_.reset();
    eula_observer_.reset();
  }

 private:
  raw_ptr<FeedService::Delegate> service_delegate_;
  std::unique_ptr<web_resource::EulaAcceptedNotifier> eula_notifier_;
  std::unique_ptr<EulaObserver> eula_observer_;
  std::unique_ptr<HistoryObserverImpl> history_observer_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<PrefService> profile_prefs_;
};

class FeedService::IdentityManagerObserverImpl
    : public signin::IdentityManager::Observer {
 public:
  IdentityManagerObserverImpl(signin::IdentityManager* identity_manager,
                              FeedStream* stream,
                              FeedService::Delegate* service_delegate)
      : identity_manager_(identity_manager),
        feed_stream_(stream),
        service_delegate_(service_delegate) {}
  IdentityManagerObserverImpl(const IdentityManagerObserverImpl&) = delete;
  IdentityManagerObserverImpl& operator=(const IdentityManagerObserverImpl&) =
      delete;
  ~IdentityManagerObserverImpl() override {
    identity_manager_->RemoveObserver(this);
  }
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    switch (event.GetEventTypeFor(GetConsentLevelNeededForPersonalizedFeed())) {
      case signin::PrimaryAccountChangeEvent::Type::kSet:
        feed_stream_->OnSignedIn();
        return;
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        feed_stream_->OnSignedOut();
        return;
      case signin::PrimaryAccountChangeEvent::Type::kNone:
        return;
    }
  }

  signin::IdentityManager& identity_manager() { return *identity_manager_; }

 private:
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<FeedStream> feed_stream_;
  raw_ptr<FeedService::Delegate> service_delegate_;
};

FeedService::FeedService(std::unique_ptr<FeedStream> stream)
    : stream_(std::move(stream)) {}

FeedService::FeedService(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<RefreshTaskScheduler> refresh_task_scheduler,
    PrefService* profile_prefs,
    PrefService* local_state,
    std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database,
    std::unique_ptr<leveldb_proto::ProtoDatabase<feedkvstore::Entry>>
        key_value_store_database,
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const std::string& api_key,
    const ChromeInfo& chrome_info,
    TemplateURLService* template_url_service)
    : delegate_(std::move(delegate)),
      refresh_task_scheduler_(std::move(refresh_task_scheduler)) {
  stream_delegate_ = std::make_unique<StreamDelegateImpl>(
      delegate_.get(), identity_manager, profile_prefs);
  network_delegate_ =
      std::make_unique<NetworkDelegateImpl>(delegate_.get(), identity_manager);
  metrics_reporter_ = std::make_unique<MetricsReporter>(profile_prefs);
  feed_network_ = std::make_unique<FeedNetworkImpl>(
      network_delegate_.get(), identity_manager, api_key, url_loader_factory,
      profile_prefs);
  image_fetcher_ = std::make_unique<ImageFetcher>(url_loader_factory);
  store_ = std::make_unique<FeedStore>(std::move(database));
  persistent_key_value_store_ = std::make_unique<PersistentKeyValueStoreImpl>(
      std::move(key_value_store_database));

  stream_ = std::make_unique<FeedStream>(
      refresh_task_scheduler_.get(), metrics_reporter_.get(),
      stream_delegate_.get(), profile_prefs, feed_network_.get(),
      image_fetcher_.get(), store_.get(), persistent_key_value_store_.get(),
      template_url_service, chrome_info);
  api_ = stream_.get();

  history_observer_ = std::make_unique<HistoryObserverImpl>(
      history_service, static_cast<FeedStream*>(stream_.get()),
      identity_manager);
  stream_delegate_->Initialize(static_cast<FeedStream*>(stream_.get()),
                               local_state);

  identity_manager_observer_ = std::make_unique<IdentityManagerObserverImpl>(
      identity_manager, stream_.get(), delegate_.get());
  identity_manager->AddObserver(identity_manager_observer_.get());

  delegate_->RegisterExperiments(prefs::GetExperiments(*profile_prefs));

#if BUILDFLAG(IS_ANDROID)
  application_status_listener_ =
      base::android::ApplicationStatusListener::New(base::BindRepeating(
          &FeedService::OnApplicationStateChange, base::Unretained(this)));
#endif
}

FeedService::FeedService() = default;

// static
std::unique_ptr<FeedService> FeedService::CreateForTesting(FeedApi* api) {
  auto result = base::WrapUnique(new FeedService());
  result->api_ = api;
  return result;
}

FeedService::~FeedService() = default;

FeedApi* FeedService::GetStream() {
  return api_;
}

void FeedService::ClearCachedData() {
  stream_->OnCacheDataCleared();
}

const Experiments& FeedService::GetExperiments() const {
  return delegate_->GetExperiments();
}

// static
bool FeedService::IsEnabled(const PrefService& pref_service) {
  return pref_service.GetBoolean(feed::prefs::kEnableSnippets);
}

// static
uint64_t FeedService::GetReliabilityLoggingId(const std::string& metrics_id,
                                              PrefService* prefs) {
  // The reliability logging ID is generated from the UMA client ID so that it
  // changes whenever the UMA client ID changes. We hash the UMA client ID with
  // a random salt so that the UMA client ID can't be guessed from the
  // reliability logging ID. The salt never leaves the client.
  uint64_t salt;
  if (!prefs->HasPrefPath(prefs::kReliabilityLoggingIdSalt)) {
    salt = base::RandUint64();
    prefs->SetUint64(prefs::kReliabilityLoggingIdSalt, salt);
  } else {
    salt = prefs->GetUint64(prefs::kReliabilityLoggingIdSalt);
  }
  return base::FastHash(base::StrCat(
      {metrics_id, std::string(reinterpret_cast<char*>(&salt), sizeof(salt))}));
}

bool FeedService::IsSignedIn() {
  if (identity_manager_observer_) {
    return identity_manager_observer_->identity_manager().HasPrimaryAccount(
        GetConsentLevelNeededForPersonalizedFeed());
  }
  return false;
}

#if BUILDFLAG(IS_ANDROID)
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

void FeedService::Shutdown() {
  identity_manager_observer_.reset();
  if (history_observer_) {
    history_observer_->Shutdown();
  }

  if (stream_delegate_) {
    stream_delegate_->Shutdown();
  }
}

}  // namespace feed
