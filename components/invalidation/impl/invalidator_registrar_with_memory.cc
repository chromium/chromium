// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace invalidation {

namespace {

constexpr char kTopicsToHandler[] = "invalidation.per_sender_topics_to_handler";
constexpr char kDeprecatedSyncInvalidationGCMSenderId[] = "8181035976";

constexpr char kHandler[] = "handler";
constexpr char kIsPublic[] = "is_public";

absl::optional<TopicData> FindAnyDuplicatedTopic(
    const std::set<TopicData>& lhs,
    const std::set<TopicData>& rhs) {
  auto intersection =
      base::STLSetIntersection<std::vector<TopicData>>(lhs, rhs);
  if (!intersection.empty()) {
    return intersection[0];
  }
  return absl::nullopt;
}

}  // namespace

BASE_FEATURE(kRestoreInterestingTopicsFeature,
             "InvalidatorRestoreInterestingTopics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
void InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTopicsToHandler);
}

// static
void InvalidatorRegistrarWithMemory::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // For local state, we want to register exactly the same prefs as for profile
  // prefs; see comment in the header.
  RegisterProfilePrefs(registry);
}

// static
void InvalidatorRegistrarWithMemory::ClearDeprecatedPrefs(PrefService* prefs) {
  if (prefs->HasPrefPath(kTopicsToHandler)) {
    ScopedDictPrefUpdate update(prefs, kTopicsToHandler);
    update->Remove(kDeprecatedSyncInvalidationGCMSenderId);
  }
}

InvalidatorRegistrarWithMemory::InvalidatorRegistrarWithMemory(
    PrefService* prefs,
    const std::string& sender_id)
    : state_(DEFAULT_INVALIDATION_ERROR), prefs_(prefs), sender_id_(sender_id) {
  CHECK(!sender_id_.empty());
  const base::Value::Dict* pref_data =
      prefs_->GetDict(kTopicsToHandler).FindDict(sender_id_);
  if (!pref_data) {
    ScopedDictPrefUpdate update(prefs_, kTopicsToHandler);
    update->Set(sender_id_, base::Value::Dict());
    return;
  }
  // Restore |handler_name_to_subscribed_topics_map_| from prefs.
  if (!base::FeatureList::IsEnabled(kRestoreInterestingTopicsFeature))
    return;
  for (auto it : *pref_data) {
    const std::string& topic_name = it.first;
    if (it.second.is_dict()) {
      const base::Value::Dict& second_dict = it.second.GetDict();
      const std::string* handler = second_dict.FindString(kHandler);
      const absl::optional<bool> is_public = second_dict.FindBool(kIsPublic);
      if (!handler || !is_public) {
        continue;
      }
      handler_name_to_subscribed_topics_map_[*handler].insert(
          TopicData(topic_name, *is_public));
    } else if (it.second.is_string()) {
      handler_name_to_subscribed_topics_map_[it.second.GetString()].insert(
          TopicData(topic_name, false));
    }
  }
}

InvalidatorRegistrarWithMemory::~InvalidatorRegistrarWithMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(registered_handler_to_topics_map_.empty());
}

void InvalidatorRegistrarWithMemory::RegisterHandler(
    InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(handler);
  CHECK(!handlers_.HasObserver(handler));
  handlers_.AddObserver(handler);
}

void InvalidatorRegistrarWithMemory::UnregisterHandler(
    InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));
  handlers_.RemoveObserver(handler);
  registered_handler_to_topics_map_.erase(handler);
  // Note: Do *not* remove the entry from
  // |handler_name_to_subscribed_topics_map_| - we haven't actually unsubscribed
  // from any of the topics on the server, so GetAllSubscribedTopics() should
  // still return the topics.
}

