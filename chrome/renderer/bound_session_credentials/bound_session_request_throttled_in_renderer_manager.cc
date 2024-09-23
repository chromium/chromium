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
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace {
using UnblockAction = BoundSessionRequestThrottledHandler::UnblockAction;
using ResumeOrCancelThrottledRequestCallback =
    BoundSessionRequestThrottledHandler::ResumeOrCancelThrottledRequestCallback;
using ResumeBlockedRequestsTrigger =
    chrome::mojom::ResumeBlockedRequestsTrigger;

void OnHandleRequestBlockedOnCookie(
    ResumeOrCancelThrottledRequestCallback callback,
    ResumeBlockedRequestsTrigger trigger) {
  UnblockAction action =
      trigger == ResumeBlockedRequestsTrigger::kRendererDisconnected
          ? UnblockAction::kCancel
          : UnblockAction::kResume;
  std::move(callback).Run(action, trigger);
}
}  // namespace

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
}

void BoundSessionRequestThrottledInRendererManager::
    HandleRequestBlockedOnCookie(
        const GURL& untrusted_request_url,
        ResumeOrCancelThrottledRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  CHECK(remote_.is_bound());
  CHECK(!callback.is_null());

  remote_->HandleRequestBlockedOnCookie(
      untrusted_request_url,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnHandleRequestBlockedOnCookie, std::move(callback)),
          chrome::mojom::ResumeBlockedRequestsTrigger::kRendererDisconnected));
}
