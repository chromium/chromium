// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/deprecated_invalidator_registrar.h"

#include <cstddef>
#include <iterator>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/invalidation/public/object_id_invalidation_map.h"

namespace syncer {

DeprecatedInvalidatorRegistrar::DeprecatedInvalidatorRegistrar()
    : state_(DEFAULT_INVALIDATION_ERROR) {}

DeprecatedInvalidatorRegistrar::~DeprecatedInvalidatorRegistrar() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Substitute CHECK(handler_to_ids_map_.empty()) with histogram,
  // in order to investigate bug https://crbug.com/880226
  for (const auto& handler_to_id : handler_to_ids_map_) {
    UMA_HISTOGRAM_ENUMERATION(
        "DeprecatedInvalidatorRegistrar.CrashStatus",
        OwnerNameToHandlerType(handler_to_id.first->GetOwnerName()));
  }
}

void DeprecatedInvalidatorRegistrar::RegisterHandler(
    InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(!handlers_.HasObserver(handler));
  handlers_.AddObserver(handler);
}

bool DeprecatedInvalidatorRegistrar::UpdateRegisteredIds(
    InvalidationHandler* handler,
    const ObjectIdSet& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));

  for (HandlerIdsMap::const_iterator it = handler_to_ids_map_.begin();
       it != handler_to_ids_map_.end(); ++it) {
    if (it->first == handler) {
      continue;
    }

    std::vector<invalidation::ObjectId> intersection;
    std::set_intersection(
        it->second.begin(), it->second.end(), ids.begin(), ids.end(),
        std::inserter(intersection, intersection.end()), ObjectIdLessThan());
    if (!intersection.empty()) {
      LOG(ERROR) << "Duplicate registration: trying to register "
                 << ObjectIdToString(*intersection.begin()) << " for "
                 << handler << " when it's already registered for "
                 << it->first;
      return false;
    }
  }

  if (ids.empty()) {
    handler_to_ids_map_.erase(handler);
  } else {
    handler_to_ids_map_[handler] = ids;
  }
  return true;
}

void DeprecatedInvalidatorRegistrar::UnregisterHandler(
    InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));
  handlers_.RemoveObserver(handler);
  handler_to_ids_map_.erase(handler);
}

ObjectIdSet DeprecatedInvalidatorRegistrar::GetRegisteredIds(
    InvalidationHandler* handler) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto lookup = handler_to_ids_map_.find(handler);
  return lookup != handler_to_ids_map_.end() ? lookup->second : ObjectIdSet();
}

ObjectIdSet DeprecatedInvalidatorRegistrar::GetAllRegisteredIds() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  ObjectIdSet registered_ids;
  for (auto it = handler_to_ids_map_.begin(); it != handler_to_ids_map_.end();
       ++it) {
    registered_ids.insert(it->second.begin(), it->second.end());
  }
  return registered_ids;
}

void DeprecatedInvalidatorRegistrar::DispatchInvalidationsToHandlers(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If we have no handlers, there's nothing to do.
  if (!handlers_.might_have_observers()) {
    return;
  }

  for (auto it = handler_to_ids_map_.begin(); it != handler_to_ids_map_.end();
       ++it) {
    ObjectIdInvalidationMap to_emit =
        invalidation_map.GetSubsetWithObjectIds(it->second);
    if (!to_emit.Empty()) {
      it->first->OnIncomingInvalidation(to_emit);
    }
  }
}

void DeprecatedInvalidatorRegistrar::UpdateInvalidatorState(
    InvalidatorState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "New invalidator state: " << InvalidatorStateToString(state_)
           << " -> " << InvalidatorStateToString(state);
  state_ = state;
  for (auto& observer : handlers_)
    observer.OnInvalidatorStateChange(state);
}

InvalidatorState DeprecatedInvalidatorRegistrar::GetInvalidatorState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_;
}

std::map<std::string, ObjectIdSet>
DeprecatedInvalidatorRegistrar::GetSanitizedHandlersIdsMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::map<std::string, ObjectIdSet> clean_handlers_to_ids;
  for (HandlerIdsMap::const_iterator it = handler_to_ids_map_.begin();
       it != handler_to_ids_map_.end(); ++it) {
    clean_handlers_to_ids[it->first->GetOwnerName()] = ObjectIdSet(it->second);
  }
  return clean_handlers_to_ids;
}

bool DeprecatedInvalidatorRegistrar::HasRegisteredHandlers() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (GetAllRegisteredIds().size() == 0);
}

bool DeprecatedInvalidatorRegistrar::IsHandlerRegisteredForTest(
    const InvalidationHandler* handler) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return handlers_.HasObserver(handler);
}

void DeprecatedInvalidatorRegistrar::DetachFromThreadForTest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  thread_checker_.DetachFromThread();
}

}  // namespace syncer
