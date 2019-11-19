// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_registration_manager.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {

namespace {

const char kTypeRegisteredForInvalidationsDeprecated[] =
    "invalidation.registered_for_invalidation";

const char kTypeRegisteredForInvalidations[] =
    "invalidation.per_sender_registered_for_invalidation";

const char kActiveRegistrationTokenDeprecated[] =
    "invalidation.active_registration_token";

const char kActiveRegistrationTokens[] =
    "invalidation.per_sender_active_registration_tokens";

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kFCMOAuthScope[] =
    "https://www.googleapis.com/auth/firebase.messaging";

using SubscriptionFinishedCallback =
    base::OnceCallback<void(Topic topic,
                            Status code,
                            std::string private_topic_name,
                            PerUserTopicRegistrationRequest::RequestType type)>;

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
      : update_(prefs, kTypeRegisteredForInvalidations) {
    per_sender_pref_ = update_->FindDictKey(project_id);
    if (!per_sender_pref_) {
      update_->SetDictionary(project_id,
                             std::make_unique<base::DictionaryValue>());
      per_sender_pref_ = update_->FindDictKey(project_id);
    }
    DCHECK(per_sender_pref_);
  }

  base::Value& operator*() { return *per_sender_pref_; }

  base::Value* operator->() { return per_sender_pref_; }

 private:
  DictionaryPrefUpdate update_;
  base::Value* per_sender_pref_;
};

// Added in M76.
void MigratePrefs(PrefService* prefs, const std::string& project_id) {
  if (!prefs->HasPrefPath(kActiveRegistrationTokenDeprecated)) {
    return;
  }
  {
    DictionaryPrefUpdate token_update(prefs, kActiveRegistrationTokens);
    token_update->SetString(
        project_id, prefs->GetString(kActiveRegistrationTokenDeprecated));
  }

  auto* old_registrations =
      prefs->GetDictionary(kTypeRegisteredForInvalidationsDeprecated);
  {
    PerProjectDictionaryPrefUpdate update(prefs, project_id);
    *update = old_registrations->Clone();
  }
  prefs->ClearPref(kActiveRegistrationTokenDeprecated);
  prefs->ClearPref(kTypeRegisteredForInvalidationsDeprecated);
}

}  // namespace

// static
void PerUserTopicRegistrationManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTypeRegisteredForInvalidationsDeprecated);
  registry->RegisterStringPref(kActiveRegistrationTokenDeprecated,
                               std::string());

  registry->RegisterDictionaryPref(kTypeRegisteredForInvalidations);
  registry->RegisterDictionaryPref(kActiveRegistrationTokens);
}

// static
void PerUserTopicRegistrationManager::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTypeRegisteredForInvalidationsDeprecated);
  registry->RegisterStringPref(kActiveRegistrationTokenDeprecated,
                               std::string());

  registry->RegisterDictionaryPref(kTypeRegisteredForInvalidations);
  registry->RegisterDictionaryPref(kActiveRegistrationTokens);
}

struct PerUserTopicRegistrationManager::RegistrationEntry {
  RegistrationEntry(const Topic& topic,
                    SubscriptionFinishedCallback completion_callback,
                    PerUserTopicRegistrationRequest::RequestType type,
                    bool topic_is_public = false);
  ~RegistrationEntry();

  void RegistrationFinished(const Status& code,
                            const std::string& private_topic_name);
  void Cancel();

  // The object for which this is the status.
  const Topic topic;
  const bool topic_is_public;
  SubscriptionFinishedCallback completion_callback;
  PerUserTopicRegistrationRequest::RequestType type;

  base::OneShotTimer request_retry_timer_;
  net::BackoffEntry request_backoff_;

  std::unique_ptr<PerUserTopicRegistrationRequest> request;

  DISALLOW_COPY_AND_ASSIGN(RegistrationEntry);
};

PerUserTopicRegistrationManager::RegistrationEntry::RegistrationEntry(
    const Topic& topic,
    SubscriptionFinishedCallback completion_callback,
    PerUserTopicRegistrationRequest::RequestType type,
    bool topic_is_public)
    : topic(topic),
      topic_is_public(topic_is_public),
      completion_callback(std::move(completion_callback)),
      type(type),
      request_backoff_(&kBackoffPolicy) {}

