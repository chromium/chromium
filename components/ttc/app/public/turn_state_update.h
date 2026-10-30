// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_TURN_STATE_UPDATE_H_
#define COMPONENTS_TTC_APP_PUBLIC_TURN_STATE_UPDATE_H_

namespace ttc {

// A change in the state of the model's current turn, as reported by the
// backend.
enum class ModelTurnStateUpdate {
  // The model has started generating a response. Marks the start of a model
  // turn.
  kTurnStarted,

  // The model has finished generating a response. Output audio may still be
  // playing back. Not sent when the turn is interrupted.
  kGenerationCompleted,

  // Model generation was interrupted (e.g. user barge-in).
  kInterrupted,

  // The model has completed its turn, either naturally (after output audio
  // finished playing) or immediately following an interruption.
  kTurnCompleted,
};

// A change in the state of the user's current turn, as reported by the
// backend. A user turn can start while a model turn is still in progress, in
// which case the model's `kInterrupted` and `kTurnCompleted` updates follow
// the user's `kTurnStarted`.
enum class UserTurnStateUpdate {
  // The user has started their turn (e.g. started speaking).
  kTurnStarted,

  // The user has completed their turn.
  kTurnCompleted,
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_TURN_STATE_UPDATE_H_
