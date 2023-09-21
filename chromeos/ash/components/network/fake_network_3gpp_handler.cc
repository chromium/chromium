// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/fake_network_3gpp_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/values.h"

namespace ash {

FakeNetwork3gppHandler::FakeNetwork3gppHandler() = default;

FakeNetwork3gppHandler::~FakeNetwork3gppHandler() = default;

void FakeNetwork3gppHandler::SetCarrierLock(
    const std::string& config,
    Modem3gppClient::CarrierLockCallback callback) {
  carrier_lock_callback_ = std::move(callback);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeNetwork3gppHandler::CompleteSetCarrierLock,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeNetwork3gppHandler::CompleteSetCarrierLock() {
  std::move(carrier_lock_callback_).Run(carrier_lock_result_);
}

}  // namespace ash
