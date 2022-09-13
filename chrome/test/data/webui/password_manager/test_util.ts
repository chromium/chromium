// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function makePasswordCheckStatus(
    state?: chrome.passwordsPrivate.PasswordCheckState, checked?: number,
    remaining?: number,
    lastCheck?: string): chrome.passwordsPrivate.PasswordCheckStatus {
  return {
    state: state || chrome.passwordsPrivate.PasswordCheckState.IDLE,
    alreadyProcessed: checked,
    remainingInQueue: remaining,
    elapsedTimeSinceLastCheck: lastCheck,
  };
}
