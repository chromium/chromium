// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/shutdown_reason.h"

#include "base/logging.h"

namespace syncer {

const char* ShutdownReasonToString(ShutdownReason reason) {
  switch (reason) {
    case STOP_SYNC:
      return "STOP_SYNC";
    case DISABLE_SYNC:
      return "DISABLE_SYNC";
    case BROWSER_SHUTDOWN:
      return "BROWSER_SHUTDOWN";
  }

  NOTREACHED();
  return "";
}

}  // namespace syncer
