// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class FrameNode;
class ProcessNode;

namespace v8_memory {

// A helper class for implementing WebMeasureMemory(). This manages a request
// object that sends a V8 detailed memory request to the renderer, and formats
// the result into a mojom::WebMemoryMeasurement.
class WebMemoryMeasurer {
 public:
  using MeasurementCallback =
      base::OnceCallback<void(mojom::WebMemoryMeasurementPtr)>;

  // Implements WebMeasureMemory (from public/v8_memory/web_memory.h) by
  // instantiating a WebMemoryMeasurer. |frame_node| must be the last parameter
  // so it can be used with base::Bind.
  static void MeasureMemory(mojom::WebMemoryMeasurement::Mode mode,
                            MeasurementCallback callback,
                            base::WeakPtr<FrameNode> frame_node);

  ~WebMemoryMeasurer();

  WebMemoryMeasurer(const WebMemoryMeasurer& other) = delete;
  WebMemoryMeasurer& operator=(const WebMemoryMeasurer& other) = delete;

  V8DetailedMemoryRequestOneShot* request() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return request_.get();
  }

  // A callback for V8DetailedMemoryRequestOneShot.
  void MeasurementComplete(const ProcessNode*,
                           const V8DetailedMemoryProcessData*);

 private:
  friend class WebMemoryImplTest;

  WebMemoryMeasurer(const blink::LocalFrameToken&,
                    V8DetailedMemoryRequest::MeasurementMode,
                    MeasurementCallback);

  blink::LocalFrameToken frame_token_;
  MeasurementCallback callback_;
  std::unique_ptr<V8DetailedMemoryRequestOneShot> request_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// The default implementation of WebMeasureMemorySecurityChecker.
class WebMeasureMemorySecurityCheckerImpl
    : public WebMeasureMemorySecurityChecker {
 public:
  WebMeasureMemorySecurityCheckerImpl() = default;
  ~WebMeasureMemorySecurityCheckerImpl() override = default;

  void CheckMeasureMemoryIsAllowed(
      const FrameNode* frame,
      MeasureMemoryCallback measure_memory_callback,
      mojo::ReportBadMessageCallback bad_message_callback) const override;
};

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_IMPL_H_
