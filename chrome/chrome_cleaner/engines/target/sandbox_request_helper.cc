// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/sandbox_request_helper.h"

#include <utility>

namespace chrome_cleaner {

// static
MojoCallStatus MojoCallStatus::Success() {
  return MojoCallStatus{MOJO_CALL_MADE};
}

// static
MojoCallStatus MojoCallStatus::Failure(SandboxErrorCode error_code) {
  return MojoCallStatus{MOJO_CALL_ERROR, error_code};
}

namespace internal {

void SendAsyncMojoRequest(base::OnceCallback<MojoCallStatus()> request,
                          base::WaitableEvent* async_call_done_event,
                          MojoCallStatus* status_out) {
  *status_out = std::move(request).Run();

  // If the async Mojo call was made successfully, the Mojo response callback
  // will signal the event when the response is received. Otherwise signal it
  // now because the Mojo response callback will never be invoked.
  if (status_out->state != MojoCallStatus::MOJO_CALL_MADE)
    async_call_done_event->Signal();
}

}  // namespace internal

}  // namespace chrome_cleaner
