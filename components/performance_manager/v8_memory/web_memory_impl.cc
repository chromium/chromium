// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "components/performance_manager/v8_memory/web_memory_aggregator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace v8_memory {

namespace {

mojom::WebMemoryMeasurementPtr BuildMemoryUsageResult(
    const blink::LocalFrameToken& frame_token,
    const ProcessNode* process_node) {
  const auto& frame_nodes = process_node->GetFrameNodes();
  const auto it =
      base::ranges::find(frame_nodes, frame_token, &FrameNode::GetFrameToken);

  if (it == frame_nodes.end()) {
    // The frame no longer exists.
    return mojom::WebMemoryMeasurement::New();
  }
  return WebMemoryAggregator(*it).AggregateMeasureMemoryResult();
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

// Checks if the frame referenced by |rfh_proxy| is crossOriginIsolated. If so,
// invokes |measure_memory_callback| on the PM sequences. If not, invokes
// |bad_message_callback| instead. If the frame disappears at any point, does
// nothing.
void CheckIsCrossOriginIsolatedOnUISeq(
    const RenderFrameHostProxy& rfh_proxy,
    WebMeasureMemorySecurityChecker::MeasureMemoryCallback
        measure_memory_callback,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh = rfh_proxy.Get();
  if (!rfh) {
    // Frame was deleted before the task ran.
    return;
  }
  if (rfh->GetWebExposedIsolationLevel() ==
          content::WebExposedIsolationLevel::kNotIsolated &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    std::move(bad_message_callback)
        .Run("Requesting frame must be cross-origin isolated.");
    return;
  }
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(std::move(measure_memory_callback),
                     PerformanceManager::GetFrameNodeForRenderFrameHost(rfh)));
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
void WebMemoryMeasurer::MeasureMemory(mojom::WebMemoryMeasurement::Mode mode,
                                      MeasurementCallback callback,
                                      base::WeakPtr<FrameNode> frame_node) {
  if (!frame_node) {
    // Frame was deleted while validating it on the UI sequence.
    return;
  }

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
    MeasureMemoryCallback measure_memory_callback,
    mojo::ReportBadMessageCallback bad_message_callback) const {
  DCHECK(frame);
  DCHECK_ON_GRAPH_SEQUENCE(frame->GetGraph());

  // TODO(crbug.com/40132061): The frame may have navigated since it sent the
  // measureMemory request. We could return true if the new document is allowed
  // to measure memory, but the actual document that sent the request is not.
  // If that happens the DocumentCoordinationUnit mojo interface is reset so
  // the measurement result will be thrown away, so this is not a security
  // issue, but it does mean doing extra work.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CheckIsCrossOriginIsolatedOnUISeq,
                                frame->GetRenderFrameHostProxy(),
                                std::move(measure_memory_callback),
                                std::move(bad_message_callback)));
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
      base::BindOnce(&WebMemoryMeasurer::MeasureMemory, mode,
                     std::move(result_callback)),
      std::move(bad_message_callback));
}

}  // namespace v8_memory

}  // namespace performance_manager
