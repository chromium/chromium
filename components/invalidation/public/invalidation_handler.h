// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_

#include <string>

#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class TopicInvalidationMap;

class INVALIDATION_EXPORT InvalidationHandler {
 public:
  InvalidationHandler() = default;
  InvalidationHandler(const InvalidationHandler& other) = delete;
  InvalidationHandler& operator=(const InvalidationHandler& other) = delete;
  virtual ~InvalidationHandler() = default;

  // Called when the invalidator state changes.
  virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;

  // Called when a invalidation is received. Note that this may be called
  // regardless of the current invalidator state.
  virtual void OnIncomingInvalidation(
      const TopicInvalidationMap& invalidation_map) = 0;

  // Returned value must be unique for the handlers using the same invalidation
  // service.
  virtual std::string GetOwnerName() const = 0;

  // Called on change of |client_id|. Client id is used to identify the
  // the invalidator. The id is only relevant to some handlers, e.g. Sync
  // where the reflection blocking logic is based on it.
  virtual void OnInvalidatorClientIdChange(const std::string& client_id);

  virtual bool IsPublicTopic(const Topic& topic) const;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_
