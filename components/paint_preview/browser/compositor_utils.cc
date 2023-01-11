// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/compositor_utils.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/paint_preview/browser/paint_preview_compositor_service_impl.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace paint_preview {

namespace {

void BindDiscardableSharedMemoryManagerOnIOThread(
    mojo::PendingReceiver<
        discardable_memory::mojom::DiscardableSharedMemoryManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
      std::move(receiver));
}

}  // namespace

std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
StartCompositorService(base::OnceClosure disconnect_handler) {
  // Create a dedicated sequence for communicating with the compositor. This
  // sequence will handle message serialization/deserialization of bitmaps so it
  // affects user visible elements. This is an implementation detail and the
  // caller should continue to communicate with the compositor via the sequence
  // that called this.
  auto compositor_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::ThreadPolicy::MUST_USE_FOREGROUND});

  // The discardable memory manager isn't initialized here. This is handled in
  // the constructor of PaintPreviewCompositorServiceImpl once the pending
  // remote becomes bound.
  mojo::PendingRemote<mojom::PaintPreviewCompositorCollection> pending_remote;
  compositor_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateCompositorCollectionPending,
                     pending_remote.InitWithNewPipeAndPassReceiver()));

  return std::unique_ptr<PaintPreviewCompositorServiceImpl,
                         base::OnTaskRunnerDeleter>(
      new PaintPreviewCompositorServiceImpl(std::move(pending_remote),
                                            compositor_task_runner,
                                            std::move(disconnect_handler)),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
}

mojo::Remote<mojom::PaintPreviewCompositorCollection>
CreateCompositorCollection() {
  mojo::Remote<mojom::PaintPreviewCompositorCollection> collection;
  CreateCompositorCollectionPending(collection.BindNewPipeAndPassReceiver());
  BindDiscardableSharedMemoryManager(&collection);
  return collection;
}

void CreateCompositorCollectionPending(
    mojo::PendingReceiver<mojom::PaintPreviewCompositorCollection> collection) {
  content::ServiceProcessHost::Launch<mojom::PaintPreviewCompositorCollection>(
      std::move(collection),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_PAINT_PREVIEW_COMPOSITOR_SERVICE_DISPLAY_NAME)
          .Pass());
}

void BindDiscardableSharedMemoryManager(
    mojo::Remote<mojom::PaintPreviewCompositorCollection>* collection) {
  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      discardable_memory_manager;

  // Set up the discardable memory manager.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BindDiscardableSharedMemoryManagerOnIOThread,
          discardable_memory_manager.InitWithNewPipeAndPassReceiver()));
  collection->get()->SetDiscardableSharedMemoryManager(
      std::move(discardable_memory_manager));
}

}  // namespace paint_preview
