// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/channels_states.h"

namespace invalidation {

const char* FcmChannelStateToString(FcmChannelState state) {
  switch (state) {
    case FcmChannelState::NOT_STARTED:
      return "NOT_STARTED";
    case FcmChannelState::ENABLED:
      return "ENABLED";
    case FcmChannelState::NO_INSTANCE_ID_TOKEN:
      return "NO_INSTANCE_ID_TOKEN";
  }
}

const char* SubscriptionChannelStateToString(SubscriptionChannelState state) {
  switch (state) {
    case SubscriptionChannelState::NOT_STARTED:
      return "NOT_STARTED";
    case SubscriptionChannelState::ENABLED:
      return "ENABLED";
    case SubscriptionChannelState::ACCESS_TOKEN_FAILURE:
      return "ACCESS_TOKEN_FAILURE";
    case SubscriptionChannelState::SUBSCRIPTION_FAILURE:
      return "SUBSCRIPTION_FAILURE";
  }
}

}  // namespace invalidation
