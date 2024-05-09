// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_controller.h"

#include "base/no_destructor.h"

namespace chromeos {

// static
MagicBoostController* MagicBoostController::Get() {
  static base::NoDestructor<MagicBoostController> instance;
  return instance.get();
}

MagicBoostController::MagicBoostController() = default;

MagicBoostController::~MagicBoostController() = default;

}  // namespace chromeos
