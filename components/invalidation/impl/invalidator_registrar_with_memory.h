// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_WITH_MEMORY_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_WITH_MEMORY_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;
class PrefService;

namespace invalidation {

class Invalidation;

BASE_DECLARE_FEATURE(kRestoreInterestingTopicsFeature);

// A helper class for FCMInvalidationService.  It helps keep track of registered
// handlers and which topic registrations are associated with each handler.
class INVALIDATION_EXPORT InvalidatorRegistrarWithMemory {
 public:
  InvalidatorRegistrarWithMemory(PrefService* prefs,
                                 const std::string& sender_id);
  InvalidatorRegistrarWithMemory(const InvalidatorRegistrarWithMemory& other) =
      delete;
  InvalidatorRegistrarWithMemory& operator=(
      const InvalidatorRegistrarWithMemory& other) = delete;
  // It is an error to have registered handlers on destruction.
  ~InvalidatorRegistrarWithMemory();

  // RegisterProfilePrefs and RegisterPrefs register the same prefs, because on
  // device level (sign in screen, device local account) we spin up separate
  // InvalidationService and on profile level (when user signed in) we have
  // another InvalidationService, and we want to keep profile data in an
  // encrypted area of disk. While device data which is public can be kept in an
  // unencrypted area.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void RegisterPrefs(PrefRegistrySimple* registry);

  static void ClearDeprecatedPrefs(PrefService* prefs);

  // Starts sending notifications to |handler|.  |handler| must not be nullptr,
  // and it must not already be registered.
  void AddObserver(InvalidationHandler* handler);

  bool HasObserver(const InvalidationHandler* handler) const;

  // Stops sending notifications to |handler|.  |handler| must not be nullptr,
  // and it must already be registered.  Note that this doesn't unregister the
  // topics associated with |handler| from the server.
  void RemoveObserver(const InvalidationHandler* handler);

  // Updates the set of topics associated with |handler|. |handler| must not be
  // nullptr, and must already be registered. A topic must be registered for at
  // most one handler. If any of the |topics| is already registered to a
  // different handler, returns false. Note that this also updates the
  // *subscribed* topics - assuming that whoever called this will also send
  // (un)subscription requests to the server. However, this method does *not*
  // unsubscribe from the topics which were not registered since browser
  // startup.
  [[nodiscard]] bool UpdateRegisteredTopics(InvalidationHandler* handler,
                                            const TopicMap& topics);

  // Returns all topics currently registered to |handler|.
  TopicMap GetRegisteredTopics(InvalidationHandler* handler) const;

  // Returns the set of all topics that (we think) we are subscribed to on the
  // server. This is the set of topics which were registered to some handler and
  // not unregistered (via UpdateRegisteredTopics()). This includes topics whose
  // *handler* has been unregistered without unregistering the topic itself
  // first (e.g. because Chrome was restarted and the handler hasn't registered
  // itself again yet).
  TopicMap GetAllSubscribedTopics() const;

  // Dispatches incoming invalidation to the corresponding handler based on its
  // topic.
  // Invalidations for topics with no corresponding handler are returned.
  std::optional<Invalidation> DispatchInvalidationToHandlers(
      const Invalidation& invalidation);

  // Dispatches a notification that the client has successfully subscribed to
  // `topic` to handlers.
  void DispatchSuccessfullySubscribedToHandlers(const Topic& topic);

  // Updates the invalidator state to the given one and then notifies
  // all handlers.  Note that the order is important; handlers that
  // call GetInvalidatorState() when notified will see the new state.
  void UpdateInvalidatorState(InvalidatorState state);

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this returns the
  // updated state.
  InvalidatorState GetInvalidatorState() const;

 private:
  // Checks if any of the |new_topics| is already registered for a *different*
  // handler than the given one.
  bool HasDuplicateTopicRegistration(InvalidationHandler* handler,
                                     const TopicMap& new_topics) const;

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<InvalidationHandler, true> handlers_;
  // Note: When a handler is unregistered, its entry is removed from
  // |registered_handler_to_topics_map_| but NOT from
  // |handler_name_to_subscribed_topics_map_|.
  std::map<InvalidationHandler*, TopicMap, std::less<>>
      registered_handler_to_topics_map_;
  std::map<std::string, TopicMap> handler_name_to_subscribed_topics_map_;

  InvalidatorState state_;

  // This can be either a regular (Profile-attached) PrefService or the local
  // state PrefService.
  const raw_ptr<PrefService> prefs_;

  // The FCM sender ID.
  const std::string sender_id_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_WITH_MEMORY_H_
