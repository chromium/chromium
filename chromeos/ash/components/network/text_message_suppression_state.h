// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_SUPPRESSION_STATE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_SUPPRESSION_STATE_H_

namespace ash {

// The suppression state set via the policy.
enum class PolicyTextMessageSuppressionState {
  kUnset = 0,
  kAllow = 1,
  kSuppress = 2,
};

// The suppression state set by the user.
enum class UserTextMessageSuppressionState {
  // Text message notifications will be allowed.
  kAllow = 1,
  // Text message notifications will be suppressed.
  kSuppress = 2,
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TEXT_MESSAGE_SUPPRESSION_STATE_H_
