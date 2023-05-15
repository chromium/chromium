// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_RENDERER_IMPL_H_
#define CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_RENDERER_IMPL_H_

#include "chrome/common/bound_session_request_throttled_listener.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"

class BoundSessionRequestThrottledListenerRendererImpl
    : public BoundSessionRequestThrottledListener {
 public:
  explicit BoundSessionRequestThrottledListenerRendererImpl(
      scoped_refptr<BoundSessionRequestThrottledInRendererManager>
          bound_session_request_throttled_manager,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  ~BoundSessionRequestThrottledListenerRendererImpl() override;

  BoundSessionRequestThrottledListenerRendererImpl(
      const BoundSessionRequestThrottledListenerRendererImpl&) = delete;
  BoundSessionRequestThrottledListenerRendererImpl& operator=(
      const BoundSessionRequestThrottledListenerRendererImpl&) = delete;

  // BoundSessionRequestThrottledListener:
  void OnRequestBlockedOnCookie(
      ResumeOrCancelThrottledRequestCallback callback) override;

 private:
  const scoped_refptr<BoundSessionRequestThrottledInRendererManager>
      bound_session_request_throttled_manager_;
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};
#endif  // CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_RENDERER_IMPL_H_
