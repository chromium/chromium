// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_logger.h"

#include "base/observer_list.h"
#include "base/values.h"
#include "components/invalidation/impl/invalidation_logger_observer.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace invalidation {

InvalidationLogger::InvalidationLogger()
    : last_invalidator_state_timestamp_(base::Time::Now()) {}

InvalidationLogger::~InvalidationLogger() = default;

void InvalidationLogger::OnRegistration(const std::string& registrar_name) {
  DCHECK(registered_handlers_.find(registrar_name) ==
         registered_handlers_.end());
  registered_handlers_.insert(registrar_name);
  EmitRegisteredHandlers();
}

void InvalidationLogger::OnUnregistration(const std::string& registrar_name) {
  DCHECK(registered_handlers_.find(registrar_name) !=
         registered_handlers_.end());
  auto it = registered_handlers_.find(registrar_name);
  // Delete only one instance of registrar_name.
  registered_handlers_.erase(it);
  EmitRegisteredHandlers();
}

void InvalidationLogger::EmitRegisteredHandlers() {
  for (auto& observer : observer_list_)
    observer.OnRegistrationChange(registered_handlers_);
}

void InvalidationLogger::OnStateChange(const InvalidatorState& new_state) {
  // Prevent spurious same state emissions from updating the timestamp.
  if (new_state != last_invalidator_state_)
    last_invalidator_state_timestamp_ = base::Time::Now();
  last_invalidator_state_ = new_state;
  EmitState();
}

void InvalidationLogger::EmitState() {
  for (auto& observer : observer_list_) {
    observer.OnStateChange(last_invalidator_state_,
                           last_invalidator_state_timestamp_);
  }
}

void InvalidationLogger::OnUpdatedTopics(
    std::map<std::string, Topics> handler_updated_topics_map) {
  for (const auto& updated_topics : handler_updated_topics_map) {
    handler_latest_topics_map_[updated_topics.first] = updated_topics.second;
  }
  EmitUpdatedTopics();
}

void InvalidationLogger::EmitUpdatedTopics() {
  for (const auto& handler_name_and_topics : handler_latest_topics_map_) {
    TopicCountMap per_handler_invalidation_count;
    for (const auto& topic_item : handler_name_and_topics.second) {
      const Topic& topic = topic_item.first;
      per_handler_invalidation_count[topic] = invalidation_count_[topic];
    }
    for (auto& observer : observer_list_) {
      observer.OnUpdatedTopics(handler_name_and_topics.first,
                               per_handler_invalidation_count);
    }
  }
}

void InvalidationLogger::OnDebugMessage(const base::Value::Dict& details) {
  for (auto& observer : observer_list_)
    observer.OnDebugMessage(details);
}

void InvalidationLogger::OnInvalidation(
    const TopicInvalidationMap& invalidations) {
  for (const auto& topic : invalidations.GetTopics()) {
    invalidation_count_[topic] += invalidations.ForTopic(topic).GetSize();
  }
  for (auto& observer : observer_list_)
    observer.OnInvalidation(invalidations);
}

void InvalidationLogger::EmitContent() {
  EmitState();
  EmitUpdatedTopics();
  EmitRegisteredHandlers();
}

void InvalidationLogger::RegisterObserver(
    InvalidationLoggerObserver* debug_observer) {
  observer_list_.AddObserver(debug_observer);
}

void InvalidationLogger::UnregisterObserver(
    InvalidationLoggerObserver* debug_observer) {
  observer_list_.RemoveObserver(debug_observer);
}

bool InvalidationLogger::IsObserverRegistered(
    const InvalidationLoggerObserver* debug_observer) const {
  return observer_list_.HasObserver(debug_observer);
}

}  // namespace invalidation
