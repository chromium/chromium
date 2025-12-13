// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface to retrieve the different game states shared across the dino game.
 */
export interface GameStateProvider {
  get hasSlowdown(): boolean;
  get hasAudioCues(): boolean;
  isAltGameModeEnabled(): boolean;
}
