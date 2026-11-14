// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_RUST_SHIM_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_RUST_SHIM_H_

#include "third_party/crashpad/crashpad/client/annotation.h"

namespace crash_reporter::rust {

// Adds `annotation` to Crashpad's list of annotations, so that it is included
// in crash reports. Does nothing if it was already added. `annotation` must
// never be destroyed.
void RegisterAnnotation(crashpad::Annotation* annotation);

}  // namespace crash_reporter::rust

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEY_RUST_SHIM_H_
