// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/process_priority_policy.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {
namespace policies {

namespace {

#if DCHECK_IS_ON()
size_t g_instance_count = 0;
#endif

// Used as a testing seam. If this is set then SetProcessPriorityOnUiThread will
// invoke the provided callback immediately after calling SetPriorityOverride on
// the RenderProcessHost.
ProcessPriorityPolicy::SetPriorityOnUiThreadCallback* g_callback = nullptr;

// Maps a TaskPriority to a "is_foreground" bool. Process priorities are
// currently simply "background" or "foreground", despite there actually being
// more expressive power on most platforms.
bool IsForegroundTaskPriority(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::BEST_EFFORT:
      return false;

    case base::TaskPriority::USER_VISIBLE:
    case base::TaskPriority::USER_BLOCKING:
      break;
  }

  return true;
}

// Helper function for setting the RenderProcessHost priority.
void SetProcessPriorityOnUIThread(RenderProcessHostProxy rph_proxy,
                                  bool foreground) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Deliver the policy message if the RPH still exists by the time the
  // message arrives. Note that this will involve yet another bounce over to
  // the process launcher thread.
  auto* rph = rph_proxy.Get();
  if (rph)
    rph->SetPriorityOverride(foreground);

  // Invoke the testing seam callback if one was provided.
  if (g_callback && !g_callback->is_null())
    g_callback->Run(rph_proxy, foreground);
}

// Dispatches a process priority change to the RenderProcessHost associated with
// a given ProcessNode. The task is posted to the UI thread, where the RPH
// lives.
void DispatchSetProcessPriority(const ProcessNode* process_node,
                                bool foreground) {
  // TODO(chrisha): This will actually result in a further thread-hop over to
  // the process launcher thread. If we migrate to process priority logic being
  // driven 100% from the PM, we could post directly to the launcher thread
  // via the base::Process directly.
  const auto& proxy = process_node->GetRenderProcessHostProxy();
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&SetProcessPriorityOnUIThread, proxy, foreground));
}

}  // namespace

ProcessPriorityPolicy::ProcessPriorityPolicy() {
#if DCHECK_IS_ON()
  DCHECK_EQ(0u, g_instance_count);
  ++g_instance_count;
#endif
}

ProcessPriorityPolicy::~ProcessPriorityPolicy() {
#if DCHECK_IS_ON()
  DCHECK_EQ(1u, g_instance_count);
  --g_instance_count;
#endif
}

// static
void ProcessPriorityPolicy::SetCallbackForTesting(
    SetPriorityOnUiThreadCallback callback) {
  ClearCallbackForTesting();
  g_callback = new SetPriorityOnUiThreadCallback(callback);
}

void ProcessPriorityPolicy::ClearCallbackForTesting() {
  if (!g_callback)
    return;
  delete g_callback;
  g_callback = nullptr;
}

void ProcessPriorityPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->IsEmpty());
  graph->AddProcessNodeObserver(this);
}

void ProcessPriorityPolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
}

void ProcessPriorityPolicy::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  // Set the initial process priority.
  // TODO(chrisha): Get provisional nodes working so we can make an informed
  // choice in the graph (processes launching ads-to-be, or extensions, or
  // frames for backgrounded tabs, etc, can be launched with background
  // priority).
  // TODO(chrisha): Make process creation take a detour through the graph in
  // order to get the initial priority parameter that is set here. Currently
  // this is effectively a nop.
  DispatchSetProcessPriority(
      process_node, IsForegroundTaskPriority(process_node->GetPriority()));
}

void ProcessPriorityPolicy::OnPriorityChanged(
    const ProcessNode* process_node,
    base::TaskPriority previous_value) {
  bool previous_foreground = IsForegroundTaskPriority(previous_value);
  bool current_foreground =
      IsForegroundTaskPriority(process_node->GetPriority());

  // Only dispatch a message if the resulting process priority has changed.
  if (previous_foreground != current_foreground)
    DispatchSetProcessPriority(process_node, current_foreground);
}

}  // namespace policies
}  // namespace performance_manager
