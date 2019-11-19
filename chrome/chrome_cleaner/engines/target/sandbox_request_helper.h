// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_REQUEST_HELPER_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_REQUEST_HELPER_H_

#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "chrome/chrome_cleaner/engines/common/sandbox_error_code.h"

namespace chrome_cleaner {

// A basic struct to return the state of the Mojo request, including any
// specific error messages.
struct MojoCallStatus {
  enum State {
    MOJO_CALL_MADE,
    MOJO_CALL_ERROR,
  } state;

  SandboxErrorCode error_code;

  static MojoCallStatus Success();

  static MojoCallStatus Failure(SandboxErrorCode error_code);
};

// These functions are included in the header so SyncSandboxRequest can access
// them, they shouldn't be used by anyone else.
namespace internal {

using MojoCallStatusCallback = base::OnceCallback<MojoCallStatus()>;

// calls |request|, which should send an async mojo call, and copies its return
// value to |status_out|. this function is executed on the mojo io thread.
//
// the comment in syncsandboxrequest explains when |event| is signaled.
void SendAsyncMojoRequest(MojoCallStatusCallback request,
                          base::WaitableEvent* async_call_done_event,
                          MojoCallStatus* status_out);

}  // namespace internal

// This template function is inlined in the header because otherwise we would
// have to introduce a templated class, and then the callsites would no longer
// be able to implicitly call the right function, but would have to explicitly
// create the class.
template <typename ProxyType,
          typename RequestCallback,
          typename ResponseCallback>
MojoCallStatus SyncSandboxRequest(ProxyType* proxy,
                                  RequestCallback request,
                                  ResponseCallback response) {
  if (!proxy) {
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  // This function is executed on an arbitrary thread which may be controlled
  // by a third-party engine. It blocks the current thread until a
  // WaitableEvent is signaled, as follows:
  //
  // 1. If an error occurs that prevents the asynchronous Mojo call from being
  // made, SendAsyncMojoRequest will signal the event. The response callback
  // will not be called.
  //
  // 2. Otherwise as soon as the asynchronous Mojo call is made,
  // SendAsyncMojoRequest will return MOJO_CALL_MADE but not signal the event.
  //
  // 3. When the asynchronous Mojo call returns a response, the response
  // callback will be called and will signal the event.
  //
  // The event can be bound using base::Unretained since it will not go out of
  // scope until it is signaled.
  base::WaitableEvent async_call_done_event;

  // |response| should be a Mojo response callback plus an extra event
  // parameter. Binding |event| to the callback gives a callback with the
  // correct signature to pass to |request|.
  auto response_callback = base::BindOnce(
      std::move(response), base::Unretained(&async_call_done_event));

  // |request| should wrap an async Mojo request that calls |response_callback|
  // with the response.
  internal::MojoCallStatusCallback request_callback =
      base::BindOnce(std::move(request), std::move(response_callback));

  // Invoke the asynchronous Mojo call on the Mojo IO thread.
  MojoCallStatus call_status;
  proxy->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&internal::SendAsyncMojoRequest,
                                std::move(request_callback),
                                base::Unretained(&async_call_done_event),
                                base::Unretained(&call_status)));
  async_call_done_event.Wait();
  return call_status;
}

template <typename ProxyType,
          typename RequestCallback,
          typename ResponseCallback>
MojoCallStatus SyncSandboxRequest(scoped_refptr<ProxyType> proxy,
                                  RequestCallback request,
                                  ResponseCallback response) {
  return SyncSandboxRequest(proxy.get(), std::move(request),
                            std::move(response));
}

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_REQUEST_HELPER_H_
