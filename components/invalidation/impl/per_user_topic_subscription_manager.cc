// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_subscription_manager.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "google_apis/gaia/gaia_constants.h"

namespace invalidation {

namespace {

constexpr char kDeprecatedSyncInvalidationGCMSenderId[] = "8181035976";

const char kTypeSubscribedForInvalidations[] =
    "invalidation.per_sender_registered_for_invalidation";

const char kActiveRegistrationTokens[] =
    "invalidation.per_sender_active_registration_tokens";

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

// Note: Taking |topic| and |private_topic_name| by value (rather than const
// ref) because the caller (in practice, SubscriptionEntry) may be destroyed by
// the callback.
// This is a RepeatingCallback because in case of failure, the request will get
// retried, so it might actually run multiple times.
using SubscriptionFinishedCallback = base::RepeatingCallback<void(
    Topic topic,
    Status code,
    std::string private_topic_name,
    PerUserTopicSubscriptionRequest::RequestType type)>;

static const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 3600 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

class PerProjectDictionaryPrefUpdate {
 public:
  explicit PerProjectDictionaryPrefUpdate(PrefService* prefs,
                                          const std::string& project_id)
      : update_(prefs, kTypeSubscribedForInvalidations) {
    per_sender_pref_ = update_->EnsureDict(project_id);
    DCHECK(per_sender_pref_);
  }

  base::Value::Dict& operator*() { return *per_sender_pref_; }

  base::Value::Dict* operator->() { return per_sender_pref_; }

 private:
  ScopedDictPrefUpdate update_;
  raw_ptr<base::Value::Dict> per_sender_pref_;
};

// State of the instance ID token when subscription is requested.
// Used by UMA histogram, so entries shouldn't be reordered or removed.
enum class TokenStateOnSubscriptionRequest {
  kTokenWasEmpty = 0,
  kTokenUnchanged = 1,
  kTokenChanged = 2,
  kTokenCleared = 3,
  kMaxValue = kTokenCleared,
};

void ReportTokenState(TokenStateOnSubscriptionRequest token_state) {
  base::UmaHistogramEnumeration(
      "FCMInvalidations.TokenStateOnRegistrationRequest2", token_state);
}

}  // namespace

// static
void PerUserTopicSubscriptionManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTypeSubscribedForInvalidations);
  registry->RegisterDictionaryPref(kActiveRegistrationTokens);
}

// static
void PerUserTopicSubscriptionManager::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // Same as RegisterProfilePrefs; see comment in the header.
  RegisterProfilePrefs(registry);
}

// static
void PerUserTopicSubscriptionManager::ClearDeprecatedPrefs(PrefService* prefs) {
  if (prefs->HasPrefPath(kTypeSubscribedForInvalidations)) {
    ScopedDictPrefUpdate update(prefs, kTypeSubscribedForInvalidations);
    update->Remove(kDeprecatedSyncInvalidationGCMSenderId);
  }
  if (prefs->HasPrefPath(kActiveRegistrationTokens)) {
    ScopedDictPrefUpdate update(prefs, kActiveRegistrationTokens);
    update->Remove(kDeprecatedSyncInvalidationGCMSenderId);
  }
}

struct PerUserTopicSubscriptionManager::SubscriptionEntry {
  SubscriptionEntry(const Topic& topic,
                    SubscriptionFinishedCallback completion_callback,
                    PerUserTopicSubscriptionRequest::RequestType type,
                    bool topic_is_public = false);

  SubscriptionEntry(const SubscriptionEntry&) = delete;
  SubscriptionEntry& operator=(const SubscriptionEntry&) = delete;

  // Destruction of this object causes cancellation of the request.
  ~SubscriptionEntry();

  void SubscriptionFinished(const Status& code,
                            const std::string& private_topic_name);

  // The object for which this is the status.
  const Topic topic;
  const bool topic_is_public;
  SubscriptionFinishedCallback completion_callback;
  PerUserTopicSubscriptionRequest::RequestType type;

  base::OneShotTimer request_retry_timer_;
  net::BackoffEntry request_backoff_;

  std::unique_ptr<PerUserTopicSubscriptionRequest> request;
  std::string last_request_access_token;

  bool has_retried_on_auth_error = false;
};

