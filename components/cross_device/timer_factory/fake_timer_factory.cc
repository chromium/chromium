// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cross_device/timer_factory/fake_timer_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/cross_device/timer_factory/fake_one_shot_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cross_device {

FakeTimerFactory::FakeTimerFactory() = default;

FakeTimerFactory::~FakeTimerFactory() = default;

std::unique_ptr<cross_device::TimerFactory>
FakeTimerFactory::Factory::CreateInstance() {
  EXPECT_FALSE(instance_);
  auto instance = std::make_unique<FakeTimerFactory>();
  instance_ = instance.get();
  return instance;
}

std::unique_ptr<base::OneShotTimer> FakeTimerFactory::CreateOneShotTimer() {
  ++num_instances_created_;

  auto fake_one_shot_timer = std::make_unique<FakeOneShotTimer>(
      base::BindOnce(&FakeTimerFactory::OnOneShotTimerDeleted,
                     weak_ptr_factory_.GetWeakPtr()));

  id_for_last_created_one_shot_timer_ = fake_one_shot_timer->id();
  id_to_active_one_shot_timer_map_.insert_or_assign(fake_one_shot_timer->id(),
                                                    fake_one_shot_timer.get());
  return fake_one_shot_timer;
}

void FakeTimerFactory::FireAll() {
  // Make a copy because firing a timer will usually destroy it.  This calls
  // OnOneShotTimerDeleted and removes it from |id_to_timer_map_|.
  auto id_to_timer_map_copy = id_to_active_one_shot_timer_map_;
  for (auto [id, timer] : id_to_timer_map_copy) {
    timer->Fire();
  }
}

void FakeTimerFactory::OnOneShotTimerDeleted(
    const base::UnguessableToken& deleted_timer_id) {
  id_to_active_one_shot_timer_map_.erase(deleted_timer_id);
}

}  // namespace cross_device
