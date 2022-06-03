// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/fake_timer_factory.h"

#include "base/unguessable_token.h"
#include "chromeos/components/sync_wifi/fake_one_shot_timer.h"

namespace chromeos {

namespace sync_wifi {

FakeTimerFactory::FakeTimerFactory() = default;

FakeTimerFactory::~FakeTimerFactory() = default;

std::unique_ptr<base::OneShotTimer> FakeTimerFactory::CreateOneShotTimer() {
  auto mock_timer = std::make_unique<FakeOneShotTimer>(
      base::BindOnce(&FakeTimerFactory::OnOneShotTimerDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
  id_to_timer_map_[mock_timer->id()] = mock_timer.get();
  return mock_timer;
}

void FakeTimerFactory::FireAll() {
  // Make a copy because firing a timer will usually destroy it.  This calls
  // OnOneShotTimerDeleted and removes it from |id_to_timer_map_|.
  auto id_to_timer_map_copy = id_to_timer_map_;
  for (auto it : id_to_timer_map_copy)
    it.second->Fire();
}

void FakeTimerFactory::OnOneShotTimerDeleted(
    const base::UnguessableToken& deleted_timer_id) {
  id_to_timer_map_.erase(deleted_timer_id);
}

}  // namespace sync_wifi

}  // namespace chromeos
