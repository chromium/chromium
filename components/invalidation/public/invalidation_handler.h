// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_

#include <string>

#include "base/observer_list_types.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class Invalidation;

class INVALIDATION_EXPORT InvalidationHandler : public base::CheckedObserver {
 public:
  InvalidationHandler() = default;
  InvalidationHandler(const InvalidationHandler& other) = delete;
  InvalidationHandler& operator=(const InvalidationHandler& other) = delete;
  ~InvalidationHandler() override;

  // Called when the invalidator state changes.
  virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;

  // Called when the client has successfully subscribed to invalidations for
  // `topic`. This can be relevant for invalidation handlers because
  // invalidations published before the client has subscribed to the
  // corresponding topic will not be delivered. This can be called multiple
  // times because the subscription may need to be re-created in rare cases.
  // Child classes should override this if they're interested in this
  // notification - the default implementation does nothing.
  virtual void OnSuccessfullySubscribed(const Topic& invalidation) {}

  // Called when an invalidation is received. Note that this may be called
  // regardless of the current invalidator state.
  virtual void OnIncomingInvalidation(const Invalidation& invalidation) = 0;

  // Returned value must be unique for the handlers using the same invalidation
  // service.
  virtual std::string GetOwnerName() const = 0;

  virtual bool IsPublicTopic(const Topic& topic) const;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_
