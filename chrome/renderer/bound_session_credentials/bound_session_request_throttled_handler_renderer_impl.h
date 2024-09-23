// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_HANDLER_RENDERER_IMPL_H_
#define CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_HANDLER_RENDERER_IMPL_H_

#include "chrome/common/bound_session_request_throttled_handler.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"

class BoundSessionRequestThrottledHandlerRendererImpl
    : public BoundSessionRequestThrottledHandler {
 public:
  explicit BoundSessionRequestThrottledHandlerRendererImpl(
      scoped_refptr<BoundSessionRequestThrottledInRendererManager>
          bound_session_request_throttled_manager,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  ~BoundSessionRequestThrottledHandlerRendererImpl() override;

  BoundSessionRequestThrottledHandlerRendererImpl(
      const BoundSessionRequestThrottledHandlerRendererImpl&) = delete;
  BoundSessionRequestThrottledHandlerRendererImpl& operator=(
      const BoundSessionRequestThrottledHandlerRendererImpl&) = delete;

  // BoundSessionRequestThrottledHandler:
  void HandleRequestBlockedOnCookie(
      const GURL& untrusted_request_url,
      ResumeOrCancelThrottledRequestCallback callback) override;

 private:
  const scoped_refptr<BoundSessionRequestThrottledInRendererManager>
      bound_session_request_throttled_manager_;
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};
#endif  // CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_HANDLER_RENDERER_IMPL_H_
