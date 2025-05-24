// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/dispatch_to_renderer_callback.h"

namespace input {

ScopedDispatchToRendererCallback::ScopedDispatchToRendererCallback(
    DispatchToRendererCallback dispatch_callback)
    : callback(std::move(dispatch_callback)) {}

ScopedDispatchToRendererCallback::~ScopedDispatchToRendererCallback() {
  CHECK(!callback);
}

}  // namespace input
