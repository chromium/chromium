// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar.h"

#include <cstddef>
#include <iterator>
#include <utility>

#include "base/logging.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace syncer {

InvalidatorRegistrar::InvalidatorRegistrar()
    : state_(DEFAULT_INVALIDATION_ERROR) {}

InvalidatorRegistrar::~InvalidatorRegistrar() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler_to_topics_map_.empty());
}

void InvalidatorRegistrar::RegisterHandler(InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(!handlers_.HasObserver(handler));
  handlers_.AddObserver(handler);
}

bool InvalidatorRegistrar::UpdateRegisteredTopics(InvalidationHandler* handler,
                                                  const TopicSet& topics) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));

  for (const auto& handler_and_topics : handler_to_topics_map_) {
    if (handler_and_topics.first == handler) {
      continue;
    }

    std::vector<Topic> intersection;
    std::set_intersection(handler_and_topics.second.begin(),
                          handler_and_topics.second.end(), topics.begin(),
                          topics.end(),
                          std::inserter(intersection, intersection.end()));
    if (!intersection.empty()) {
      DVLOG(1) << "Duplicate registration: trying to register "
               << *intersection.begin() << " for " << handler
               << " when it's already registered for "
               << handler_and_topics.first;
      return false;
    }
  }

  if (topics.empty()) {
    handler_to_topics_map_.erase(handler);
  } else {
    handler_to_topics_map_[handler] = topics;
  }
  return true;
}

void InvalidatorRegistrar::UnregisterHandler(InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));
  handlers_.RemoveObserver(handler);
  handler_to_topics_map_.erase(handler);
}

TopicSet InvalidatorRegistrar::GetRegisteredTopics(
    InvalidationHandler* handler) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto lookup = handler_to_topics_map_.find(handler);
  return lookup != handler_to_topics_map_.end() ? lookup->second : TopicSet();
}

TopicSet InvalidatorRegistrar::GetAllRegisteredIds() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  TopicSet registered_ids;
  for (auto it = handler_to_topics_map_.begin();
       it != handler_to_topics_map_.end(); ++it) {
    registered_ids.insert(it->second.begin(), it->second.end());
  }
  return registered_ids;
}

void InvalidatorRegistrar::DispatchInvalidationsToHandlers(
    const TopicInvalidationMap& invalidation_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If we have no handlers, there's nothing to do.
  if (!handlers_.might_have_observers()) {
    return;
  }

  for (const auto& handler_and_topics : handler_to_topics_map_) {
    TopicInvalidationMap to_emit =
        invalidation_map.GetSubsetWithTopics(handler_and_topics.second);
    ObjectIdInvalidationMap object_ids_to_emit;
    std::vector<syncer::Invalidation> invalidations;
    to_emit.GetAllInvalidations(&invalidations);
    for (const auto& invalidation : invalidations) {
      object_ids_to_emit.Insert(invalidation);
    }
    if (!to_emit.Empty()) {
      handler_and_topics.first->OnIncomingInvalidation(object_ids_to_emit);
    }
  }
}

void InvalidatorRegistrar::UpdateInvalidatorState(InvalidatorState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "New invalidator state: " << InvalidatorStateToString(state_)
           << " -> " << InvalidatorStateToString(state);
  state_ = state;
  for (auto& observer : handlers_)
    observer.OnInvalidatorStateChange(state);
}

InvalidatorState InvalidatorRegistrar::GetInvalidatorState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_;
}

std::map<std::string, TopicSet>
InvalidatorRegistrar::GetSanitizedHandlersIdsMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::map<std::string, TopicSet> clean_handlers_to_topics;
  for (const auto& handler_and_topics : handler_to_topics_map_)
    clean_handlers_to_topics[handler_and_topics.first->GetOwnerName()] =
        TopicSet(handler_and_topics.second);
  return clean_handlers_to_topics;
}

bool InvalidatorRegistrar::IsHandlerRegistered(
    const InvalidationHandler* handler) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return handlers_.HasObserver(handler);
}

void InvalidatorRegistrar::DetachFromThreadForTest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  thread_checker_.DetachFromThread();
}

}  // namespace syncer
