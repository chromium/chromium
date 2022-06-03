// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/timer_factory.h"

#include <memory>

namespace chromeos {

namespace sync_wifi {

TimerFactory::~TimerFactory() = default;

std::unique_ptr<base::OneShotTimer> TimerFactory::CreateOneShotTimer() {
  return std::make_unique<base::OneShotTimer>();
}

}  // namespace sync_wifi

}  // namespace chromeos
