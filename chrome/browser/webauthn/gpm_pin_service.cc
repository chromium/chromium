// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_pin_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace webauthn {

GpmPinService::GpmPinService() = default;

GpmPinService::~GpmPinService() = default;

void GpmPinService::SyncPinState(const CoreAccountInfo& account,
                                 PinStateCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void GpmPinService::VerifyPin(const CoreAccountInfo& account,
                              GpmPin pin,
                              VerifyPinCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), VerifyPinResult::kError,
                                /*remaining_attempts=*/0));
}

void GpmPinService::SetPin(const CoreAccountInfo& account,
                           GpmPin new_pin,
                           SetPinCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*success=*/false));
}

}  // namespace webauthn
