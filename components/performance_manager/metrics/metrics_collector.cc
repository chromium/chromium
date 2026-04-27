// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/metrics_collector.h"

#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

namespace {

void RecordProcessLifetime(std::string_view histogram_name,
                           base::TimeDelta lifetime) {
  base::UmaHistogramCustomTimes(histogram_name, lifetime, base::Seconds(1),
                                base::Days(1), 100);
}

void RecordShortProcessLifetime(std::string_view histogram_name,
                                base::TimeDelta lifetime) {
  base::UmaHistogramLongTimes(histogram_name, lifetime);
}

void OnRendererDestroyed(const ProcessNode* process_node,
                         base::TimeDelta lifetime) {
  RecordProcessLifetime("Renderer.ProcessLifetime3", lifetime);

  ProcessNode::ContentTypes content_types =
      process_node->GetHostedContentTypes();
  if (content_types.Has(ProcessNode::ContentType::kExtension)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.Extension", lifetime);
  } else if (!content_types.Has(ProcessNode::ContentType::kNavigatedFrame)) {
    if (content_types.Has(ProcessNode::ContentType::kWorker)) {
      RecordProcessLifetime("Renderer.ProcessLifetime3.Worker", lifetime);
    } else if (content_types.Has(ProcessNode::ContentType::kMainFrame) ||
               content_types.Has(ProcessNode::ContentType::kSubframe)) {
      RecordProcessLifetime("Renderer.ProcessLifetime3.Speculative", lifetime);
    } else {
      RecordProcessLifetime("Renderer.ProcessLifetime3.Empty", lifetime);
    }
  } else if (content_types.Has(ProcessNode::ContentType::kMainFrame)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.MainFrame", lifetime);
  } else if (content_types.Has(ProcessNode::ContentType::kAd)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.Subframe_Ad", lifetime);
  } else if (content_types.Has(ProcessNode::ContentType::kSubframe)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.Subframe_NoAd", lifetime);
  } else {
    NOTREACHED();
  }
}

}  // namespace

MetricsCollector::MetricsCollector() = default;

MetricsCollector::~MetricsCollector() = default;

void MetricsCollector::OnPassedToGraph(Graph* graph) {
  graph->AddProcessNodeObserver(this);
}

void MetricsCollector::OnTakenFromGraph(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
}

void MetricsCollector::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  // Ignore process creation.
  if (!process_node->GetExitStatus().has_value()) {
    return;
  }

  OnProcessDestroyed(process_node);
}

void MetricsCollector::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  // If the ProcessNode is destroyed with a valid process handle, consider this
  // the end of the process' life.
  if (process_node->GetProcess().IsValid()) {
    OnProcessDestroyed(process_node);
  }
}


void MetricsCollector::OnProcessDestroyed(const ProcessNode* process_node) {
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks launch_time = process_node->GetLaunchTime();

  if (launch_time.is_null()) {
    // Terminating a process quickly after initiating its launch (for example
    // with FastShutdownIfPossible()) may result in receiving
    // RenderProcessHostObserver::RenderProcessExited() without a corresponding
    // RenderProcessHostCreationObserver::OnRenderProcessHostCreated(). In this
    // case, GetLaunchTime() won't be set. It's correct to report a lifetime of
    // 0 in this case.
    launch_time = now;
  }

  const base::TimeDelta lifetime = now - launch_time;

  if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
    OnRendererDestroyed(process_node, lifetime);
  } else if (process_node->GetProcessType() == content::PROCESS_TYPE_UTILITY) {
    // Utility processes are known to often have short lifetimes. There are
    // exceptions like the network service that could be broken out later if the
    // data suggests it's necessary.
    RecordShortProcessLifetime("ChildProcess.ProcessLifetime.Utility",
                               lifetime);
  }
}

}  // namespace performance_manager
