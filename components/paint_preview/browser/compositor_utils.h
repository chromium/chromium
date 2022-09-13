// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_COMPOSITOR_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_COMPOSITOR_UTILS_H_

#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace paint_preview {

// Starts the compositor service in a utility process.
std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
StartCompositorService(base::OnceClosure disconnect_handler);

// Creates a utility process via the service manager that is sandboxed and
// running an instance of the PaintPreviewCompositorCollectionImpl. This can be
// used to create compositor instances that composite Paint Previews into
// bitmaps. The service is killed when the remote goes out of scope.
mojo::Remote<mojom::PaintPreviewCompositorCollection>
CreateCompositorCollection();

// Same as the above method, but the initialization is performed for a remote
// or pending remote owned by the caller. NOTE: the caller must explicitly
// initialize the discardable memory manager.
//
// EXAMPLE USAGE: (pending remote)
//
// mojo::PendingRemote<mojom::PaintPreviewCompositorCollection> pending_remote;
// CreateCompositorCollectionPending(
//     pending_remote.InitWithNewPipeAndPassReceiver()));
//
// mojo::Remote<mojom::PaintPreviewCompositorCollection>
//     remote(pending_remote);
// BindDiscardableSharedMemoryManager(&remote);
void CreateCompositorCollectionPending(
    mojo::PendingReceiver<mojom::PaintPreviewCompositorCollection>
        pending_receiver);

// Binds a discardable memory manager for |collection|.
// NOTE: this requires the remote to be bound.
void BindDiscardableSharedMemoryManager(
    mojo::Remote<mojom::PaintPreviewCompositorCollection>* collection);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_COMPOSITOR_UTILS_H_
