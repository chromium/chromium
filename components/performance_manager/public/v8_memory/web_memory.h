// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_WEB_MEMORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_WEB_MEMORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"

namespace performance_manager {

class FrameNode;

namespace v8_memory {

// Verifies that a frame is allowed to call WebMeasureMemory.
//
// The production implementation repeats the checks in the
// performance.measureUserAgentSpecificMemory spec (see the comments on
// WebMeasureMemory for the link and version). These checks are performed
// first on the renderer side but repeated in the browser to guard against
// a compromised renderer invoking the API without them.
class WebMeasureMemorySecurityChecker {
 public:
  // A callback that starts the memory measurement if the security check
  // succeeds. The FrameNode that requested the measurement is included as a
  // WeakPtr parameter because the security check might be done on another
  // sequence, and the frame might be destroyed before the callback is invoked
  // on the PM sequence.
  using MeasureMemoryCallback =
      base::OnceCallback<void(base::WeakPtr<FrameNode>)>;

  virtual ~WebMeasureMemorySecurityChecker() = default;

  // Creates a WebMeasureMemorySecurityChecker for production use.
  static std::unique_ptr<WebMeasureMemorySecurityChecker> Create();

  // Invokes |measure_memory_callback| with |frame| as a parameter on the PM
  // sequence if |frame| is allowed to call WebMeasureMemory,
  // |bad_message_callback| otherwise.
  virtual void CheckMeasureMemoryIsAllowed(
      const FrameNode* frame,
      MeasureMemoryCallback measure_memory_callback,
      mojo::ReportBadMessageCallback bad_message_callback) const = 0;
};

// Implements mojom::DocumentCoordinationUnit::OnWebMemoryMeasurementRequest to
// perform a memory measurement as defined in
// https://wicg.github.io/performance-measure-memory (this implementation
// targets the draft of 20 October 2020.)
//
// Verifies that |frame_node| is allowed to measure memory using
// |security_checker|. If so, measures memory usage of each frame in
// |frame_node|'s browsing context group and invokes |result_callback| with the
// result; if not, invokes |bad_message_callback|.
//
// This must be called on the PM sequence.
void WebMeasureMemory(
    const FrameNode* frame_node,
    mojom::WebMemoryMeasurement::Mode mode,
    std::unique_ptr<WebMeasureMemorySecurityChecker> security_checker,
    base::OnceCallback<void(mojom::WebMemoryMeasurementPtr)> result_callback,
    mojo::ReportBadMessageCallback bad_message_callback);

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_WEB_MEMORY_H_
