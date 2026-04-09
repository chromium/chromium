// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_ACTOR_UTIL_H_
#define COMPONENTS_ACTOR_CORE_ACTOR_UTIL_H_

namespace actor {

// Whether actor safety checks are disabled.
bool IsActorSafetyCheckDisabled();

// Whether actor navigation gating is enabled.
bool IsNavigationGatingEnabled();

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_ACTOR_UTIL_H_
