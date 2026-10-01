// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_ACTOR_SURFACE_ID_H_
#define COMPONENTS_ACTOR_CORE_ACTOR_SURFACE_ID_H_

#include "base/types/id_type.h"

namespace actor {

// Identifies a page the actor can act on. Stable across promotion and
// demotion.
using ActorSurfaceId = base::IdType32<class ActorSurfaceIdTag>;

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_ACTOR_SURFACE_ID_H_
