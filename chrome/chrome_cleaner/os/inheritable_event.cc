// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/inheritable_event.h"

#include <windows.h>
#include "base/logging.h"

namespace chrome_cleaner {

std::unique_ptr<base::WaitableEvent> CreateInheritableEvent(
    base::WaitableEvent::ResetPolicy reset_policy,
    base::WaitableEvent::InitialState initial_state) {
  SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES)};
  attributes.bInheritHandle = true;

  HANDLE handle = ::CreateEvent(
      &attributes, reset_policy == base::WaitableEvent::ResetPolicy::MANUAL,
      initial_state == base::WaitableEvent::InitialState::SIGNALED, nullptr);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
    PLOG(ERROR) << "Could not create inheritable event";
    return nullptr;
  }
  base::win::ScopedHandle event_handle(handle);
  return std::make_unique<base::WaitableEvent>(std::move(event_handle));
}

}  // namespace chrome_cleaner