bool InvalidatorRegistrarWithMemory::UpdateRegisteredTopics(
    InvalidationHandler* handler,
    const std::set<TopicData>& topics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));

  if (HasDuplicateTopicRegistration(handler, topics)) {
    return false;
  }

  std::set<TopicData> old_topics = registered_handler_to_topics_map_[handler];
  if (topics.empty()) {
    registered_handler_to_topics_map_.erase(handler);
  } else {
    registered_handler_to_topics_map_[handler] = topics;
  }

  // This does *not* remove subscribed topics which are not registered. This
  // behaviour is used by some handlers to keep topic subscriptions after
  // browser startup even if they are not included in the first call of this
  // method. It's useful to prevent unsubscribing from and subscribing to the
  // topics on each browser startup.
  //
  // TODO(crbug.com/1051893): make the unsubscription behaviour consistent
  // regardless of browser restart in between.
  auto topics_to_unregister =
      base::STLSetDifference<std::set<TopicData>>(old_topics, topics);
  RemoveSubscribedTopics(handler, topics_to_unregister);

  ScopedDictPrefUpdate update(prefs_, kTopicsToHandler);
  base::Value::Dict* pref_data = update->FindDict(sender_id_);
  for (const auto& topic : topics) {
    handler_name_to_subscribed_topics_map_[handler->GetOwnerName()].insert(
        topic);
    base::Value::Dict handler_pref;
    handler_pref.Set(kHandler, handler->GetOwnerName());
    handler_pref.Set(kIsPublic, topic.is_public);
    pref_data->Set(topic.name, std::move(handler_pref));
  }
  return true;
}

TopicMap InvalidatorRegistrarWithMemory::GetRegisteredTopics(
    InvalidationHandler* handler) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lookup = registered_handler_to_topics_map_.find(handler);
  return lookup != registered_handler_to_topics_map_.end()
             ? ConvertTopicSetToLegacyTopicMap(lookup->second)
             : TopicMap();
}

TopicMap InvalidatorRegistrarWithMemory::GetAllSubscribedTopics() const {
  std::set<TopicData> subscribed_topics;
  for (const auto& handler_to_topic : handler_name_to_subscribed_topics_map_) {
    subscribed_topics.insert(handler_to_topic.second.begin(),
                             handler_to_topic.second.end());
  }
  return ConvertTopicSetToLegacyTopicMap(subscribed_topics);
}

void InvalidatorRegistrarWithMemory::DispatchInvalidationToHandlers(
    const Invalidation& invalidation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we have no handlers, there's nothing to do.
  if (handlers_.empty()) {
    return;
  }

  // Each handler has a set of registered topics. In order to send the incoming
  // invalidation to the correct handlers we are going through each handler and
  // each of their sets of topics.
  for (const auto& [handler, registered_topics] :
       registered_handler_to_topics_map_) {
    for (const auto& registered_topic : registered_topics) {
      // If the topic of the invalidation matches a registered topic, we send
      // the invalidation to the respective handler.
      if (invalidation.topic() != registered_topic.name) {
        continue;
      }
      handler->OnIncomingInvalidation(invalidation);
    }
  }
}

void InvalidatorRegistrarWithMemory::UpdateInvalidatorState(
    InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "New invalidator state: " << InvalidatorStateToString(state_)
           << " -> " << InvalidatorStateToString(state);
  state_ = state;
  for (auto& observer : handlers_)
    observer.OnInvalidatorStateChange(state);
}

InvalidatorState InvalidatorRegistrarWithMemory::GetInvalidatorState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

bool InvalidatorRegistrarWithMemory::HasDuplicateTopicRegistration(
    InvalidationHandler* handler,
    const std::set<TopicData>& topics) const {
  for (const auto& handler_and_topics : registered_handler_to_topics_map_) {
    if (handler_and_topics.first == handler) {
      continue;
    }

    if (absl::optional<TopicData> duplicate =
            FindAnyDuplicatedTopic(topics, handler_and_topics.second)) {
      DVLOG(1) << "Duplicate registration: trying to register "
               << duplicate->name << " for " << handler
               << " when it's already registered for "
               << handler_and_topics.first;
      return true;
    }
  }
  return false;
}

void InvalidatorRegistrarWithMemory::RemoveSubscribedTopics(
    const InvalidationHandler* handler,
    const std::set<TopicData>& topics_to_unsubscribe) {
  ScopedDictPrefUpdate update(prefs_, kTopicsToHandler);
  base::Value::Dict* pref_data = update->FindDict(sender_id_);
  DCHECK(pref_data);
  for (const TopicData& topic : topics_to_unsubscribe) {
    pref_data->Remove(topic.name);
    handler_name_to_subscribed_topics_map_[handler->GetOwnerName()].erase(
        topic);
  }
}

}  // namespace invalidation
