// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/timer_factory/timer_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"

namespace ash::timer_factory {

// static
std::unique_ptr<TimerFactoryImpl::Factory>
    TimerFactoryImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<TimerFactory> TimerFactoryImpl::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }

  return base::WrapUnique(new TimerFactoryImpl());
}

// static
void TimerFactoryImpl::Factory::SetFactoryForTesting(
    std::unique_ptr<Factory> test_factory) {
  test_factory_ = std::move(test_factory);
}

TimerFactoryImpl::Factory::~Factory() = default;

TimerFactoryImpl::TimerFactoryImpl() = default;

TimerFactoryImpl::~TimerFactoryImpl() = default;

std::unique_ptr<base::OneShotTimer> TimerFactoryImpl::CreateOneShotTimer() {
  return std::make_unique<base::OneShotTimer>();
}

}  // namespace ash::timer_factory
