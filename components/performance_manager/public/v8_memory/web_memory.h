// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_WEB_MEMORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_WEB_MEMORY_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/util/type_safety/pass_key.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class FrameNode;

namespace v8_memory {

// Implements mojom::DocumentCoordinationUnit::OnWebMemoryMeasurementRequest.
// Measures memory usage of each frame in the browsing context group of the
// given frame and invokes the given callback with the result.
void WebMeasureMemory(const FrameNode*,
                      mojom::WebMemoryMeasurement::Mode,
                      base::OnceCallback<void(mojom::WebMemoryMeasurementPtr)>);

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_WEB_MEMORY_H_
