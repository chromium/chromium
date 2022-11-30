// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_INHERITABLE_EVENT_H_
#define CHROME_CHROME_CLEANER_OS_INHERITABLE_EVENT_H_

#include <memory>

#include "base/synchronization/waitable_event.h"

namespace chrome_cleaner {

std::unique_ptr<base::WaitableEvent> CreateInheritableEvent(
    base::WaitableEvent::ResetPolicy reset_policy,
    base::WaitableEvent::InitialState initial_state);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_INHERITABLE_EVENT_H_
