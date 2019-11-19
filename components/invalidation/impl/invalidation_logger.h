// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LOGGER_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LOGGER_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {
class ObjectIdInvalidationMap;
}  // namespace syncer

namespace invalidation {

class InvalidationLoggerObserver;

// This class is in charge of logging invalidation-related information.
// It is used store the state of the InvalidatorService that owns this object
// and then rebroadcast it to observers than can display it accordingly
// (like a debugging page). This class only stores lightweight state, as in
// which services are registered and listening for certain objects, and the
// status of the InvalidatorService (in order not to increase unnecesary
// memory usage).
//
// Observers can be registered and will be called to be notified of any
// status change immediatly. They can log there the history of what messages
// they receive.

class InvalidationLogger {

 public:
  InvalidationLogger();
  ~InvalidationLogger();

  // Pass through to any registered InvalidationLoggerObservers.
  // We will do local logging here too.
  void OnRegistration(const std::string& details);
  void OnUnregistration(const std::string& details);
  void OnStateChange(const syncer::InvalidatorState& new_state);
  void OnUpdateIds(std::map<std::string, syncer::ObjectIdSet> updated_ids);
  void OnUpdateTopics(std::map<std::string, syncer::Topics> updated_topics);
  void OnDebugMessage(const base::DictionaryValue& details);
  void OnInvalidation(const syncer::ObjectIdInvalidationMap& details);

  // Triggers messages to be sent to the Observers to provide them with
  // the current state of the logging.
  void EmitContent();

  // Add and remove observers listening for messages.
  void RegisterObserver(InvalidationLoggerObserver* debug_observer);
  void UnregisterObserver(InvalidationLoggerObserver* debug_observer);
  bool IsObserverRegistered(
      const InvalidationLoggerObserver* debug_observer) const;

 private:
  // Send to every Observer a OnStateChange event with the latest state.
  void EmitState();

  // Send to every Observer many OnUpdateIds events, each with one registrar
  // and every objectId it currently has registered.
  void EmitUpdatedIds();

  // Send to every Observer a vector with the all the current registered
  // handlers.
  void EmitRegisteredHandlers();

  // The list of every observer currently listening for notifications.
  base::ObserverList<InvalidationLoggerObserver>::Unchecked observer_list_;

  // The last InvalidatorState updated by the InvalidatorService.
  syncer::InvalidatorState last_invalidator_state_;
  base::Time last_invalidator_state_timestamp_;

  // The map that contains every object id that is currently registered
  // and its owner.
  std::map<std::string, syncer::ObjectIdSet> latest_ids_;

  // The map that counts how many invalidations per objectId there has been.
  syncer::ObjectIdCountMap invalidation_count_;

  // The name of all invalidatorHandler registered (note that this is not
  // necessarily the same as the keys of latest_ids_, because they might
  // have not registered any object id).
  std::multiset<std::string> registered_handlers_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationLogger);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LOGGER_H_