PerUserTopicSubscriptionManager::SubscriptionEntry::SubscriptionEntry(
    const Topic& topic,
    SubscriptionFinishedCallback completion_callback,
    PerUserTopicSubscriptionRequest::RequestType type,
    bool topic_is_public)
    : topic(topic),
      topic_is_public(topic_is_public),
      completion_callback(std::move(completion_callback)),
      type(type),
      request_backoff_(&kBackoffPolicy) {}

PerUserTopicSubscriptionManager::SubscriptionEntry::~SubscriptionEntry() =
    default;

void PerUserTopicSubscriptionManager::SubscriptionEntry::SubscriptionFinished(
    const Status& code,
    const std::string& topic_name) {
  completion_callback.Run(topic, code, topic_name, type);
}

PerUserTopicSubscriptionManager::PerUserTopicSubscriptionManager(
    IdentityProvider* identity_provider,
    PrefService* pref_service,
    network::mojom::URLLoaderFactory* url_loader_factory,
    const std::string& project_id)
    : pref_service_(pref_service),
      identity_provider_(identity_provider),
      url_loader_factory_(url_loader_factory),
      project_id_(project_id),
      request_access_token_backoff_(&kBackoffPolicy) {}

PerUserTopicSubscriptionManager::~PerUserTopicSubscriptionManager() = default;

// static
std::unique_ptr<PerUserTopicSubscriptionManager>
PerUserTopicSubscriptionManager::Create(
    network::mojom::URLLoaderFactory* url_loader_factory,
    IdentityProvider* identity_provider,
    PrefService* pref_service,
    const std::string& project_id) {
  return std::make_unique<PerUserTopicSubscriptionManager>(
      identity_provider, pref_service, url_loader_factory, project_id);
}

void PerUserTopicSubscriptionManager::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Load registration token from prefs
  const auto& token_dict = pref_service_->GetDict(kActiveRegistrationTokens);
  const auto* cached_token = token_dict.FindString(project_id_);
  if (cached_token) {
    instance_id_token_ = *cached_token;
  }

  // Load subscribed topics from prefs.
  PerProjectDictionaryPrefUpdate update(pref_service_, project_id_);
  if (update->empty()) {
    return;
  }

  if (instance_id_token_.empty()) {
    // Cannot be subscribed without a token.
    update->clear();
    return;
  }

  std::vector<std::string> keys_to_remove;
  // Load subscribed topics from prefs.
  for (auto it : *update) {
    Topic topic = it.first;
    const std::string* private_topic_name = it.second.GetIfString();
    if (private_topic_name && !private_topic_name->empty()) {
      topic_to_private_topic_[topic] = *private_topic_name;
      private_topic_to_topic_[*private_topic_name] = topic;
    } else {
      // Couldn't decode the pref value; remove it.
      keys_to_remove.push_back(topic);
    }
  }

  // Delete prefs, which weren't decoded successfully.
  for (const std::string& key : keys_to_remove) {
    update->Remove(key);
  }
}