PerUserTopicRegistrationManager::RegistrationEntry::~RegistrationEntry() {}

void PerUserTopicRegistrationManager::RegistrationEntry::RegistrationFinished(
    const Status& code,
    const std::string& topic_name) {
  if (completion_callback)
    std::move(completion_callback).Run(topic, code, topic_name, type);
}

void PerUserTopicRegistrationManager::RegistrationEntry::Cancel() {
  request_retry_timer_.Stop();
  request.reset();
}

PerUserTopicRegistrationManager::PerUserTopicRegistrationManager(
    invalidation::IdentityProvider* identity_provider,
    PrefService* local_state,
    network::mojom::URLLoaderFactory* url_loader_factory,
    const std::string& project_id,
    bool migrate_prefs)
    : local_state_(local_state),
      identity_provider_(identity_provider),
      request_access_token_backoff_(&kBackoffPolicy),
      url_loader_factory_(url_loader_factory),
      project_id_(project_id),
      migrate_prefs_(migrate_prefs) {}

PerUserTopicRegistrationManager::~PerUserTopicRegistrationManager() {}

// static
std::unique_ptr<PerUserTopicRegistrationManager>
PerUserTopicRegistrationManager::Create(
    invalidation::IdentityProvider* identity_provider,
    PrefService* local_state,
    network::mojom::URLLoaderFactory* url_loader_factory,
    const std::string& project_id,
    bool migrate_prefs) {
  return std::make_unique<PerUserTopicRegistrationManager>(
      identity_provider, local_state, url_loader_factory, project_id,
      migrate_prefs);
}

void PerUserTopicRegistrationManager::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (migrate_prefs_) {
    MigratePrefs(local_state_, project_id_);
  }
  PerProjectDictionaryPrefUpdate update(local_state_, project_id_);
  if (update->DictEmpty()) {
    return;
  }

  std::vector<std::string> keys_to_remove;
  // Load registered ids from prefs.
  for (const auto& it : update->DictItems()) {
    Topic topic = it.first;
    std::string private_topic_name;
    if (it.second.GetAsString(&private_topic_name) &&
        !private_topic_name.empty()) {
      topic_to_private_topic_[topic] = private_topic_name;
      private_topic_to_topic_[private_topic_name] = topic;
      continue;
    }
    // Remove saved pref.
    keys_to_remove.push_back(topic);
  }

  // Delete prefs, which weren't decoded successfully.
  for (const std::string& key : keys_to_remove) {
    update->RemoveKey(key);
  }
}

void PerUserTopicRegistrationManager::UpdateRegisteredTopics(
    const Topics& topics,
    const std::string& instance_id_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  instance_id_token_ = instance_id_token;
  DropAllSavedRegistrationsOnTokenChange();

  for (const auto& topic : topics) {
    // If the topic isn't registered yet, schedule the registration.
    if (topic_to_private_topic_.find(topic.first) ==
        topic_to_private_topic_.end()) {
      auto it = registration_statuses_.find(topic.first);
      if (it != registration_statuses_.end())
        it->second->Cancel();
      registration_statuses_[topic.first] = std::make_unique<RegistrationEntry>(
          topic.first,
          base::BindOnce(
              &PerUserTopicRegistrationManager::RegistrationFinishedForTopic,
              base::Unretained(this)),
          PerUserTopicRegistrationRequest::SUBSCRIBE, topic.second.is_public);
    }
  }

  // There may be registered topics which need to be unregistered.
  // Schedule unregistration and immediately remove from
  // |topic_to_private_topic_| and |private_topic_to_topic_|.
  for (auto it = topic_to_private_topic_.begin();
       it != topic_to_private_topic_.end();) {
    Topic topic = it->first;
    if (topics.find(topic) == topics.end()) {
      registration_statuses_[topic] = std::make_unique<RegistrationEntry>(
          topic,
          base::BindOnce(
              &PerUserTopicRegistrationManager::RegistrationFinishedForTopic,
              base::Unretained(this)),
          PerUserTopicRegistrationRequest::UNSUBSCRIBE);
      private_topic_to_topic_.erase(it->second);
      it = topic_to_private_topic_.erase(it);
      // The decision to unregister from the invalidations for the |topic| was
      // made, the preferences should be cleaned up immediately.
      PerProjectDictionaryPrefUpdate update(local_state_, project_id_);
      update->RemoveKey(topic);
    } else {
      // Topic is still wanted, nothing to do.
      ++it;
    }
  }

  // Kick off the process of actually processing the (un)registrations we just
  // scheduled.
  RequestAccessToken();
}

