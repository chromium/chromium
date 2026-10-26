// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_DEBUG_SINK_H_
#define COMPONENTS_TTC_APP_PUBLIC_DEBUG_SINK_H_

namespace ttc {

// Typed instrumentation of the TTC audio pipeline, consumed by the developer
// debug tools. All methods are called on the main sequence and must be cheap:
// some are called for every audio chunk. Only provided while the debug tools
// are enabled; callers hold a nullable pointer and skip reporting otherwise.
class DebugSink {
 public:
  virtual ~DebugSink() = default;
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_DEBUG_SINK_H_
