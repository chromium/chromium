// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/texture_deleter.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/single_release_callback.h"
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

static void PostTaskFromMainToImplThread(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    ReleaseCallback run_impl_callback,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  // This posts the task to RunDeleteTextureOnImplThread().
  impl_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(run_impl_callback), sync_token, is_lost));
}

TextureDeleter::TextureDeleter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : impl_task_runner_(std::move(task_runner)) {}

TextureDeleter::~TextureDeleter() {
  for (size_t i = 0; i < impl_callbacks_.size(); ++i)
    impl_callbacks_.at(i)->Run(gpu::SyncToken(), true);
}

std::unique_ptr<SingleReleaseCallback> TextureDeleter::GetReleaseCallback(
    scoped_refptr<ContextProvider> context_provider,
    const gpu::Mailbox& mailbox) {
  // This callback owns the |context_provider|. It must be destroyed on the impl
  // thread. Upon destruction of this class, the callback must immediately be
  // destroyed.
  std::unique_ptr<SingleReleaseCallback> impl_callback =
      SingleReleaseCallback::Create(base::BindOnce(
          &DeleteTextureOnImplThread, std::move(context_provider), mailbox));

  impl_callbacks_.push_back(std::move(impl_callback));

  // The raw pointer to the impl-side callback is valid as long as this
  // class is alive. So we guard it with a WeakPtr.
  ReleaseCallback run_impl_callback = base::BindOnce(
      &TextureDeleter::RunDeleteTextureOnImplThread,
      weak_ptr_factory_.GetWeakPtr(), impl_callbacks_.back().get());

  // Provide a callback for the main thread that posts back to the impl
  // thread.
  std::unique_ptr<SingleReleaseCallback> main_callback;
  if (impl_task_runner_) {
    main_callback = SingleReleaseCallback::Create(
        base::BindOnce(&PostTaskFromMainToImplThread, impl_task_runner_,
                       std::move(run_impl_callback)));
  } else {
    main_callback = SingleReleaseCallback::Create(std::move(run_impl_callback));
  }

  return main_callback;
}

void TextureDeleter::RunDeleteTextureOnImplThread(
    SingleReleaseCallback* impl_callback,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  for (size_t i = 0; i < impl_callbacks_.size(); ++i) {
    if (impl_callbacks_[i].get() == impl_callback) {
      // Run the callback, then destroy it here on the impl thread.
      impl_callbacks_[i]->Run(sync_token, is_lost);
      impl_callbacks_.erase(impl_callbacks_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "The Callback returned by GetDeleteCallback() was called "
               << "more than once.";
}

}  // namespace viz