void PerUserTopicRegistrationManager::DoRegistrationUpdate() {
  for (const auto& registration_status : registration_statuses_) {
    StartRegistrationRequest(registration_status.first);
  }
}

void PerUserTopicRegistrationManager::StartRegistrationRequest(
    const Topic& topic) {
  auto it = registration_statuses_.find(topic);
  if (it == registration_statuses_.end()) {
    NOTREACHED() << "StartRegistrationRequest called on " << topic
                 << " which is not in the registration map";
    return;
  }
  PerUserTopicRegistrationRequest::Builder builder;
  it->second->request.reset();  // Resetting request in case it's running.
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
      base::BindOnce(&PerUserTopicRegistrationManager::RegistrationEntry::
                         RegistrationFinished,
                     base::Unretained(it->second.get())),
      url_loader_factory_);
}

void PerUserTopicRegistrationManager::ActOnSuccesfullRegistration(
    const Topic& topic,
    const std::string& private_topic_name,
    PerUserTopicRegistrationRequest::RequestType type) {
  auto it = registration_statuses_.find(topic);
  it->second->request_backoff_.InformOfRequest(true);
  registration_statuses_.erase(it);
  if (type == PerUserTopicRegistrationRequest::SUBSCRIBE) {
    {
      PerProjectDictionaryPrefUpdate update(local_state_, project_id_);
      update->SetKey(topic, base::Value(private_topic_name));
      topic_to_private_topic_[topic] = private_topic_name;
      private_topic_to_topic_[private_topic_name] = topic;
    }
    local_state_->CommitPendingWrite();
  }
  bool all_subscription_completed = true;
  for (const auto& entry : registration_statuses_) {
    if (entry.second->type == PerUserTopicRegistrationRequest::SUBSCRIBE) {
      all_subscription_completed = false;
    }
  }
  // Emit ENABLED once we recovered from failed request.
  if (all_subscription_completed &&
      base::FeatureList::IsEnabled(
          invalidation::switches::kFCMInvalidationsConservativeEnabling)) {
    NotifySubscriptionChannelStateChange(SubscriptionChannelState::ENABLED);
  }
}

void PerUserTopicRegistrationManager::ScheduleRequestForRepetition(
    const Topic& topic) {
  registration_statuses_[topic]->completion_callback = base::BindOnce(
      &PerUserTopicRegistrationManager::RegistrationFinishedForTopic,
      base::Unretained(this));
  // TODO(treib): We already called InformOfRequest(false) before in
  // RegistrationFinishedForTopic(), should probably not call it again here?
  registration_statuses_[topic]->request_backoff_.InformOfRequest(false);
  registration_statuses_[topic]->request_retry_timer_.Start(
      FROM_HERE,
      registration_statuses_[topic]->request_backoff_.GetTimeUntilRelease(),
      base::BindRepeating(
          &PerUserTopicRegistrationManager::StartRegistrationRequest,
          base::Unretained(this), topic));
}

void PerUserTopicRegistrationManager::RegistrationFinishedForTopic(
    Topic topic,
    Status code,
    std::string private_topic_name,
    PerUserTopicRegistrationRequest::RequestType type) {
  if (code.IsSuccess()) {
    ActOnSuccesfullRegistration(topic, private_topic_name, type);
  } else {
    auto it = registration_statuses_.find(topic);
    it->second->request_backoff_.InformOfRequest(false);
    if (code.IsAuthFailure()) {
      // Re-request access token and fire registrations again.
      RequestAccessToken();
    } else {
      // If one of the registration requests failed, emit SUBSCRIPTION_FAILURE.
      if (type == PerUserTopicRegistrationRequest::SUBSCRIBE &&
          base::FeatureList::IsEnabled(
              invalidation::switches::kFCMInvalidationsConservativeEnabling)) {
        NotifySubscriptionChannelStateChange(
            SubscriptionChannelState::SUBSCRIPTION_FAILURE);
      }
      if (!code.ShouldRetry()) {
        registration_statuses_.erase(it);
        return;
      }
      ScheduleRequestForRepetition(topic);
    }
  }
}

