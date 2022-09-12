// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_TIMER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_TIMER_FACTORY_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/sync_wifi/fake_one_shot_timer.h"
#include "chromeos/ash/components/sync_wifi/timer_factory.h"

namespace base {
class OneShotTimer;
class UnguessableToken;
}  // namespace base

namespace ash::sync_wifi {

class FakeTimerFactory : public TimerFactory {
 public:
  FakeTimerFactory();

  FakeTimerFactory(const FakeTimerFactory&) = delete;
  FakeTimerFactory& operator=(const FakeTimerFactory&) = delete;

  ~FakeTimerFactory() override;

  // TimerFactory:
  std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() override;

  void FireAll();

 private:
  void OnOneShotTimerDeleted(const base::UnguessableToken& deleted_timer_id);

  base::flat_map<base::UnguessableToken, FakeOneShotTimer*> id_to_timer_map_;
  base::WeakPtrFactory<FakeTimerFactory> weak_ptr_factory_{this};
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_FAKE_TIMER_FACTORY_H_
