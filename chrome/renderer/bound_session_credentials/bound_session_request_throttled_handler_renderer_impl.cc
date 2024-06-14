// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_handler_renderer_impl.h"

#include <memory>

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"
#include "content/public/child/child_thread.h"
#include "url/gurl.h"

BoundSessionRequestThrottledHandlerRendererImpl::
    BoundSessionRequestThrottledHandlerRendererImpl(
        scoped_refptr<BoundSessionRequestThrottledInRendererManager>
            bound_session_request_throttled_manager,
        scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : bound_session_request_throttled_manager_(
          bound_session_request_throttled_manager),
      io_task_runner_(io_task_runner) {
  CHECK(bound_session_request_throttled_manager);
}

BoundSessionRequestThrottledHandlerRendererImpl::
    ~BoundSessionRequestThrottledHandlerRendererImpl() = default;

void BoundSessionRequestThrottledHandlerRendererImpl::
    HandleRequestBlockedOnCookie(
        const GURL& untrusted_request_url,
        ResumeOrCancelThrottledRequestCallback callback) {
  // Bind the callback to the current sequence to ensure invoking `Run()` will
  // run the callback on the current sequence.
  ResumeOrCancelThrottledRequestCallback callback_bound_to_current_sequence =
      base::BindPostTaskToCurrentDefault(std::move(callback));

  // `BoundSessionRequestThrottledInRendererManager` should only be called on
  // the IO thread.
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BoundSessionRequestThrottledInRendererManager::
                                    HandleRequestBlockedOnCookie,
                                bound_session_request_throttled_manager_,
                                untrusted_request_url,
                                std::move(callback_bound_to_current_sequence)));
}
