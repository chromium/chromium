// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SHUTDOWN_REASON_H_
#define COMPONENTS_SYNC_ENGINE_SHUTDOWN_REASON_H_

namespace syncer {

// Reason for shutting down sync engine.
enum ShutdownReason {
  STOP_SYNC,         // Sync is asked to stop, e.g. due to scarce resource.
  DISABLE_SYNC,      // Sync is disabled, e.g. user sign out, dashboard clear.
  BROWSER_SHUTDOWN,  // Browser is closed.
};

const char* ShutdownReasonToString(ShutdownReason reason);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SHUTDOWN_REASON_H_
