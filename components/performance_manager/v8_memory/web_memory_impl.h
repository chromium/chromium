// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class ProcessNode;

namespace v8_memory {

// A helper class for implementing WebMeasureMemory(). This manages a request
// object that sends a V8 detailed memory request to the renderer, and formats
// the result into a mojom::WebMemoryMeasurement.
// TODO(crbug.com/1085129): Extend this to measure all renderers that are
// reachable from the requesting node.
class WebMemoryMeasurer {
 public:
  using MeasurementCallback =
      base::OnceCallback<void(mojom::WebMemoryMeasurementPtr)>;

  WebMemoryMeasurer(const blink::LocalFrameToken&,
                    V8DetailedMemoryRequest::MeasurementMode,
                    MeasurementCallback);
  ~WebMemoryMeasurer();

  WebMemoryMeasurer(const WebMemoryMeasurer& other) = delete;
  WebMemoryMeasurer& operator=(const WebMemoryMeasurer& other) = delete;

  V8DetailedMemoryRequestOneShot* request() const { return request_.get(); }

  // A callback for V8DetailedMemoryRequestOneShot.
  void MeasurementComplete(const ProcessNode*,
                           const V8DetailedMemoryProcessData*);

 private:
  blink::LocalFrameToken frame_token_;
  MeasurementCallback callback_;
  std::unique_ptr<V8DetailedMemoryRequestOneShot> request_;
};

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_IMPL_H_
