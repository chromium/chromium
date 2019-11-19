// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_supervision_transition.h"
#include "base/logging.h"

namespace arc {

std::ostream& operator<<(std::ostream& os,
                         ArcSupervisionTransition supervision_transition) {
  switch (supervision_transition) {
    case ArcSupervisionTransition::NO_TRANSITION:
      return os << "NO_TRANSITION";
    case ArcSupervisionTransition::CHILD_TO_REGULAR:
      return os << "CHILD_TO_REGULAR";
    case ArcSupervisionTransition::REGULAR_TO_CHILD:
      return os << "REGULAR_TO_CHILD";
  }
  NOTREACHED() << "Unexpected value for ArcSupervisionTransition: "
               << static_cast<int>(supervision_transition);

  return os << "ArcSupervisionTransition("
            << static_cast<int>(supervision_transition) << ")";
}

}  // namespace arc
