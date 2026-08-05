// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_ACTOR_UI_MODE_H_
#define COMPONENTS_ACTOR_CORE_ACTOR_UI_MODE_H_

namespace actor {

// Coarse UI state of an actor task, used for journal annotation and
// Android UMA segmentation.
// Precedence: Pip > Controlled tab visibility.
enum class ActorUiMode {
  // A controlled tab associated with the task is visible (tab in foreground).
  kForeground,
  // No controlled tab associated with the task is visible (tab in background).
  kBackground,
  // The task is running in Picture-in-Picture mode (Android-only).
  kPip,
  kMaxValue = kPip,
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_ACTOR_UI_MODE_H_
