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

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {

namespace {

const char kTypeRegisteredForInvalidation[] =
    "invalidation.registered_for_invalidation";

const char kActiveRegistrationToken[] =
    "invalidation.active_registration_token";

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kProjectId[] = "8181035976";

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

}  // namespace

// static
void PerUserTopicRegistrationManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTypeRegisteredForInvalidation);
  registry->RegisterStringPref(kActiveRegistrationToken, std::string());
}

struct PerUserTopicRegistrationManager::RegistrationEntry {
  RegistrationEntry(const Topic& id,
                    SubscriptionFinishedCallback completion_callback,
                    PerUserTopicRegistrationRequest::RequestType type);
  ~RegistrationEntry();

  void RegistrationFinished(const Status& code,
                            const std::string& private_topic_name);
  void Cancel();

  // The object for which this is the status.
  const Topic topic;
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
    PerUserTopicRegistrationRequest::RequestType type)
    : topic(topic),
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
    const ParseJSONCallback& parse_json)
    : local_state_(local_state),
      identity_provider_(identity_provider),
      request_access_token_backoff_(&kBackoffPolicy),
      parse_json_(parse_json),
      url_loader_factory_(url_loader_factory) {}

PerUserTopicRegistrationManager::~PerUserTopicRegistrationManager() {}

void PerUserTopicRegistrationManager::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* pref_data =
      local_state_->Get(kTypeRegisteredForInvalidation);
  std::vector<std::string> keys_to_remove;
  // Load registered ids from prefs.
  for (const auto& it : pref_data->DictItems()) {
    Topic topic = it.first;
    std::string private_topic_name;
    if (it.second.GetAsString(&private_topic_name) &&
        !private_topic_name.empty()) {
      topic_to_private_topic_[topic] = private_topic_name;
      continue;
    }
    // Remove saved pref.
    keys_to_remove.push_back(topic);
  }

  // Delete prefs, which weren't decoded successfully.
  DictionaryPrefUpdate update(local_state_, kTypeRegisteredForInvalidation);
  base::DictionaryValue* pref_update = update.Get();
  for (const std::string& key : keys_to_remove) {
    pref_update->RemoveKey(key);
  }
}

void PerUserTopicRegistrationManager::UpdateRegisteredTopics(
    const TopicSet& topics,
    const std::string& instance_id_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_ = instance_id_token;
  DropAllSavedRegistrationsOnTokenChange(instance_id_token);
  for (const auto& topic : topics) {
    // If id isn't registered, schedule the registration.
    if (topic_to_private_topic_.find(topic) == topic_to_private_topic_.end()) {
      auto it = registration_statuses_.find(topic);
      if (it != registration_statuses_.end())
        it->second->Cancel();
      registration_statuses_[topic] = std::make_unique<RegistrationEntry>(
          topic,
          base::BindOnce(
              &PerUserTopicRegistrationManager::RegistrationFinishedForTopic,
              base::Unretained(this)),
          PerUserTopicRegistrationRequest::SUBSCRIBE);
    }
  }

  // There is registered topic, which need to be unregistered.
  // Schedule unregistration and immediately remove from
  // |topic_to_private_topic_|
  for (auto it = topic_to_private_topic_.begin();
       it != topic_to_private_topic_.end();) {
    auto topic = it->first;
    if (topics.find(topic) == topics.end()) {
      registration_statuses_[topic] = std::make_unique<RegistrationEntry>(
          topic,
          base::BindOnce(
              &PerUserTopicRegistrationManager::RegistrationFinishedForTopic,
              base::Unretained(this)),
          PerUserTopicRegistrationRequest::UNSUBSCRIBE);
      it = topic_to_private_topic_.erase(it);
    } else {
      ++it;
    }
  }
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
  it->second->request = builder.SetToken(token_)
                            .SetScope(kInvalidationRegistrationScope)
                            .SetPublicTopicName(topic)
                            .SetAuthenticationHeader(base::StringPrintf(
                                "Bearer %s", access_token_.c_str()))
                            .SetProjectId(kProjectId)
                            .SetType(it->second->type)
                            .Build();
  it->second->request->Start(
      base::BindOnce(&PerUserTopicRegistrationManager::RegistrationEntry::
                         RegistrationFinished,
                     base::Unretained(it->second.get())),
      parse_json_, url_loader_factory_);
}

void PerUserTopicRegistrationManager::RegistrationFinishedForTopic(
    Topic topic,
    Status code,
    std::string private_topic_name,
    PerUserTopicRegistrationRequest::RequestType type) {
  if (code.IsSuccess()) {
    auto it = registration_statuses_.find(topic);
    registration_statuses_.erase(it);
    DictionaryPrefUpdate update(local_state_, kTypeRegisteredForInvalidation);
    switch (type) {
      case PerUserTopicRegistrationRequest::SUBSCRIBE: {
        update->SetKey(topic, base::Value(private_topic_name));
        topic_to_private_topic_[topic] = private_topic_name;
        break;
      }
      case PerUserTopicRegistrationRequest::UNSUBSCRIBE: {
        update->RemoveKey(topic);
        break;
      }
    }
    local_state_->CommitPendingWrite();
  } else {
    if (code.IsAuthFailure()) {
      // Re-request access token and fire registrations again.
      RequestAccessToken();
    } else {
      auto completition_callback = base::BindOnce(
          &PerUserTopicRegistrationManager::RegistrationFinishedForTopic,
          base::Unretained(this));
      registration_statuses_[topic]->completion_callback =
          std::move(completition_callback);
      registration_statuses_[topic]->request_backoff_.InformOfRequest(false);
      registration_statuses_[topic]->request_retry_timer_.Start(
          FROM_HERE,
          registration_statuses_[topic]->request_backoff_.GetTimeUntilRelease(),
          base::BindRepeating(
              &PerUserTopicRegistrationManager::StartRegistrationRequest,
              base::Unretained(this), topic));
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
  OAuth2TokenService::ScopeSet oauth2_scopes = {kFCMOAuthScope};
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
  NotifySubscriptionChannelStateChange(INVALIDATIONS_ENABLED);
  DoRegistrationUpdate();
}

void PerUserTopicRegistrationManager::OnAccessTokenRequestFailed(
    GoogleServiceAuthError error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  NotifySubscriptionChannelStateChange(INVALIDATION_CREDENTIALS_REJECTED);
  request_access_token_backoff_.InformOfRequest(false);
  request_access_token_retry_timer_.Start(
      FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
      base::BindRepeating(&PerUserTopicRegistrationManager::RequestAccessToken,
                          base::Unretained(this)));
}

void PerUserTopicRegistrationManager::DropAllSavedRegistrationsOnTokenChange(
    const std::string& instance_id_token) {
  std::string current_token = local_state_->GetString(kActiveRegistrationToken);
  if (current_token.empty()) {
    local_state_->SetString(kActiveRegistrationToken, instance_id_token);
    return;
  }
  if (current_token == instance_id_token) {
    return;
  }
  local_state_->SetString(kActiveRegistrationToken, instance_id_token);
  DictionaryPrefUpdate update(local_state_, kTypeRegisteredForInvalidation);
  for (const auto& topic : topic_to_private_topic_) {
    update->RemoveKey(topic.first);
  }
  topic_to_private_topic_.clear();
  // TODO(melandory): Figure out if the unsubscribe request should be
  // sent with the old token.
}

void PerUserTopicRegistrationManager::NotifySubscriptionChannelStateChange(
    InvalidatorState invalidator_state) {
  for (auto& observer : observers_)
    observer.OnSubscriptionChannelStateChanged(invalidator_state);
}

}  // namespace syncer
