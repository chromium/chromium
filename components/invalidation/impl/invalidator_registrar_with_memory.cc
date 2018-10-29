// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <cstddef>
#include <iterator>
#include <utility>

#include "base/logging.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {

namespace {

const char kTopicsToHandler[] = "invalidation.topics_to_handler";

}  // namespace

// static
void InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTopicsToHandler);
}

InvalidatorRegistrarWithMemory::InvalidatorRegistrarWithMemory(
    PrefService* local_state)
    : InvalidatorRegistrar(), local_state_(local_state) {
  const base::Value* pref_data = local_state_->Get(kTopicsToHandler);
  for (const auto& it : pref_data->DictItems()) {
    Topic topic = it.first;
    std::string handler_name;
    it.second.GetAsString(&handler_name);
    handler_name_to_topics_map_[handler_name].insert(topic);
  }
}

InvalidatorRegistrarWithMemory::~InvalidatorRegistrarWithMemory() {}

bool InvalidatorRegistrarWithMemory::UpdateRegisteredTopics(
    InvalidationHandler* handler,
    const TopicSet& topics) {
  TopicSet old_topics = GetRegisteredTopics(handler);
  bool success = InvalidatorRegistrar::UpdateRegisteredTopics(handler, topics);
  if (!InvalidatorRegistrar::IsHandlerRegistered(handler)) {
    return success;
  }

  TopicSet to_unregister;
  DictionaryPrefUpdate update(local_state_, kTopicsToHandler);
  std::set_difference(old_topics.begin(), old_topics.end(), topics.begin(),
                      topics.end(),
                      std::inserter(to_unregister, to_unregister.begin()));
  if (!to_unregister.empty()) {
    for (const auto& topic : to_unregister) {
      update->RemoveKey(topic);
      handler_name_to_topics_map_[handler->GetOwnerName()].erase(topic);
    }
  }

  for (const auto& topic : topics) {
    handler_name_to_topics_map_[handler->GetOwnerName()].insert(topic);
    update->SetKey(topic, base::Value(handler->GetOwnerName()));
  }
  return success;
}

TopicSet InvalidatorRegistrarWithMemory::GetAllRegisteredIds() const {
  TopicSet registered_topics;
  for (const auto& handler_to_topic : handler_name_to_topics_map_) {
    registered_topics.insert(handler_to_topic.second.begin(),
                             handler_to_topic.second.end());
  }
  return registered_topics;
}

}  // namespace syncer
