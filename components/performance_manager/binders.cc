// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/binders.h"

#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

void BindProcessCoordinationUnit(
    int render_process_host_id,
    mojo::PendingReceiver<performance_manager::mojom::ProcessCoordinationUnit>
        receiver) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_host_id);
  if (!render_process_host)
    return;

  performance_manager::RenderProcessUserData* user_data =
      performance_manager::RenderProcessUserData::GetForRenderProcessHost(
          render_process_host);

  DCHECK(performance_manager::PerformanceManagerImpl::IsAvailable());
  performance_manager::PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&performance_manager::ProcessNodeImpl::Bind,
                                base::Unretained(user_data->process_node()),
                                std::move(receiver)));
}

void BindDocumentCoordinationUnit(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<performance_manager::mojom::DocumentCoordinationUnit>
        receiver) {
  auto* content = content::WebContents::FromRenderFrameHost(host);
  // |content| can be null if RenderFrameHost's delegate is not a WebContents.
  if (!content)
    return;
  auto* helper =
      performance_manager::PerformanceManagerTabHelper::FromWebContents(
          content);
  // This condition is for testing-only. We should handle a bind request after
  // PerformanceManagerTabHelper is attached to WebContents.
  if (!helper)
    return;
  return helper->BindDocumentCoordinationUnit(host, std::move(receiver));
}

}  // namespace performance_manager