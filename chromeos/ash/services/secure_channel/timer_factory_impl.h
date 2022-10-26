// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_IMPL_H_

#include <memory>

#include "chromeos/ash/services/secure_channel/timer_factory.h"

namespace base {
class OneShotTimer;
}

namespace ash::secure_channel {

// Concrete TimerFactory implementation, which returns base::OneShotTimer
// objects.
class TimerFactoryImpl : public TimerFactory {
 public:
  class Factory {
   public:
    static std::unique_ptr<TimerFactory> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<TimerFactory> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  TimerFactoryImpl(const TimerFactoryImpl&) = delete;
  TimerFactoryImpl& operator=(const TimerFactoryImpl&) = delete;

  ~TimerFactoryImpl() override;

 private:
  TimerFactoryImpl();

  // TimerFactory:
  std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() override;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_IMPL_H_
