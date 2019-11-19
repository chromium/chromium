// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Interface to the invalidator, which is an object that receives
// invalidations for registered object IDs. The corresponding
// InvalidationHandler is notifier when such an event occurs.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_H_

#include <string>

#include "base/callback.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "google_apis/gaia/core_account_id.h"

namespace syncer {
class InvalidationHandler;

class INVALIDATION_EXPORT Invalidator {
 public:
  Invalidator();
  virtual ~Invalidator();

  // Clients should follow the pattern below:
  //
  // When starting the client:
  //
  //   invalidator->RegisterHandler(client_handler);
  //
  // When the set of IDs to register changes for the client during its lifetime
  // (i.e., between calls to RegisterHandler(client_handler) and
  // UnregisterHandler(client_handler):
  //
  //   invalidator->UpdateRegisteredIds(client_handler, client_ids);
  //
  // When shutting down the client for profile shutdown:
  //
  //   invalidator->UnregisterHandler(client_handler);
  //
  // Note that there's no call to UpdateRegisteredIds() -- this is because the
  // invalidation API persists registrations across browser restarts.
  //
  // When permanently shutting down the client, e.g. when disabling the related
  // feature:
  //
  //   invalidator->UpdateRegisteredIds(client_handler, ObjectIdSet());
  //   invalidator->UnregisterHandler(client_handler);
  //
  // It is an error to have registered handlers when an invalidator is
  // destroyed; clients must ensure that they unregister themselves
  // before then.

  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must not already be registered.
  virtual void RegisterHandler(InvalidationHandler* handler) = 0;

  // Updates the set of ObjectIds associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  An ID must be registered for
  // at most one handler. If ID is already registered function returns false.
  virtual bool UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids)
      WARN_UNUSED_RESULT = 0;

  // Updates the set of ObjectIds associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  An ID must be registered for
  // at most one handler. If ID is already registered function returns false.
  virtual bool UpdateRegisteredIds(InvalidationHandler* handler,
                                   const Topics& ids) WARN_UNUSED_RESULT;

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  virtual void UnregisterHandler(InvalidationHandler* handler) = 0;

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this must return
  // the updated state.
  virtual InvalidatorState GetInvalidatorState() const = 0;

  // The observers won't be notified of any notifications until
  // UpdateCredentials is called at least once. It can be called more than
  // once.
  virtual void UpdateCredentials(const CoreAccountId& account_id,
                                 const std::string& token) = 0;

  // Requests internal detailed status to be posted back to the callback.
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) const = 0;
};
}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_H_
