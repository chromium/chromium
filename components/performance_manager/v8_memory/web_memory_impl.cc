// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace v8_memory {

namespace {

mojom::WebMemoryMeasurementPtr BuildMemoryUsageResult(
    const blink::LocalFrameToken& frame_token,
    const ProcessNode* process_node) {
  const auto& frame_nodes = process_node->GetFrameNodes();

  // Find the frame that made the request.
  const FrameNode* requesting_frame = nullptr;
  for (auto* frame_node : frame_nodes) {
    if (frame_node->GetFrameToken() == frame_token) {
      requesting_frame = frame_node;
      break;
    }
  }

  if (!requesting_frame) {
    // The frame no longer exists.
    return mojom::WebMemoryMeasurement::New();
  }

  auto result = mojom::WebMemoryMeasurement::New();

  for (const FrameNode* frame_node : frame_nodes) {
    if (frame_node->GetBrowsingInstanceId() !=
        requesting_frame->GetBrowsingInstanceId()) {
      continue;
    }
    if (frame_node->GetURL().GetOrigin() !=
        requesting_frame->GetURL().GetOrigin()) {
      continue;
    }
    auto* data = v8_memory::V8DetailedMemoryExecutionContextData::ForFrameNode(
        frame_node);
    if (!data) {
      continue;
    }
    auto attribution = mojom::WebMemoryAttribution::New();
    attribution->url = frame_node->GetURL().spec();
    attribution->scope = mojom::WebMemoryAttribution::Scope::kWindow;
    auto entry = mojom::WebMemoryBreakdownEntry::New();
    entry->bytes = data->v8_bytes_used();
    entry->attribution.push_back(std::move(attribution));
    result->breakdown.push_back(std::move(entry));
  }
  return result;
}

v8_memory::V8DetailedMemoryRequest::MeasurementMode
WebMeasurementModeToRequestMeasurementMode(
    mojom::WebMemoryMeasurement::Mode mode) {
  switch (mode) {
    case mojom::WebMemoryMeasurement::Mode::kDefault:
      return v8_memory::V8DetailedMemoryRequest::MeasurementMode::kDefault;
    case mojom::WebMemoryMeasurement::Mode::kEager:
      return v8_memory::V8DetailedMemoryRequest::MeasurementMode::
          kEagerForTesting;
  }
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// WebMemoryMeasurer

WebMemoryMeasurer::WebMemoryMeasurer(
    const blink::LocalFrameToken& frame_token,
    V8DetailedMemoryRequest::MeasurementMode mode,
    MeasurementCallback callback)
    : frame_token_(frame_token),
      callback_(std::move(callback)),
      request_(std::make_unique<V8DetailedMemoryRequestOneShot>(mode)) {}

WebMemoryMeasurer::~WebMemoryMeasurer() = default;

// static
void WebMemoryMeasurer::MeasureMemory(const FrameNode* frame_node,
                                      mojom::WebMemoryMeasurement::Mode mode,
                                      MeasurementCallback callback) {
  // Can't use make_unique with a private constructor.
  auto measurer = base::WrapUnique(new WebMemoryMeasurer(
      frame_node->GetFrameToken(),
      WebMeasurementModeToRequestMeasurementMode(mode), std::move(callback)));

  // Create a measurement complete callback to own |measurer|. It
  // will be deleted when the callback is executed or dropped.
  V8DetailedMemoryRequestOneShot* request = measurer->request();
  auto measurement_complete_callback = base::BindOnce(
      &WebMemoryMeasurer::MeasurementComplete, std::move(measurer));

  // Start memory measurement for the process of the given frame.
  request->StartMeasurement(frame_node->GetProcessNode(),
                            std::move(measurement_complete_callback));
}

void WebMemoryMeasurer::MeasurementComplete(
    const ProcessNode* process_node,
    const V8DetailedMemoryProcessData*) {
  // TODO(crbug.com/1085129): Use WebMemoryAggregator here instead of
  // BuildMemoryUsageResult.
  std::move(callback_).Run(BuildMemoryUsageResult(frame_token_, process_node));
}

////////////////////////////////////////////////////////////////////////////////
// WebMeasureMemorySecurityCheckerImpl

// Implements the public function in public/v8_memory/web_memory.h
std::unique_ptr<WebMeasureMemorySecurityChecker>
WebMeasureMemorySecurityChecker::Create() {
  return std::make_unique<WebMeasureMemorySecurityCheckerImpl>();
}

void WebMeasureMemorySecurityCheckerImpl::CheckMeasureMemoryIsAllowed(
    const FrameNode* frame,
    base::OnceClosure measure_memory_closure,
    mojo::ReportBadMessageCallback bad_message_callback) const {
  DCHECK(frame);
  DCHECK_ON_GRAPH_SEQUENCE(frame->GetGraph());

  // TODO(crbug/1085129): The frame may have navigated since it sent the
  // measureMemory request. We could return true if the new document is allowed
  // to measure memory, but the actual document that sent the request is not.
  // If that happens the DocumentCoordinationUnit mojo interface is reset so
  // the measurement result will be thrown away, so this is not a security
  // issue, but it does mean doing extra work.

  if (!base::FeatureList::IsEnabled(
          blink::features::kWebMeasureMemoryViaPerformanceManager)) {
    std::move(bad_message_callback)
        .Run("WebMeasureMemoryViaPerformanceManager feature is disabled");
    return;
  }
  // "Memory measurement allowed" predicate from
  // https://wicg.github.io/performance-measure-memory/ section 3.2.
  if (url::Origin::Create(frame->GetURL()) !=
      url::Origin::Create(frame->GetPageNode()->GetMainFrameNode()->GetURL())) {
    std::move(bad_message_callback)
        .Run("performance.measureMemory called from cross-origin subframe");
    return;
  }
  // TODO(crbug/1085129): Check crossOriginIsolated once this is available in
  // the browser. This will need to be done on the UI sequence, and return the
  // result to the PM sequence to run the closure.
  std::move(measure_memory_closure).Run();
}

////////////////////////////////////////////////////////////////////////////////
// Free functions

// Implements the public function in public/v8_memory/web_memory.h
void WebMeasureMemory(
    const FrameNode* frame_node,
    mojom::WebMemoryMeasurement::Mode mode,
    std::unique_ptr<WebMeasureMemorySecurityChecker> security_checker,
    base::OnceCallback<void(mojom::WebMemoryMeasurementPtr)> result_callback,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK(frame_node);
  DCHECK(security_checker);

  // Validate that |frame_node| is allowed to measure memory, then start the
  // measurement.
  security_checker->CheckMeasureMemoryIsAllowed(
      frame_node,
      base::BindOnce(&WebMemoryMeasurer::MeasureMemory, frame_node, mode,
                     std::move(result_callback)),
      std::move(bad_message_callback));
}

}  // namespace v8_memory

}  // namespace performance_manager
