// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_timer_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/services/secure_channel/fake_one_shot_timer.h"

namespace chromeos {

namespace secure_channel {

FakeTimerFactory::FakeTimerFactory() {}

FakeTimerFactory::~FakeTimerFactory() = default;

std::unique_ptr<base::OneShotTimer> FakeTimerFactory::CreateOneShotTimer() {
  ++num_instances_created_;

  auto fake_one_shot_timer = std::make_unique<FakeOneShotTimer>(
      base::BindOnce(&FakeTimerFactory::OnOneShotTimerDeleted,
                     weak_ptr_factory_.GetWeakPtr()));

  id_for_last_created_one_shot_timer_ = fake_one_shot_timer->id();
  id_to_active_one_shot_timer_map_[fake_one_shot_timer->id()] =
      fake_one_shot_timer.get();

  return fake_one_shot_timer;
}

void FakeTimerFactory::OnOneShotTimerDeleted(
    const base::UnguessableToken& deleted_timer_id) {
  size_t num_deleted = id_to_active_one_shot_timer_map_.erase(deleted_timer_id);
  DCHECK_EQ(1u, num_deleted);
}

}  // namespace secure_channel

}  // namespace chromeos
