// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions for binding renderer objects to their performance_manager
// Graph node counterparts in the browser process.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_BINDERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_BINDERS_H_

#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace performance_manager {

void BindProcessCoordinationUnit(
    int render_process_host_id,
    mojo::PendingReceiver<performance_manager::mojom::ProcessCoordinationUnit>
        receiver);

void BindDocumentCoordinationUnit(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<performance_manager::mojom::DocumentCoordinationUnit>
        receiver);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_BINDERS_H_