// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_DISPATCH_TO_RENDERER_CALLBACK_H_
#define COMPONENTS_INPUT_DISPATCH_TO_RENDERER_CALLBACK_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace input {

enum class DispatchToRendererResult { kDispatched, kNotDispatched };

using DispatchToRendererCallback =
    base::OnceCallback<void(const blink::WebInputEvent&,
                            DispatchToRendererResult)>;

// Class to facilitate checking if the callback for dispatching event has been
// run or moved.
struct COMPONENT_EXPORT(INPUT) ScopedDispatchToRendererCallback {
  explicit ScopedDispatchToRendererCallback(
      DispatchToRendererCallback callback);
  ~ScopedDispatchToRendererCallback();

  DispatchToRendererCallback callback;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_DISPATCH_TO_RENDERER_CALLBACK_H_
