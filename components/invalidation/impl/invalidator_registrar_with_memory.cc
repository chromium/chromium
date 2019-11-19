// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {

namespace {

constexpr char kTopicsToHandlerDeprecated[] = "invalidation.topics_to_handler";

constexpr char kTopicsToHandler[] = "invalidation.per_sender_topics_to_handler";

constexpr char kHandler[] = "handler";
constexpr char kIsPublic[] = "is_public";

// Added in M76.
void MigratePrefs(PrefService* prefs, const std::string& sender_id) {
  auto* old_prefs = prefs->GetDictionary(kTopicsToHandlerDeprecated);
  if (old_prefs->empty()) {
    return;
  }
  {
    DictionaryPrefUpdate update(prefs, kTopicsToHandler);
    update->SetKey(sender_id, old_prefs->Clone());
  }
  prefs->ClearPref(kTopicsToHandlerDeprecated);
}

}  // namespace

// static
void InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTopicsToHandlerDeprecated);
  registry->RegisterDictionaryPref(kTopicsToHandler);
}

// static
void InvalidatorRegistrarWithMemory::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // For local state, we want to register exactly the same prefs as for profile
  // prefs; see comment in the header.
  RegisterProfilePrefs(registry);
}

InvalidatorRegistrarWithMemory::InvalidatorRegistrarWithMemory(
    PrefService* prefs,
    const std::string& sender_id,
    bool migrate_old_prefs)
    : state_(DEFAULT_INVALIDATION_ERROR), prefs_(prefs), sender_id_(sender_id) {
  DCHECK(!sender_id_.empty());
  if (migrate_old_prefs) {
    MigratePrefs(prefs_, sender_id_);
  }
  const base::Value* pref_data =
      prefs_->Get(kTopicsToHandler)->FindDictKey(sender_id_);
  if (!pref_data) {
    DictionaryPrefUpdate update(prefs_, kTopicsToHandler);
    update->SetKey(sender_id_, base::DictionaryValue());
    return;
  }
  // Restore |handler_name_to_subscribed_topics_map_| from prefs.
  for (const auto& it : pref_data->DictItems()) {
    Topic topic = it.first;
    if (it.second.is_dict()) {
      const base::Value* handler = it.second.FindDictKey(kHandler);
      const base::Value* is_public = it.second.FindDictKey(kIsPublic);
      if (!handler || !is_public) {
        continue;
      }
      handler_name_to_subscribed_topics_map_[handler->GetString()].emplace(
          topic, TopicMetadata{is_public->GetBool()});
    } else if (it.second.is_string()) {
      std::string handler_name;
      it.second.GetAsString(&handler_name);
      handler_name_to_subscribed_topics_map_[handler_name].emplace(
          topic, TopicMetadata{false});
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
    const Topics& topics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));

  Topics old_topics = GetRegisteredTopics(handler);

  bool success = !HasDuplicateTopicRegistration(handler, topics);

  if (success) {
    if (topics.empty()) {
      registered_handler_to_topics_map_.erase(handler);
    } else {
      registered_handler_to_topics_map_[handler] = topics;
    }
  }

  // TODO(treib): This seems inconsistent - if there's a duplicate, we don't
  // update |registered_handler_to_topics_map_| but we *do* still update
  // |handler_name_to_subscribed_topics_map_| and the prefs?!

  DictionaryPrefUpdate update(prefs_, kTopicsToHandler);
  base::Value* pref_data = update->FindDictKey(sender_id_);
  // TODO(treib): This does *not* remove subscribed topics which were not
  // registered. Bug or feature?
  auto to_unregister = FindRemovedTopics(old_topics, topics);
  for (const auto& topic : to_unregister) {
    pref_data->RemoveKey(topic);
    handler_name_to_subscribed_topics_map_[handler->GetOwnerName()].erase(
        topic);
  }

  for (const auto& topic : topics) {
    handler_name_to_subscribed_topics_map_[handler->GetOwnerName()].insert(
        topic);
    base::DictionaryValue handler_pref;
    handler_pref.SetStringKey(kHandler, handler->GetOwnerName());
    handler_pref.SetBoolKey(kIsPublic, topic.second.is_public);
    pref_data->SetKey(topic.first, std::move(handler_pref));
  }
  return success;
}

Topics InvalidatorRegistrarWithMemory::GetRegisteredTopics(
    InvalidationHandler* handler) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lookup = registered_handler_to_topics_map_.find(handler);
  return lookup != registered_handler_to_topics_map_.end() ? lookup->second
                                                           : Topics();
}

Topics InvalidatorRegistrarWithMemory::GetAllSubscribedTopics() const {
  Topics registered_topics;
  for (const auto& handler_to_topic : handler_name_to_subscribed_topics_map_) {
    registered_topics.insert(handler_to_topic.second.begin(),
                             handler_to_topic.second.end());
  }
  return registered_topics;
}

void InvalidatorRegistrarWithMemory::DispatchInvalidationsToHandlers(
    const TopicInvalidationMap& invalidation_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we have no handlers, there's nothing to do.
  if (!handlers_.might_have_observers()) {
    return;
  }

  for (const auto& handler_and_topics : registered_handler_to_topics_map_) {
    TopicInvalidationMap topics_to_emit =
        invalidation_map.GetSubsetWithTopics(handler_and_topics.second);
    if (topics_to_emit.Empty()) {
      continue;
    }
    ObjectIdInvalidationMap object_ids_to_emit =
        ConvertTopicInvalidationMapToObjectIdInvalidationMap(topics_to_emit);
    handler_and_topics.first->OnIncomingInvalidation(object_ids_to_emit);
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

void InvalidatorRegistrarWithMemory::UpdateInvalidatorInstanceId(
    const std::string& instance_id) {
  for (auto& observer : handlers_)
    observer.OnInvalidatorClientIdChange(instance_id);
}

std::map<std::string, Topics>
InvalidatorRegistrarWithMemory::GetHandlerNameToTopicsMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, Topics> names_to_topics;
  for (const auto& handler_and_topics : registered_handler_to_topics_map_) {
    names_to_topics[handler_and_topics.first->GetOwnerName()] =
        handler_and_topics.second;
  }
  return names_to_topics;
}

void InvalidatorRegistrarWithMemory::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> callback)
    const {
  callback.Run(CollectDebugData());
}

bool InvalidatorRegistrarWithMemory::HasDuplicateTopicRegistration(
    InvalidationHandler* handler,
    const Topics& topics) const {
  for (const auto& handler_and_topics : registered_handler_to_topics_map_) {
    if (handler_and_topics.first == handler) {
      continue;
    }

    if (auto* duplicate =
            FindMatchingTopic(topics, handler_and_topics.second)) {
      DVLOG(1) << "Duplicate registration: trying to register " << *duplicate
               << " for " << handler << " when it's already registered for "
               << handler_and_topics.first;
      return true;
    }
  }
  return false;
}

base::DictionaryValue InvalidatorRegistrarWithMemory::CollectDebugData() const {
  base::DictionaryValue return_value;
  return_value.SetInteger("InvalidatorRegistrarWithMemory.Handlers",
                          handler_name_to_subscribed_topics_map_.size());
  for (const auto& handler_to_topics : handler_name_to_subscribed_topics_map_) {
    const std::string& handler = handler_to_topics.first;
    for (const auto& topic : handler_to_topics.second) {
      return_value.SetString("InvalidatorRegistrarWithMemory." + topic.first,
                             handler);
    }
  }
  return return_value;
}

}  // namespace syncer
