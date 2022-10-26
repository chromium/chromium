// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_TIMER_FACTORY_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_TIMER_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/secure_channel/timer_factory.h"

namespace ash::secure_channel {

class FakeOneShotTimer;

// Test TimerFactory implementation, which returns FakeOneShotTimer objects.
class FakeTimerFactory : public TimerFactory {
 public:
  FakeTimerFactory();

  FakeTimerFactory(const FakeTimerFactory&) = delete;
  FakeTimerFactory& operator=(const FakeTimerFactory&) = delete;

  ~FakeTimerFactory() override;

  const base::UnguessableToken& id_for_last_created_one_shot_timer() {
    return id_for_last_created_one_shot_timer_;
  }

  base::flat_map<base::UnguessableToken, FakeOneShotTimer*>&
  id_to_active_one_shot_timer_map() {
    return id_to_active_one_shot_timer_map_;
  }

  size_t num_instances_created() const { return num_instances_created_; }

 private:
  // TimerFactory:
  std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() override;

  void OnOneShotTimerDeleted(const base::UnguessableToken& deleted_timer_id);

  base::UnguessableToken id_for_last_created_one_shot_timer_;
  base::flat_map<base::UnguessableToken, FakeOneShotTimer*>
      id_to_active_one_shot_timer_map_;
  size_t num_instances_created_ = 0u;

  base::WeakPtrFactory<FakeTimerFactory> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_TIMER_FACTORY_H_
