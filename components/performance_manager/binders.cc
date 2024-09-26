// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/binders.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace performance_manager {

namespace {

void BindProcessCoordinationUnit(
    int render_process_host_id,
    mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_host_id);
  if (!render_process_host)
    return;

  RenderProcessUserData* user_data =
      RenderProcessUserData::GetForRenderProcessHost(render_process_host);

  DCHECK(PerformanceManagerImpl::IsAvailable());
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&ProcessNodeImpl::Bind,
                                base::Unretained(user_data->process_node()),
                                std::move(receiver)));
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
      base::BindRepeating(&BindProcessCoordinationUnit, host->GetID()),
      base::SequencedTaskRunner::GetCurrentDefault());
}

void Binders::ExposeInterfacesToRenderFrame(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<mojom::DocumentCoordinationUnit>(
      base::BindRepeating(&BindDocumentCoordinationUnit));
}

}  // namespace performance_manager
