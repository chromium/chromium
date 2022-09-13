// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LOGGER_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LOGGER_H_

#include <map>
#include <set>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class InvalidationLoggerObserver;
class TopicInvalidationMap;

// This class is in charge of logging invalidation-related information.
// It is used store the state of the InvalidationService that owns this object
// and then rebroadcast it to observers than can display it accordingly
// (like a debugging page). This class only stores lightweight state, as in
// which services are registered and listening for certain topics, and the
// status of the InvalidationService (in order not to increase unnecessary
// memory usage).
//
// Observers can be registered and will be called to be notified of any
// status change immediatly. They can log there the history of what messages
// they receive.
class InvalidationLogger {

 public:
  InvalidationLogger();
  InvalidationLogger(const InvalidationLogger& other) = delete;
  InvalidationLogger& operator=(const InvalidationLogger& other) = delete;
  ~InvalidationLogger();

  // Pass through to any registered InvalidationLoggerObservers.
  // We will do local logging here too.
  void OnRegistration(const std::string& details);
  void OnUnregistration(const std::string& details);
  void OnStateChange(const InvalidatorState& new_state);
  void OnUpdatedTopics(
      std::map<std::string, Topics> handler_updated_topics_map);
  void OnDebugMessage(const base::Value::Dict& details);
  void OnInvalidation(const TopicInvalidationMap& invalidations);

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

  // Send to every Observer many OnUpdatedTopics() events, each with one
  // handler and every Topic it currently has registered.
  void EmitUpdatedTopics();

  // Send to every Observer a vector with the all the current registered
  // handlers.
  void EmitRegisteredHandlers();

  // The list of every observer currently listening for notifications.
  base::ObserverList<InvalidationLoggerObserver>::Unchecked observer_list_;

  // The last InvalidatorState updated by the InvalidatorService.
  InvalidatorState last_invalidator_state_ = TRANSIENT_INVALIDATION_ERROR;
  base::Time last_invalidator_state_timestamp_;

  // The map that contains every topic that is currently registered and its
  // owner.
  std::map<std::string, Topics> handler_latest_topics_map_;

  // The map that counts how many invalidations per Topic there has been.
  TopicCountMap invalidation_count_;

  // The name of all invalidatorHandler registered (note that this is not
  // necessarily the same as the keys of latest_topics_, because they might
  // have not registered any Topic).
  std::set<std::string> registered_handlers_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LOGGER_H_
