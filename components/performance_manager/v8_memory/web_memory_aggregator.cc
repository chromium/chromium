// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "url/gurl.h"

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
    auto* data = v8_memory::V8DetailedMemoryFrameData::ForFrameNode(frame_node);
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

// Implements the public function in public/v8_memory/web_memory.h
void WebMeasureMemory(
    const FrameNode* frame_node,
    mojom::WebMemoryMeasurement::Mode mode,
    base::OnceCallback<void(mojom::WebMemoryMeasurementPtr)> callback) {
  auto web_memory_aggregator = std::make_unique<WebMemoryAggregator>(
      frame_node->GetFrameToken(),
      WebMeasurementModeToRequestMeasurementMode(mode), std::move(callback));

  // Create a measurement complete callback to own |web_memory_aggregator|. It
  // will be deleted when the callback is executed or dropped.
  V8DetailedMemoryRequestOneShot* request = web_memory_aggregator->request();
  auto measurement_complete_callback =
      base::BindOnce(&WebMemoryAggregator::MeasurementComplete,
                     std::move(web_memory_aggregator));

  // Start memory measurement for the process of the given frame.
  request->StartMeasurement(frame_node->GetProcessNode(),
                            std::move(measurement_complete_callback));
}

WebMemoryAggregator::WebMemoryAggregator(
    const blink::LocalFrameToken& frame_token,
    V8DetailedMemoryRequest::MeasurementMode mode,
    MeasurementCallback callback)
    : frame_token_(frame_token),
      callback_(std::move(callback)),
      request_(std::make_unique<V8DetailedMemoryRequestOneShot>(mode)) {}

WebMemoryAggregator::~WebMemoryAggregator() = default;

void WebMemoryAggregator::MeasurementComplete(
    const ProcessNode* process_node,
    const V8DetailedMemoryProcessData*) {
  std::move(callback_).Run(BuildMemoryUsageResult(frame_token_, process_node));
}

}  // namespace v8_memory

}  // namespace performance_manager