void PerUserTopicSubscriptionManager::UpdateSubscribedTopics(
    const TopicMap& topics,
    const std::string& new_instance_id_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReportNewInstanceIdTokenState(new_instance_id_token);
  DropAllSavedSubscriptionsOnTokenChange(new_instance_id_token);
  StoreNewToken(new_instance_id_token);

  for (const auto& topic : topics) {
    auto it = pending_subscriptions_.find(topic.first);
    if (it != pending_subscriptions_.end() &&
        it->second->type == RequestType::kSubscribe) {
      // Do not update SubscriptionEntry if there is no changes, to not loose
      // backoff timer.
      continue;
    }

    // If the topic isn't subscribed yet, schedule the subscription.
    if (topic_to_private_topic_.find(topic.first) ==
        topic_to_private_topic_.end()) {
      // If there was already a pending unsubscription request for this topic,
      // it'll get destroyed and replaced by the new one.
      pending_subscriptions_[topic.first] = std::make_unique<SubscriptionEntry>(
          topic.first,
          base::BindRepeating(
              &PerUserTopicSubscriptionManager::SubscriptionFinishedForTopic,
              base::Unretained(this)),
          RequestType::kSubscribe, topic.second.is_public);
    }
  }

  // There may be subscribed topics which need to be unsubscribed.
  // Schedule unsubscription and immediately remove from
  // |topic_to_private_topic_| and |private_topic_to_topic_|.
  for (auto it = topic_to_private_topic_.begin();
       it != topic_to_private_topic_.end();) {
    Topic topic = it->first;
    if (topics.find(topic) == topics.end()) {
      // Unsubscription request may only replace pending subscription request,
      // because topic immediately deleted from |topic_to_private_topic_| when
      // unsubscription request scheduled.
      DCHECK(pending_subscriptions_.count(topic) == 0 ||
             pending_subscriptions_[topic]->type == RequestType::kSubscribe);
      // If there was already a pending request for this topic, it'll get
      // destroyed and replaced by the new one.
      pending_subscriptions_[topic] = std::make_unique<SubscriptionEntry>(
          topic,
          base::BindRepeating(
              &PerUserTopicSubscriptionManager::SubscriptionFinishedForTopic,
              base::Unretained(this)),
          RequestType::kUnsubscribe);
      private_topic_to_topic_.erase(it->second);
      it = topic_to_private_topic_.erase(it);
      // The decision to unsubscribe from invalidations for |topic| was
      // made, the preferences should be cleaned up immediately.
      PerProjectDictionaryPrefUpdate update(pref_service_, project_id_);
      update->Remove(topic);
    } else {
      // Topic is still wanted, nothing to do.
      ++it;
    }
  }
  // There might be pending subscriptions for topics which are no longer
  // needed, but they could be in half-completed state (i.e. request already
  // sent to the server). To reduce subscription leaks they are allowed to
  // proceed and unsubscription requests will be scheduled by the next
  // UpdateSubscribedTopics() call after they successfully completed.

  if (!pending_subscriptions_.empty()) {
    // Kick off the process of actually processing the (un)subscriptions we just
    // scheduled.
    RequestAccessToken();
  } else {
    // No work to be done, emit ENABLED.
    NotifySubscriptionChannelStateChange(SubscriptionChannelState::ENABLED);
  }
}

void PerUserTopicSubscriptionManager::ClearInstanceIDToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateSubscribedTopics(/*topics=*/{}, /*new_instance_id_token=*/{});
}

void PerUserTopicSubscriptionManager::StartPendingSubscriptions() {
  for (const auto& pending_subscription : pending_subscriptions_) {
    StartPendingSubscriptionRequest(pending_subscription.first);
  }
}

void PerUserTopicSubscriptionManager::StartPendingSubscriptionRequest(
    const Topic& topic) {
  auto it = pending_subscriptions_.find(topic);
  if (it == pending_subscriptions_.end()) {
    NOTREACHED_IN_MIGRATION()
        << "StartPendingSubscriptionRequest called on " << topic
        << " which is not in the registration map";
    return;
  }
  if (it->second->request_retry_timer_.IsRunning()) {
    // A retry is already scheduled for this request; nothing to do.
    return;
  }
  if (it->second->request &&
      it->second->last_request_access_token == access_token_) {
    // The request with the same access token was already sent; nothing to do.
    return;
  }
  PerUserTopicSubscriptionRequest::Builder builder;
  it->second->last_request_access_token = access_token_;
  it->second->request = builder.SetInstanceIdToken(instance_id_token_)
                            .SetScope(kInvalidationRegistrationScope)
                            .SetPublicTopicName(topic)
                            .SetAuthenticationHeader(base::StringPrintf(
                                "Bearer %s", access_token_.c_str()))
                            .SetProjectId(project_id_)
                            .SetType(it->second->type)
                            .SetTopicIsPublic(it->second->topic_is_public)
                            .Build();
  it->second->request->Start(
      base::BindOnce(&PerUserTopicSubscriptionManager::SubscriptionEntry::
                         SubscriptionFinished,
                     base::Unretained(it->second.get())),
      url_loader_factory_);
}

