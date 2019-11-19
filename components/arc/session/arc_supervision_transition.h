// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_SUPERVISION_TRANSITION_H_
#define COMPONENTS_ARC_SESSION_ARC_SUPERVISION_TRANSITION_H_

#include <ostream>

namespace arc {

// These values must be kept in sync with
// UpgradeArcInstanceRequest.SupervisionTransition in
// third_party/cros_system_api/dbus/arc.proto.
enum class ArcSupervisionTransition : int {
  // No transition necessary.
  NO_TRANSITION = 0,
  // Child user is transitioning to a regular account, need to lift
  // supervision.
  CHILD_TO_REGULAR = 1,
  // Regular user is transitioning to a child account, need to enable
  // supervision.
  REGULAR_TO_CHILD = 2,
};

std::ostream& operator<<(std::ostream& os,
                         ArcSupervisionTransition supervisionTransition);

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_SUPERVISION_TRANSITION_H_
