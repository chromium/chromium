// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/context_cache_controller.h"

#include <chrono>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/client/context_support.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace viz {
namespace {
static const int kIdleCleanupDelaySeconds = 1;
static const int kOldResourceCleanupDelaySeconds = 15;
}  // namespace

ContextCacheController::ScopedToken::ScopedToken() = default;

ContextCacheController::ScopedToken::~ScopedToken() {
  DCHECK(released_);
}

void ContextCacheController::ScopedToken::Release() {
  DCHECK(!released_);
  released_ = true;
}

ContextCacheController::ContextCacheController(
    gpu::ContextSupport* context_support,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_support_(context_support), task_runner_(std::move(task_runner)) {
  // The |weak_factory_| can only be used from a single thread. We
  // create/destroy this class and run callbacks on a single thread, but we
  // want to be able to post callbacks from multiple threads. We need a weak
  // ptr to post callbacks, so acquire one here, while we're on the right
  // thread.
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

ContextCacheController::~ContextCacheController() {
  if (held_visibility_)
    ClientBecameNotVisible(std::move(held_visibility_));
}

void ContextCacheController::SetGrContext(GrContext* gr_context) {
  gr_context_ = gr_context;
}

void ContextCacheController::SetLock(base::Lock* lock) {
  context_lock_ = lock;
}

std::unique_ptr<ContextCacheController::ScopedVisibility>
ContextCacheController::ClientBecameVisible() {
  if (context_lock_)
    context_lock_->AssertAcquired();

  bool became_visible = num_clients_visible_ == 0;
  ++num_clients_visible_;

  if (became_visible)
    context_support_->SetAggressivelyFreeResources(false);

  return base::WrapUnique(new ScopedVisibility());
}

void ContextCacheController::ClientBecameNotVisible(
    std::unique_ptr<ScopedVisibility> scoped_visibility) {
  DCHECK(scoped_visibility);
  scoped_visibility->Release();

  if (context_lock_)
    context_lock_->AssertAcquired();

  DCHECK_GT(num_clients_visible_, 0u);
  --num_clients_visible_;

  if (num_clients_visible_ == 0) {
    // We are freeing resources now - cancel any pending idle callbacks.
    InvalidatePendingIdleCallbacks();

    if (gr_context_)
      gr_context_->freeGpuResources();
    context_support_->SetAggressivelyFreeResources(true);
    context_support_->FlushPendingWork();
  }
}

void ContextCacheController::ClientBecameNotVisibleDuringShutdown(
    std::unique_ptr<ScopedVisibility> scoped_visibility) {
  // We only need to hold on to one visibility token, so free any others that
  // come in.
  if (!held_visibility_)
    held_visibility_ = std::move(scoped_visibility);
  else
    ClientBecameNotVisible(std::move(scoped_visibility));
}

std::unique_ptr<ContextCacheController::ScopedBusy>
ContextCacheController::ClientBecameBusy() {
  if (context_lock_)
    context_lock_->AssertAcquired();

  ++num_clients_busy_;
  // We are busy, cancel any pending idle callbacks.
  InvalidatePendingIdleCallbacks();

  return base::WrapUnique(new ScopedBusy());
}

void ContextCacheController::ClientBecameNotBusy(
    std::unique_ptr<ScopedBusy> scoped_busy) {
  DCHECK(scoped_busy);
  scoped_busy->Release();

  if (context_lock_)
    context_lock_->AssertAcquired();

  DCHECK_GT(num_clients_busy_, 0u);
  --num_clients_busy_;

  // Here we ask GrContext to free any resources that haven't been used in
  // a long while even if it is under budget.
  if (gr_context_) {
    gr_context_->performDeferredCleanup(
        std::chrono::seconds(kOldResourceCleanupDelaySeconds));
  }

  // If we have become idle and we are visible, queue a task to drop resources
  // after a delay. If are not visible, we have already dropped resources.
  if (num_clients_busy_ == 0 && num_clients_visible_ > 0 && task_runner_) {
    // If we already have a callback pending, don't post a new one. The pending
    // callback will handle posting a new callback itself. This prevents us from
    // flooding the system with tasks.
    if (!callback_pending_) {
      {
        base::AutoLock hold(current_idle_generation_lock_);
        PostIdleCallback(current_idle_generation_);
      }
      callback_pending_ = true;
    }
  }
}

void ContextCacheController::PostIdleCallback(
    uint32_t current_idle_generation) const {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ContextCacheController::OnIdle, weak_ptr_,
                     current_idle_generation),
      base::TimeDelta::FromSeconds(kIdleCleanupDelaySeconds));
}

void ContextCacheController::InvalidatePendingIdleCallbacks() {
  base::AutoLock hold(current_idle_generation_lock_);
  ++current_idle_generation_;
}

void ContextCacheController::OnIdle(uint32_t idle_generation) {
  // First check if we should run our idle callback at all. If we have become
  // busy since scheduling, just schedule another idle callback and return.
  {
    base::AutoLock hold(current_idle_generation_lock_);
    if (current_idle_generation_ != idle_generation) {
      PostIdleCallback(current_idle_generation_);
      return;
    }
  }

  // Try to acquire the context lock - if we can't acquire it then we've become
  // busy since checking |current_idle_generation_| above. In this case, just
  // re-post our idle callback and return.
  if (context_lock_ && !context_lock_->Try()) {
    base::AutoLock hold(current_idle_generation_lock_);
    PostIdleCallback(current_idle_generation_);
    return;
  }

  if (gr_context_)
    gr_context_->freeGpuResources();

  // Toggle SetAggressivelyFreeResources to drop command buffer data.
  context_support_->SetAggressivelyFreeResources(true);
  context_support_->FlushPendingWork();
  context_support_->SetAggressivelyFreeResources(false);

  callback_pending_ = false;

  if (context_lock_)
    context_lock_->Release();
}

}  // namespace viz
