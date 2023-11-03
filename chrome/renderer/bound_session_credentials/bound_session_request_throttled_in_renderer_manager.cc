// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "content/public/child/child_thread.h"

// static
scoped_refptr<BoundSessionRequestThrottledInRendererManager>
BoundSessionRequestThrottledInRendererManager::Create(
    mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledHandler>
        pending_remote) {
  CHECK(pending_remote.is_valid());
  scoped_refptr<BoundSessionRequestThrottledInRendererManager> helper =
      base::WrapRefCounted(new BoundSessionRequestThrottledInRendererManager());
  content::ChildThread::Get()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BoundSessionRequestThrottledInRendererManager::Initialize,
                     helper, std::move(pending_remote)));
  return helper;
}

BoundSessionRequestThrottledInRendererManager::
    BoundSessionRequestThrottledInRendererManager() {
  DETACH_FROM_SEQUENCE(my_sequence_checker_);
}

BoundSessionRequestThrottledInRendererManager::
    ~BoundSessionRequestThrottledInRendererManager() {
  DETACH_FROM_SEQUENCE(my_sequence_checker_);
}

void BoundSessionRequestThrottledInRendererManager::Initialize(
    mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledHandler>
        pending_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  remote_.Bind(std::move(pending_remote));
  remote_.set_disconnect_handler(base::BindOnce(
      &BoundSessionRequestThrottledInRendererManager::OnRemoteDisconnected,
      base::Unretained(this)));
}

void BoundSessionRequestThrottledInRendererManager::
    HandleRequestBlockedOnCookie(
        ResumeOrCancelThrottledRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  CHECK(remote_.is_bound());
  CHECK(!callback.is_null());
  if (!remote_.is_connected()) {
    // TODO(b/279719656): Make sure the callback is always called
    // asynchronously.
    std::move(callback).Run(UnblockAction::kCancel);
    return;
  }

  resume_or_cancel_deferred_request_callbacks_.push_back(std::move(callback));
  if (resume_or_cancel_deferred_request_callbacks_.size() == 1) {
    CallRemoteHandleRequestBlockedOnCookie();
  }
}

void BoundSessionRequestThrottledInRendererManager::
    CallRemoteHandleRequestBlockedOnCookie() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  remote_->HandleRequestBlockedOnCookie(base::BindOnce(
      &BoundSessionRequestThrottledInRendererManager::ResumeAllDeferredRequests,
      base::Unretained(this)));
}

void BoundSessionRequestThrottledInRendererManager::OnRemoteDisconnected() {
  // The remote is expected to be disconnected on service shutdown which should
  // happen on profile shutdown. Cancel requests to mitigate the risk of sending
  // requests without the required short lived cookie.
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  CancelAllDeferredRequests();
}

void BoundSessionRequestThrottledInRendererManager::
    CancelAllDeferredRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  RunAllCallbacks(UnblockAction::kCancel);
}

void BoundSessionRequestThrottledInRendererManager::
    ResumeAllDeferredRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  RunAllCallbacks(UnblockAction::kResume);
}

void BoundSessionRequestThrottledInRendererManager::RunAllCallbacks(
    UnblockAction action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  std::vector<ResumeOrCancelThrottledRequestCallback> callbacks;
  std::swap(callbacks, resume_or_cancel_deferred_request_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(action);
  }
}
