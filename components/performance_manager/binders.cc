// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/binders.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace performance_manager {

namespace {

void BindRenderProcessCoordinationUnit(
    content::ChildProcessId render_process_host_id,
    mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_host_id);
  if (!render_process_host)
    return;

  RenderProcessUserData* user_data =
      RenderProcessUserData::GetForRenderProcessHost(render_process_host);

  user_data->process_node()->BindRenderProcessCoordinationUnit(
      std::move(receiver));
}

void BindChildProcessCoordinationUnit(
    base::WeakPtr<ProcessNode> process_node,
    mojo::PendingReceiver<mojom::ChildProcessCoordinationUnit> receiver) {
  if (process_node) {
    ProcessNodeImpl::FromNode(process_node.get())
        ->BindChildProcessCoordinationUnit(std::move(receiver));
  }
}

void BindChildProcessCoordinationUnitForRenderProcessHost(
    content::ChildProcessId render_process_host_id,
    mojo::PendingReceiver<mojom::ChildProcessCoordinationUnit> receiver) {
  BindChildProcessCoordinationUnit(
      PerformanceManagerImpl::GetProcessNodeForRenderProcessHostId(
          render_process_host_id),
      std::move(receiver));
}

void BindChildProcessCoordinationUnitForBrowserChildProcessHost(
    content::BrowserChildProcessHost* host,
    mojo::PendingReceiver<mojom::ChildProcessCoordinationUnit> receiver) {
  BindChildProcessCoordinationUnit(
      PerformanceManagerImpl::GetProcessNodeForBrowserChildProcessHost(host),
      std::move(receiver));
}

void BindDocumentCoordinationUnit(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver) {
  auto* content = content::WebContents::FromRenderFrameHost(host);
  // |content| can be null if RenderFrameHost's delegate is not a WebContents.
  if (!content)
    return;
  auto* helper = PerformanceManagerTabHelper::FromWebContents(content);
  // This condition is for testing-only. We should handle a bind request after
  // PerformanceManagerTabHelper is attached to WebContents.
  if (!helper)
    return;
  return helper->BindDocumentCoordinationUnit(host, std::move(receiver));
}

}  // namespace

void Binders::ExposeInterfacesToRendererProcess(
    service_manager::BinderRegistry* registry,
    content::RenderProcessHost* host) {
  registry->AddInterface(
      base::BindRepeating(&BindRenderProcessCoordinationUnit, host->GetID()),
      base::SequencedTaskRunner::GetCurrentDefault());
  registry->AddInterface(
      base::BindRepeating(&BindChildProcessCoordinationUnitForRenderProcessHost,
                          host->GetID()),
      base::SequencedTaskRunner::GetCurrentDefault());
}

void Binders::ExposeInterfacesToBrowserChildProcess(
    mojo::BinderMapWithContext<content::BrowserChildProcessHost*>* map) {
  map->Add<mojom::ChildProcessCoordinationUnit>(
      &BindChildProcessCoordinationUnitForBrowserChildProcessHost);
}

void Binders::ExposeInterfacesToRenderFrame(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<mojom::DocumentCoordinationUnit>(&BindDocumentCoordinationUnit);
}

}  // namespace performance_manager
