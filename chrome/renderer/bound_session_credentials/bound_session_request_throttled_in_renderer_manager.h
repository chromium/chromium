// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_IN_RENDERER_MANAGER_H_
#define CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_IN_RENDERER_MANAGER_H_

#include <memory>
#include <vector>

#include "base/sequence_checker.h"
#include "chrome/common/bound_session_request_throttled_handler.h"
#include "chrome/common/renderer_configuration.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

// This class is used to notify the receiver that one or more requests in
// a bound session is blocked on the bound short lived cookie. The receiver is
// responsible for calling the callback to unblock requests. If the
// mojo connection is closed, requests will be cancelled.
// `this` will notify the receiver of the first blocked request in the list. If
// the list is not empty, it means the receiver has been notified there are
// requests blocked in the renderer, in this case the callback to resume or
// cancel the request will be silently added to the list of deferred requests.
// When the receiver invokes the callback, all deferred requests will be resumed
// as the defer/resume conditions applies in the same manner.
// `this` should be called on the same thread.
class BoundSessionRequestThrottledInRendererManager
    : public base::RefCountedThreadSafe<
          BoundSessionRequestThrottledInRendererManager> {
 public:
  static scoped_refptr<BoundSessionRequestThrottledInRendererManager> Create(
      mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledHandler>
          pending_remote);

  BoundSessionRequestThrottledInRendererManager(
      const BoundSessionRequestThrottledInRendererManager&) = delete;
  BoundSessionRequestThrottledInRendererManager& operator=(
      const BoundSessionRequestThrottledInRendererManager&) = delete;

  // Binds `remote_`.
  // This function must be called before any calls to
  // `HandleRequestBlockedOnCookie()`.
  void Initialize(
      mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledHandler>
          pending_remote);

  // Called when bound session requests are blocked on expired cookie. This call
  // triggers the bound session cookie rotation if the cookie is not fresh.
  // The callback is invoked when:
  // - The receiver invokes the passed callback. This is expected to happen when
  // the cookie is fresh, rotation failure or timeout.
  // - The remote connection is closed (on profile shutdown).
  // This class expects `callback` to be bound to the sequence on which it
  // should run.
  // Marked virtual for testing.
  virtual void HandleRequestBlockedOnCookie(
      BoundSessionRequestThrottledHandler::
          ResumeOrCancelThrottledRequestCallback callback);

 private:
  using UnblockAction = BoundSessionRequestThrottledHandler::UnblockAction;
  using ResumeOrCancelThrottledRequestCallback =
      BoundSessionRequestThrottledHandler::
          ResumeOrCancelThrottledRequestCallback;

  friend class base::RefCountedThreadSafe<
      BoundSessionRequestThrottledInRendererManager>;
  friend class BoundSessionRequestThrottledInRendererManagerTest;
  friend class MockBoundSessionRequestThrottledInRendererManager;

  BoundSessionRequestThrottledInRendererManager();
  virtual ~BoundSessionRequestThrottledInRendererManager();

  void CallRemoteHandleRequestBlockedOnCookie();

  void OnRemoteDisconnected();
  void CancelAllDeferredRequests();
  void ResumeAllDeferredRequests();
  void RunAllCallbacks(UnblockAction action);

  std::vector<ResumeOrCancelThrottledRequestCallback>
      resume_or_cancel_deferred_request_callbacks_;
  mojo::Remote<chrome::mojom::BoundSessionRequestThrottledHandler> remote_;
  SEQUENCE_CHECKER(my_sequence_checker_);
};
#endif  // CHROME_RENDERER_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_IN_RENDERER_MANAGER_H_
