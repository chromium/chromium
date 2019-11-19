// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_collection_impl.h"

#include <utility>

#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkFontMgr.h"

#if defined(OS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#endif

namespace paint_preview {

PaintPreviewCompositorCollectionImpl::PaintPreviewCompositorCollectionImpl(
    mojo::PendingReceiver<mojom::PaintPreviewCompositorCollection> receiver,
    bool initialize_environment,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std ::move(io_task_runner)) {
  if (receiver)
    receiver_.Bind(std::move(receiver));

  if (!initialize_environment)
    return;

#if defined(OS_WIN)
  // Initialize direct write font proxy so skia can use it.
  content::InitializeDWriteFontProxy();
#endif

  // TODO(crbug/1013585): PDF compositor initializes Blink to leverage some
  // codecs for images. This is a huge overhead and shouldn't be necessary for
  // us. However, this may break some formats (WEBP?) so we may need to force
  // encoding to PNG or we could provide our own codec implementations.

  // Sanity check that fonts are working.
  DCHECK(SkFontMgr::RefDefault()->countFamilies());
}

PaintPreviewCompositorCollectionImpl::~PaintPreviewCompositorCollectionImpl() {
#if defined(OS_WIN)
  content::UninitializeDWriteFontProxy();
#endif
}

void PaintPreviewCompositorCollectionImpl::SetDiscardableSharedMemoryManager(
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager> manager) {
  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      manager_remote(std::move(manager));
  discardable_shared_memory_manager_ = std::make_unique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_remote), io_task_runner_);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());
}

void PaintPreviewCompositorCollectionImpl::CreateCompositor(
    mojo::PendingReceiver<mojom::PaintPreviewCompositor> receiver,
    PaintPreviewCompositorCollectionImpl::CreateCompositorCallback callback) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  compositors_.insert(
      {token,
       std::make_unique<PaintPreviewCompositorImpl>(
           std::move(receiver),
           base::BindOnce(&PaintPreviewCompositorCollectionImpl::OnDisconnect,
                          base::Unretained(this), token))});
  std::move(callback).Run(token);
}

void PaintPreviewCompositorCollectionImpl::ListCompositors(
    ListCompositorsCallback callback) {
  std::vector<base::UnguessableToken> ids;
  ids.reserve(compositors_.size());
  for (const auto& compositor : compositors_)
    ids.push_back(compositor.first);
  std::move(callback).Run(std::move(ids));
}

void PaintPreviewCompositorCollectionImpl::OnDisconnect(
    const base::UnguessableToken& id) {
  compositors_.erase(id);
}

}  // namespace paint_preview
