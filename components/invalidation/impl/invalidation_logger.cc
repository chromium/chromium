// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_logger.h"

#include "base/values.h"
#include "components/invalidation/impl/invalidation_logger_observer.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {
class InvalidationLoggerObserver;

InvalidationLogger::InvalidationLogger()
    : last_invalidator_state_(syncer::TRANSIENT_INVALIDATION_ERROR),
      last_invalidator_state_timestamp_(base::Time::Now()) { }

InvalidationLogger::~InvalidationLogger() {}

void InvalidationLogger::OnRegistration(const std::string& registrar_name) {
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

void InvalidationLogger::OnStateChange(
    const syncer::InvalidatorState& new_state) {
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

void InvalidationLogger::OnUpdateTopics(
    std::map<std::string, syncer::Topics> updated_topics) {
  for (const auto& updated_topic : updated_topics) {
    latest_ids_[updated_topic.first] =
        syncer::ConvertTopicsToIds(updated_topic.second);
  }
  EmitUpdatedIds();
}

void InvalidationLogger::OnUpdateIds(
    std::map<std::string, syncer::ObjectIdSet> updated_ids) {
  for (std::map<std::string, syncer::ObjectIdSet>::const_iterator it =
       updated_ids.begin(); it != updated_ids.end(); ++it) {
    latest_ids_[it->first] = syncer::ObjectIdSet(it->second);
  }
  EmitUpdatedIds();
}

void InvalidationLogger::EmitUpdatedIds() {
  for (std::map<std::string, syncer::ObjectIdSet>::const_iterator it =
       latest_ids_.begin(); it != latest_ids_.end(); ++it) {
    const syncer::ObjectIdSet& object_ids_for_handler = it->second;
    syncer::ObjectIdCountMap per_object_invalidation_count;
    for (auto oid_it = object_ids_for_handler.begin();
         oid_it != object_ids_for_handler.end(); ++oid_it) {
      per_object_invalidation_count[*oid_it] = invalidation_count_[*oid_it];
    }
    for (auto& observer : observer_list_)
      observer.OnUpdateIds(it->first, per_object_invalidation_count);
  }
}

void InvalidationLogger::OnDebugMessage(const base::DictionaryValue& details) {
  for (auto& observer : observer_list_)
    observer.OnDebugMessage(details);
}

void InvalidationLogger::OnInvalidation(
    const syncer::ObjectIdInvalidationMap& details) {
  std::vector<syncer::Invalidation> internal_invalidations;
  details.GetAllInvalidations(&internal_invalidations);
  for (std::vector<syncer::Invalidation>::const_iterator it =
           internal_invalidations.begin();
       it != internal_invalidations.end();
       ++it) {
    invalidation_count_[it->object_id()]++;
  }
  for (auto& observer : observer_list_)
    observer.OnInvalidation(details);
}

void InvalidationLogger::EmitContent() {
  EmitState();
  EmitUpdatedIds();
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
