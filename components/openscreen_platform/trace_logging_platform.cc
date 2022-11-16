// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/openscreen/src/platform/api/trace_logging_platform.h"

namespace openscreen {

bool IsTraceLoggingEnabled(TraceCategory category) {
  return false;
}

}  // namespace openscreen
