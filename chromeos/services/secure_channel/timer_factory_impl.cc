// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/timer_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"

namespace chromeos {

namespace secure_channel {

// static
TimerFactoryImpl::Factory* TimerFactoryImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<TimerFactory> TimerFactoryImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new TimerFactoryImpl());
}

// static
void TimerFactoryImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

TimerFactoryImpl::Factory::~Factory() = default;

TimerFactoryImpl::TimerFactoryImpl() = default;

TimerFactoryImpl::~TimerFactoryImpl() = default;

std::unique_ptr<base::OneShotTimer> TimerFactoryImpl::CreateOneShotTimer() {
  return std::make_unique<base::OneShotTimer>();
}

}  // namespace secure_channel

}  // namespace chromeos
