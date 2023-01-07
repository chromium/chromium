// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/fake_one_shot_timer.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"

namespace ash::sync_wifi {

FakeOneShotTimer::FakeOneShotTimer(
    base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback)
    : destructor_callback_(std::move(destructor_callback)),
      id_(base::UnguessableToken::Create()) {}

FakeOneShotTimer::~FakeOneShotTimer() {
  std::move(destructor_callback_).Run(id_);
}

}  // namespace ash::sync_wifi
