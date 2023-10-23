// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_SERVICE_H_

#include "base/scoped_observation_traits.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class InvalidationHandler;

// Interface for classes that handle invalidation subscriptions and send out
// invalidations to registered handlers.
//
// Invalidation handlers should follow the pattern below:
//
// When starting the handler:
//
//   service->RegisterInvalidationHandler(client_handler);
//
// When the set of topics to register changes for the handler during its
// lifetime (i.e., between calls to RegisterInvalidationHandler(client_handler)
// and UnregisterInvalidationHandler(client_handler):
//
//   service->UpdateInterestedTopics(client_handler, client_topics);
//
// When shutting down the handler for browser shutdown:
//
//   service->UnregisterInvalidationHandler(client_handler);
//
// Note that there's no need to call to unregister topics. The
// invalidation API persists subscribed topics across browser restarts.
//
// If an invalidation handler cares about the invalidator state, it should also
// do the following when starting the handler:
//
//   invalidator_state = service->GetInvalidatorState();
//
// It can also do the above in OnInvalidatorStateChange(), or it can use the
// argument to OnInvalidatorStateChange().
//
// It is an error to have registered handlers when an
// InvalidationService is shut down; clients must ensure that they
// unregister themselves before then. (Depending on the
// InvalidationService, shutdown may be equivalent to destruction, or
// a separate function call like Shutdown()).
//
// NOTE(akalin): Invalidations that come in during browser shutdown may get
// dropped.  This won't matter once we have an Acknowledge API, though: see
// http://crbug.com/78462 and http://crbug.com/124149.
class InvalidationService {
 public:
  InvalidationService() = default;
  InvalidationService(const InvalidationService& other) = delete;
  InvalidationService& operator=(const InvalidationService& other) = delete;
  virtual ~InvalidationService() = default;

  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must not already be registered.
  //
  // Handler must unregister before browser shutdown.
  virtual void RegisterInvalidationHandler(InvalidationHandler* handler) = 0;

  // Updates the set of topics associated with |handler|. |handler| must not be
  // nullptr, and must already be registered. A topic must be subscribed for at
  // most one handler. If topic is already subscribed for another
  // InvalidationHandler function does nothing and returns false.
  // Note that this method  unsubscribes only from the topics which were
  // previously added (and does *not* unsubscribe from other topics which might
  // have been added before browser restart).
  //
  // TODO(b/307033849) Make this behavior less confusing.
  //
  // Subscribed topics are persisted across restarts of sync.
  [[nodiscard]] virtual bool UpdateInterestedTopics(
      InvalidationHandler* handler,
      const TopicSet& topics) = 0;

  // Stops sending invaldations to |handler|. |handler| must not be NULL, and
  // it must already be registered. Note that this doesn't unregister the topics
  // associated with |handler|.
  virtual void UnregisterInvalidationHandler(InvalidationHandler* handler) = 0;

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this must return
  // the updated state.
  virtual InvalidatorState GetInvalidatorState() const = 0;

  // Returns the ID belonging to this invalidation handler. Can be used to
  // prevent the receipt of notifications of our own changes.
  virtual std::string GetInvalidatorClientId() const = 0;
};

}  // namespace invalidation

namespace base {

template <>
struct ScopedObservationTraits<invalidation::InvalidationService,
                               invalidation::InvalidationHandler> {
  static void AddObserver(invalidation::InvalidationService* source,
                          invalidation::InvalidationHandler* observer) {
    source->RegisterInvalidationHandler(observer);
  }
  static void RemoveObserver(invalidation::InvalidationService* source,
                             invalidation::InvalidationHandler* observer) {
    source->UnregisterInvalidationHandler(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_SERVICE_H_
