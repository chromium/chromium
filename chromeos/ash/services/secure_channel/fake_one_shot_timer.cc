// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_one_shot_timer.h"

#include "base/functional/callback.h"

namespace ash::secure_channel {

FakeOneShotTimer::FakeOneShotTimer(
    base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback)
    : base::MockOneShotTimer(),
      destructor_callback_(std::move(destructor_callback)),
      id_(base::UnguessableToken::Create()) {}

FakeOneShotTimer::~FakeOneShotTimer() {
  std::move(destructor_callback_).Run(id_);
}

}  // namespace ash::secure_channel
