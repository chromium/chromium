// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_

#include <string>

#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace syncer {

class ObjectIdInvalidationMap;

class INVALIDATION_EXPORT InvalidationHandler {
 public:
  InvalidationHandler();

  // Called when the invalidator state changes.
  virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;

  // Called when a invalidation is received.  The per-id states are in
  // |id_state_map| and the source is in |source|.  Note that this may be
  // called regardless of the current invalidator state.
  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) = 0;

  virtual std::string GetOwnerName() const = 0;

  // Called on change of |client_id|. Client id is used to identify the
  // the invalidator. The id is only relevant to some handlers, e.g. Sync
  // where the reflection blocking logic is based on it.
  virtual void OnInvalidatorClientIdChange(const std::string& client_id) {}

  virtual bool IsPublicTopic(const Topic& topic) const;

 protected:
  virtual ~InvalidationHandler();
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_HANDLER_H_
