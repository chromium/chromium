// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TEST_TEST_INHERITABLE_EVENT_H_
#define CHROME_UPDATER_WIN_TEST_TEST_INHERITABLE_EVENT_H_

#include <memory>

#include "base/synchronization/waitable_event.h"

namespace updater {

std::unique_ptr<base::WaitableEvent> CreateInheritableEvent(
    base::WaitableEvent::ResetPolicy reset_policy,
    base::WaitableEvent::InitialState initial_state);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TEST_TEST_INHERITABLE_EVENT_H_
