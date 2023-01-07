// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TEST_TIMER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TEST_TIMER_FACTORY_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/tether/timer_factory.h"

namespace ash {
namespace tether {

class TestTimerFactory : public TimerFactory {
 public:
  TestTimerFactory();
  ~TestTimerFactory() override;

  // TimerFactory:
  std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() override;

  void set_device_id_for_next_timer(
      const std::string& device_id_for_next_timer) {
    device_id_for_next_timer_ = device_id_for_next_timer;
  }

  base::MockOneShotTimer* GetTimerForDeviceId(const std::string& device_id) {
    return device_id_to_timer_map_[device_id_for_next_timer_];
  }

  void ClearTimerForDeviceId(const std::string& device_id) {
    device_id_to_timer_map_.erase(device_id_for_next_timer_);
  }

 private:
  std::string device_id_for_next_timer_;
  base::flat_map<std::string, base::MockOneShotTimer*> device_id_to_timer_map_;
};

}  // namespace tether
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TEST_TIMER_FACTORY_H_
