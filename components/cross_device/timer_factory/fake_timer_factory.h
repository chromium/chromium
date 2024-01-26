// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CROSS_DEVICE_TIMER_FACTORY_FAKE_TIMER_FACTORY_H_
#define COMPONENTS_CROSS_DEVICE_TIMER_FACTORY_FAKE_TIMER_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/cross_device/timer_factory/timer_factory_impl.h"

namespace cross_device {

class FakeOneShotTimer;

// Test TimerFactory implementation, which returns FakeOneShotTimer objects.
class FakeTimerFactory : public cross_device::TimerFactory {
 public:
  class Factory : public TimerFactoryImpl::Factory {
   public:
    Factory() = default;

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    ~Factory() override = default;

    FakeTimerFactory* instance() { return instance_.get(); }

   private:
    // TimerFactoryImpl::Factory:
    std::unique_ptr<cross_device::TimerFactory> CreateInstance() override;

    raw_ptr<FakeTimerFactory> instance_ = nullptr;
  };

  FakeTimerFactory();

  FakeTimerFactory(const FakeTimerFactory&) = delete;
  FakeTimerFactory& operator=(const FakeTimerFactory&) = delete;

  ~FakeTimerFactory() override;

  const base::UnguessableToken& id_for_last_created_one_shot_timer() {
    return id_for_last_created_one_shot_timer_;
  }

  FakeOneShotTimer* last_created_one_shot_timer() {
    if (id_to_active_one_shot_timer_map_.contains(
            id_for_last_created_one_shot_timer_)) {
      return id_to_active_one_shot_timer_map_.at(
          id_for_last_created_one_shot_timer_);
    }
    return nullptr;
  }

  void ClearTimerById(const base::UnguessableToken& id) {
    id_to_active_one_shot_timer_map_.erase(id);
  }

  base::flat_map<base::UnguessableToken, FakeOneShotTimer*>&
  id_to_active_one_shot_timer_map() {
    return id_to_active_one_shot_timer_map_;
  }

  size_t num_instances_created() const { return num_instances_created_; }

  void FireAll();

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

}  // namespace cross_device

#endif  // COMPONENTS_CROSS_DEVICE_TIMER_FACTORY_FAKE_TIMER_FACTORY_H_
