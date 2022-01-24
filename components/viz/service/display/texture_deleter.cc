// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/texture_deleter.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace viz {

static void DeleteTextureOnImplThread(
    const scoped_refptr<ContextProvider>& context_provider,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  context_provider->SharedImageInterface()->DestroySharedImage(sync_token,
                                                               mailbox);
}

TextureDeleter::TextureDeleter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : impl_task_runner_(std::move(task_runner)) {}

TextureDeleter::~TextureDeleter() {
  for (auto& callback : impl_callbacks_)
    std::move(*callback).Run(gpu::SyncToken(), /*is_lost=*/true);
}

ReleaseCallback TextureDeleter::GetReleaseCallback(
    scoped_refptr<ContextProvider> context_provider,
    const gpu::Mailbox& mailbox) {
  // This callback owns the |context_provider|. It must be destroyed on the impl
  // thread. Upon destruction of this class, the callback must immediately be
  // destroyed.
  auto impl_callback = std::make_unique<ReleaseCallback>(base::BindOnce(
      &DeleteTextureOnImplThread, std::move(context_provider), mailbox));

  impl_callbacks_.push_back(std::move(impl_callback));

  // The raw pointer to the impl-side callback is valid as long as this
  // class is alive. So we guard it with a WeakPtr.
  ReleaseCallback run_impl_callback = base::BindOnce(
      &TextureDeleter::RunDeleteTextureOnImplThread,
      weak_ptr_factory_.GetWeakPtr(), impl_callbacks_.back().get());

  // Provide a callback for the main thread that posts back to the impl
  // thread.
  ReleaseCallback main_callback;
  if (impl_task_runner_) {
    main_callback =
        base::BindPostTask(impl_task_runner_, std::move(run_impl_callback));
  } else {
    main_callback = std::move(run_impl_callback);
  }

  return main_callback;
}

void TextureDeleter::RunDeleteTextureOnImplThread(
    ReleaseCallback* impl_callback,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  for (size_t i = 0; i < impl_callbacks_.size(); ++i) {
    if (impl_callbacks_[i].get() == impl_callback) {
      // Run the callback, then destroy it here on the impl thread.
      std::move(*impl_callbacks_[i]).Run(sync_token, is_lost);
      impl_callbacks_.erase(impl_callbacks_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "The Callback returned by GetDeleteCallback() was called "
               << "more than once.";
}

}  // namespace viz