TopicSet PerUserTopicRegistrationManager::GetRegisteredIds() const {
  TopicSet topics;
  for (const auto& t : topic_to_private_topic_)
    topics.insert(t.first);

  return topics;
}

void PerUserTopicRegistrationManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PerUserTopicRegistrationManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PerUserTopicRegistrationManager::RequestAccessToken() {
  // TODO(melandory): Implement traffic optimisation.
  // * Before sending request to server ask for access token from identity
  //   provider (don't invalidate previous token).
  //   Identity provider will take care of retrieving/caching.
  // * Only invalidate access token when server didn't accept it.

  // Only one active request at a time.
  if (access_token_fetcher_ != nullptr)
    return;
  request_access_token_retry_timer_.Stop();
  OAuth2AccessTokenManager::ScopeSet oauth2_scopes = {kFCMOAuthScope};
  // Invalidate previous token, otherwise the identity provider will return the
  // same token again.
  identity_provider_->InvalidateAccessToken(oauth2_scopes, access_token_);
  access_token_.clear();
  access_token_fetcher_ = identity_provider_->FetchAccessToken(
      "fcm_invalidation", oauth2_scopes,
      base::BindOnce(
          &PerUserTopicRegistrationManager::OnAccessTokenRequestCompleted,
          base::Unretained(this)));
}

void PerUserTopicRegistrationManager::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    std::string access_token) {
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE)
    OnAccessTokenRequestSucceeded(access_token);
  else
    OnAccessTokenRequestFailed(error);
}

void PerUserTopicRegistrationManager::OnAccessTokenRequestSucceeded(
    std::string access_token) {
  // Reset backoff time after successful response.
  request_access_token_backoff_.Reset();
  access_token_ = access_token;
  // Emit ENABLED when successfully got the token.
  NotifySubscriptionChannelStateChange(SubscriptionChannelState::ENABLED);
  DoRegistrationUpdate();
}

void PerUserTopicRegistrationManager::OnAccessTokenRequestFailed(
    GoogleServiceAuthError error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  NotifySubscriptionChannelStateChange(
      SubscriptionChannelState::ACCESS_TOKEN_FAILURE);
  request_access_token_backoff_.InformOfRequest(false);
  request_access_token_retry_timer_.Start(
      FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
      base::BindRepeating(&PerUserTopicRegistrationManager::RequestAccessToken,
                          base::Unretained(this)));
}

void PerUserTopicRegistrationManager::DropAllSavedRegistrationsOnTokenChange() {
  {
    DictionaryPrefUpdate token_update(local_state_, kActiveRegistrationTokens);
    std::string current_token;
    token_update->GetString(project_id_, &current_token);
    if (current_token.empty()) {
      token_update->SetString(project_id_, instance_id_token_);
      return;
    }
    if (current_token == instance_id_token_) {
      return;
    }
    token_update->SetString(project_id_, instance_id_token_);
  }

  PerProjectDictionaryPrefUpdate update(local_state_, project_id_);
  *update = base::Value(base::Value::Type::DICTIONARY);
  topic_to_private_topic_.clear();
  private_topic_to_topic_.clear();
  // TODO(melandory): Figure out if the unsubscribe request should be
  // sent with the old token.
}

void PerUserTopicRegistrationManager::NotifySubscriptionChannelStateChange(
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

base::DictionaryValue PerUserTopicRegistrationManager::CollectDebugData()
    const {
  base::DictionaryValue status;
  for (const auto& topic_to_private_topic : topic_to_private_topic_) {
    status.SetString(topic_to_private_topic.first,
                     topic_to_private_topic.second);
  }
  status.SetString("Instance id token", instance_id_token_);
  return status;
}

base::Optional<Topic>
PerUserTopicRegistrationManager::LookupRegisteredPublicTopicByPrivateTopic(
    const std::string& private_topic) const {
  auto it = private_topic_to_topic_.find(private_topic);
  if (it == private_topic_to_topic_.end()) {
    return base::nullopt;
  }
  return it->second;
}

}  // namespace syncer
