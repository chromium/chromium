// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/nacl_subprocess.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <sstream>

#include "components/nacl/renderer/plugin/plugin_error.h"

namespace plugin {

NaClSubprocess::NaClSubprocess() {
}

// Shutdown the socket connection and service runtime, in that order.
void NaClSubprocess::Shutdown() {
  if (service_runtime_.get() != NULL) {
    service_runtime_->Shutdown();
    service_runtime_.reset(NULL);
  }
}

NaClSubprocess::~NaClSubprocess() {
  Shutdown();
}

}  // namespace plugin
