// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

const char* InvalidatorStateToString(InvalidatorState state) {
  switch (state) {
    case TRANSIENT_INVALIDATION_ERROR:
      return "TRANSIENT_INVALIDATION_ERROR";
    case INVALIDATION_CREDENTIALS_REJECTED:
      return "INVALIDATION_CREDENTIALS_REJECTED";
    case INVALIDATIONS_ENABLED:
      return "INVALIDATIONS_ENABLED";
    case INVALIDATOR_SHUTTING_DOWN:
      return "INVALIDATOR_SHUTTING_DOWN";
    case STOPPED:
      return "STOPPED";
  }
}

}  // namespace invalidation
