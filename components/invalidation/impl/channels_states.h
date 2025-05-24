// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_CHANNELS_STATES_H_
#define COMPONENTS_INVALIDATION_IMPL_CHANNELS_STATES_H_

namespace invalidation {

enum class FcmChannelState {
  NOT_STARTED,
  // Fcm network channel is working properly.
  ENABLED,
  // Failed to retrieve instance id token.
  NO_INSTANCE_ID_TOKEN,

  kMaxValue = NO_INSTANCE_ID_TOKEN,
};

enum class SubscriptionChannelState {
  NOT_STARTED,
  ENABLED,
  ACCESS_TOKEN_FAILURE,
  SUBSCRIPTION_FAILURE,

  kMaxValue = SUBSCRIPTION_FAILURE,
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_CHANNELS_STATES_H_
