// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_

#include "components/invalidation/public/invalidation_export.h"

namespace invalidation {

enum InvalidatorState {
  // Invalidations are fully working.
  INVALIDATIONS_ENABLED,

  // Failure states
  // --------------
  // There is an underlying transient problem (e.g., network- or
  // XMPP-related).
  TRANSIENT_INVALIDATION_ERROR,
  DEFAULT_INVALIDATION_ERROR = TRANSIENT_INVALIDATION_ERROR,
  // Our credentials have been rejected.
  INVALIDATION_CREDENTIALS_REJECTED,

  // Called just before shutdown so handlers can unregister themselves.
  INVALIDATOR_SHUTTING_DOWN,

  // Invalidator was stopped.
  STOPPED,
};

INVALIDATION_EXPORT const char* InvalidatorStateToString(
    InvalidatorState state);

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATOR_STATE_H_
