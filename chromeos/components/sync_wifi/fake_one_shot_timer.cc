// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/fake_one_shot_timer.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"

namespace chromeos {

namespace sync_wifi {

FakeOneShotTimer::FakeOneShotTimer(
    base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback)
    : destructor_callback_(std::move(destructor_callback)),
      id_(base::UnguessableToken::Create()) {}

FakeOneShotTimer::~FakeOneShotTimer() {
  std::move(destructor_callback_).Run(id_);
}

}  // namespace sync_wifi

}  // namespace chromeos