void PerUserTopicSubscriptionManager::ActOnSuccessfulSubscription(
    const Topic& topic,
    const std::string& private_topic_name,
    PerUserTopicSubscriptionRequest::RequestType type) {
  auto it = pending_subscriptions_.find(topic);
  it->second->request_backoff_.InformOfRequest(true);
  pending_subscriptions_.erase(it);
  if (type == RequestType::kSubscribe) {
    // If this was a subscription, update the prefs now (if it was an
    // unsubscription, we've already updated the prefs when scheduling the
    // request).
    {
      PerProjectDictionaryPrefUpdate update(pref_service_, project_id_);
      update->Set(topic, private_topic_name);
      topic_to_private_topic_[topic] = private_topic_name;
      private_topic_to_topic_[private_topic_name] = topic;
    }
    pref_service_->CommitPendingWrite();
  }
  // Check if there are any other subscription (not unsubscription) requests
  // pending.
  bool all_subscriptions_completed = true;
  for (const auto& entry : pending_subscriptions_) {
    if (entry.second->type == RequestType::kSubscribe) {
      all_subscriptions_completed = false;
    }
  }
  // Emit ENABLED once all requests have finished.
  if (all_subscriptions_completed) {
    NotifySubscriptionChannelStateChange(SubscriptionChannelState::ENABLED);
  }
}

void PerUserTopicSubscriptionManager::ScheduleRequestForRepetition(
    const Topic& topic) {
  pending_subscriptions_[topic]->request_backoff_.InformOfRequest(false);
  // Schedule RequestAccessToken() to ensure that request is performed with
  // fresh access token. There should be no redundant request: the identity
  // code requests new access token from the network only if the old one
  // expired; StartPendingSubscriptionRequest() guarantees that no redundant
  // (un)subscribe requests performed.
  pending_subscriptions_[topic]->request_retry_timer_.Start(
      FROM_HERE,
      pending_subscriptions_[topic]->request_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&PerUserTopicSubscriptionManager::RequestAccessToken,
                     base::Unretained(this)));
}

void PerUserTopicSubscriptionManager::SubscriptionFinishedForTopic(
    Topic topic,
    Status code,
    std::string private_topic_name,
    PerUserTopicSubscriptionRequest::RequestType type) {
  NotifySubscriptionRequestFinished(topic, type, code);
  if (code.IsSuccess()) {
    ActOnSuccessfulSubscription(topic, private_topic_name, type);
    return;
  }

  auto it = pending_subscriptions_.find(topic);
  // Reset |request| to make sure it will be rescheduled during the next
  // attempt.
  it->second->request.reset();
  // If this is the first auth error we've encountered, then most likely the
  // access token has just expired. Get a new one and retry immediately.
  if (code.IsAuthFailure() && !it->second->has_retried_on_auth_error) {
    it->second->has_retried_on_auth_error = true;
    // Invalidate previous token if it's not already refreshed, otherwise
    // the identity provider will return the same token again.
    if (!access_token_.empty() &&
        it->second->last_request_access_token == access_token_) {
      identity_provider_->InvalidateAccessToken({GaiaConstants::kFCMOAuthScope},
                                                access_token_);
      access_token_.clear();
    }
    // Re-request access token and try subscription requests again.
    RequestAccessToken();
    return;
  }

  // If one of the subscription requests failed (and we need to either observe
  // backoff before retrying, or won't retry at all), emit SUBSCRIPTION_FAILURE.
  if (type == RequestType::kSubscribe) {
    // TODO(crbug.com/40105630): case !code.ShouldRetry() now leads to
    // inconsistent behavior depending on requests completion order: if any
    // request was successful after it, we may have no |pending_subscriptions_|
    // and emit ENABLED; otherwise, if failed request is the last one, state
    // would be SUBSCRIPTION_FAILURE.
    NotifySubscriptionChannelStateChange(
        SubscriptionChannelState::SUBSCRIPTION_FAILURE);
  }
  if (!code.ShouldRetry()) {
    // Note: This is a pretty bad (and "silent") failure case. The subscription
    // will generally not be retried until the next Chrome restart (or user
    // sign-out + re-sign-in).
    DVLOG(1) << "Got a persistent error while trying to subscribe to topic "
             << topic << ", giving up.";
    pending_subscriptions_.erase(it);
    return;
  }
  ScheduleRequestForRepetition(topic);
}

TopicSet PerUserTopicSubscriptionManager::GetSubscribedTopicsForTest() const {
  TopicSet topics;
  for (const auto& t : topic_to_private_topic_)
    topics.insert(t.first);

  return topics;
}

void PerUserTopicSubscriptionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PerUserTopicSubscriptionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PerUserTopicSubscriptionManager::RequestAccessToken() {
  // Only one active request at a time.
  if (access_token_fetcher_ != nullptr) {
    return;
  }
  if (request_access_token_retry_timer_.IsRunning()) {
    // Previous access token request failed and new request shouldn't be issued
    // until backoff timer passed.
    return;
  }

  access_token_.clear();
  access_token_fetcher_ = identity_provider_->FetchAccessToken(
      "fcm_invalidation", {GaiaConstants::kFCMOAuthScope},
      base::BindOnce(
          &PerUserTopicSubscriptionManager::OnAccessTokenRequestCompleted,
          base::Unretained(this)));
}

void PerUserTopicSubscriptionManager::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    std::string access_token) {
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE)
    OnAccessTokenRequestSucceeded(access_token);
  else
    OnAccessTokenRequestFailed(error);
}

void PerUserTopicSubscriptionManager::OnAccessTokenRequestSucceeded(
    const std::string& access_token) {
  // Reset backoff time after successful response.
  request_access_token_backoff_.InformOfRequest(/*succeeded=*/true);
  access_token_ = access_token;
  StartPendingSubscriptions();
}

void PerUserTopicSubscriptionManager::OnAccessTokenRequestFailed(
    GoogleServiceAuthError error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  NotifySubscriptionChannelStateChange(
      SubscriptionChannelState::ACCESS_TOKEN_FAILURE);
  request_access_token_backoff_.InformOfRequest(false);
  request_access_token_retry_timer_.Start(
      FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&PerUserTopicSubscriptionManager::RequestAccessToken,
                     base::Unretained(this)));
}

void PerUserTopicSubscriptionManager::ReportNewInstanceIdTokenState(
    const std::string& new_instance_id_token) const {
  if (instance_id_token_ == new_instance_id_token) {
    ReportTokenState(TokenStateOnSubscriptionRequest::kTokenUnchanged);
  } else if (instance_id_token_.empty()) {
    ReportTokenState(TokenStateOnSubscriptionRequest::kTokenWasEmpty);
  } else if (new_instance_id_token.empty()) {
    ReportTokenState(TokenStateOnSubscriptionRequest::kTokenCleared);
  } else {
    ReportTokenState(TokenStateOnSubscriptionRequest::kTokenChanged);
  }
}

void PerUserTopicSubscriptionManager::StoreNewToken(
    const std::string& new_instance_id_token) {
  instance_id_token_ = new_instance_id_token;
  ScopedDictPrefUpdate token_update(pref_service_, kActiveRegistrationTokens);
  token_update->Set(project_id_, new_instance_id_token);
}

void PerUserTopicSubscriptionManager::DropAllSavedSubscriptionsOnTokenChange(
    const std::string& new_instance_id_token) {
  if (instance_id_token_ == new_instance_id_token) {
    return;
  }

  // The token has been cleared or changed. In either case, clear all existing
  // subscriptions since they won't be valid anymore. (No need to send
  // unsubscribe requests - if the token was revoked, the server will drop the
  // subscriptions anyway.)
  PerProjectDictionaryPrefUpdate update(pref_service_, project_id_);
  *update = base::Value::Dict();
  topic_to_private_topic_.clear();
  private_topic_to_topic_.clear();
  pending_subscriptions_.clear();
}

void PerUserTopicSubscriptionManager::NotifySubscriptionChannelStateChange(
    SubscriptionChannelState state) {
  // NOT_STARTED is the default state of the subscription
  // channel and shouldn't explicitly issued.
  DCHECK(state != SubscriptionChannelState::NOT_STARTED);
  if (last_issued_state_ == state) {
    // Notify only on state change.
    return;
  }

  last_issued_state_ = state;
  for (auto& observer : observers_) {
    observer.OnSubscriptionChannelStateChanged(state);
  }
}

void PerUserTopicSubscriptionManager::NotifySubscriptionRequestFinished(
    Topic topic,
    RequestType request_type,
    Status code) {
  for (auto& observer : observers_) {
    observer.OnSubscriptionRequestFinished(topic, request_type, code);
  }
}

std::optional<Topic>
PerUserTopicSubscriptionManager::LookupSubscribedPublicTopicByPrivateTopic(
    const std::string& private_topic) const {
  auto it = private_topic_to_topic_.find(private_topic);
  if (it == private_topic_to_topic_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace invalidation
