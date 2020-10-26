// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_

#include <memory>

#include "base/callback.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class ProcessNode;

namespace v8_memory {

class V8DetailedMemoryRequestOneShot;
class V8DetailedMemoryProcessData;
// A helper class for implementing WebMeasureMemory().
class WebMemoryAggregator {
 public:
  using MeasurementCallback =
      base::OnceCallback<void(mojom::WebMemoryMeasurementPtr)>;

  WebMemoryAggregator(const blink::LocalFrameToken&, MeasurementCallback);

  ~WebMemoryAggregator();

  // A callback for V8DetailedMemoryRequestOneShot.
  void MeasurementComplete(const ProcessNode*,
                           const V8DetailedMemoryProcessData*);

  // Transfer ownership of the given request to the given WebMemoryAggregator
  // and makes the latter self-owned.
  static void MakeSelfOwned(std::unique_ptr<WebMemoryAggregator>,
                            std::unique_ptr<V8DetailedMemoryRequestOneShot>);

 private:
  blink::LocalFrameToken frame_token_;
  MeasurementCallback callback_;
  std::unique_ptr<V8DetailedMemoryRequestOneShot> request_;
  // WebMemory is self-owned and lives until the measurement request
  // is completed or failed.
  std::unique_ptr<WebMemoryAggregator> self_reference_;
};

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_
