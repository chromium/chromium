// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/timer_factory.h"

#include <memory>

namespace ash::sync_wifi {

TimerFactory::~TimerFactory() = default;

std::unique_ptr<base::OneShotTimer> TimerFactory::CreateOneShotTimer() {
  return std::make_unique<base::OneShotTimer>();
}

}  // namespace ash::sync_wifi
