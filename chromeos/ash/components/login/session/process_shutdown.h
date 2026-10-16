// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_PROCESS_SHUTDOWN_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_PROCESS_SHUTDOWN_H_

namespace ash {

// Returns whether the browser process is shutting down, i.e. its main run loop
// has been asked to quit after the last `ScopedKeepAlive` was released (see
// `BrowserProcessImpl::Unpin()`). The run loop may still be running pending
// tasks at that point. Once true, this stays true until the process exits, and
// registering a new `ScopedKeepAlive` crashes.
//
// In //chromeos and //ash, prefer this over depending on `KeepAliveRegistry`
// directly.
//
// Must be called on the UI thread.
bool IsProcessShuttingDown();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_PROCESS_SHUTDOWN_H_
