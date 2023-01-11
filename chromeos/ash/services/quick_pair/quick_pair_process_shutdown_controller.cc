// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_process_shutdown_controller.h"

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace quick_pair {

constexpr base::TimeDelta kProcessShutdownTimeout = base::Seconds(5);

QuickPairProcessShutdownController::QuickPairProcessShutdownController() =
    default;

QuickPairProcessShutdownController::~QuickPairProcessShutdownController() =
    default;

void QuickPairProcessShutdownController::Start(base::OnceClosure callback) {
  timer_.Start(FROM_HERE, kProcessShutdownTimeout, std::move(callback));
}

void QuickPairProcessShutdownController::Stop() {
  timer_.Stop();
}

}  // namespace quick_pair
}  // namespace ash
