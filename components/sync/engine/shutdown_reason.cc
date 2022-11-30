// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/shutdown_reason.h"

#include "base/notreached.h"

namespace syncer {

const char* ShutdownReasonToString(ShutdownReason reason) {
  switch (reason) {
    case ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
      return "STOP_SYNC_AND_KEEP_DATA";
    case ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
      return "DISABLE_SYNC_AND_CLEAR_DATA";
    case ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
      return "BROWSER_SHUTDOWN_AND_KEEP_DATA";
  }

  NOTREACHED();
  return "";
}

}  // namespace syncer
