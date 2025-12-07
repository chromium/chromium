// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings/screen_power_controller_aura.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace chromecast {

namespace {

constexpr base::TimeDelta kScreenOnOffDuration = base::Milliseconds(200);
// These delays are needed to ensure there are no visible artifacts due to the
// backlight turning on prior to the LCD fully initializing or vice-versa.
// TODO(b/161140301): Make this configurable for different products
// TODO(b/161268188): Remove these if the delays can be handled by the kernel
constexpr base::TimeDelta kDisplayPowerOnDelay = base::Milliseconds(35);
constexpr base::TimeDelta kDisplayPowerOffDelay = base::Milliseconds(85);

}  // namespace

std::unique_ptr<ScreenPowerController> ScreenPowerController::Create(
    Delegate* delegate) {
  return std::make_unique<ScreenPowerControllerAura>(delegate);
}

ScreenPowerControllerAura::ScreenPowerControllerAura(
    ScreenPowerController::Delegate* delegate)
    : screen_on_(true),
      screen_power_on_(true),
      allow_screen_power_off_(false),
      pending_task_(PendingTask::kNone),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK(delegate_);
}

ScreenPowerControllerAura::~ScreenPowerControllerAura() = default;

void ScreenPowerControllerAura::SetScreenOn() {
  if (pending_task_ != PendingTask::kNone) {
    pending_task_ = PendingTask::kOn;
    return;
  }

  if (!screen_on_) {
    pending_task_ = PendingTask::kOn;
    TriggerPendingTask();
  }
}

void ScreenPowerControllerAura::SetScreenOff() {
  if (pending_task_ != PendingTask::kNone) {
    if (!allow_screen_power_off_) {
      pending_task_ = PendingTask::kBrightnessOff;
    } else {
      pending_task_ = PendingTask::kPowerOff;
    }
    return;
  }

  if (!allow_screen_power_off_ && (screen_on_ || !screen_power_on_)) {
    pending_task_ = PendingTask::kBrightnessOff;
    TriggerPendingTask();
  } else if (allow_screen_power_off_ && screen_power_on_) {
    pending_task_ = PendingTask::kPowerOff;
    TriggerPendingTask();
  }
}

void ScreenPowerControllerAura::SetAllowScreenPowerOff(bool allow_power_off) {
  LOG(INFO) << "SetAllowScreenPowerOff set to "
            << (allow_power_off ? "true" : "false");
  allow_screen_power_off_ = allow_power_off;
  switch (pending_task_) {
    case PendingTask::kOn:
      return;
    case PendingTask::kBrightnessOff:
    case PendingTask::kPowerOff:
      if (!allow_screen_power_off_) {
        pending_task_ = PendingTask::kBrightnessOff;
      } else {
        pending_task_ = PendingTask::kPowerOff;
      }
      return;
    case PendingTask::kNone:
      if (!screen_on_) {
        // Set screen power off if screen is already off.
        SetScreenOff();
      }
      return;
  }
}

bool ScreenPowerControllerAura::IsScreenOn() const {
  return screen_on_;
}

void ScreenPowerControllerAura::TriggerPendingTask() {
  switch (pending_task_) {
    case PendingTask::kOn:
      if (screen_on_) {
        NOTREACHED();
      } else if (screen_power_on_) {
        SetScreenBrightnessOn(true);
        pending_task_ = PendingTask::kNone;
      } else {
        SetScreenPowerOn();
      }
      return;
    case PendingTask::kBrightnessOff:
      if (screen_on_) {
        SetScreenBrightnessOn(false);
        pending_task_ = PendingTask::kNone;
      } else if (screen_power_on_) {
        pending_task_ = PendingTask::kNone;
      } else {
        SetScreenPowerOn();
      }
      return;
    case PendingTask::kPowerOff:
      if (screen_on_) {
        SetScreenBrightnessOn(false);
        SetScreenPowerOff();
      } else if (screen_power_on_) {
        SetScreenPowerOff();
      } else {
        NOTREACHED();
      }
      return;
    case PendingTask::kNone:
      NOTREACHED();
  }
}

void ScreenPowerControllerAura::SetScreenBrightnessOn(bool brightness_on) {
  delegate_->SetScreenBrightnessOn(brightness_on, kScreenOnOffDuration);
  screen_on_ = brightness_on;
}

void ScreenPowerControllerAura::SetScreenPowerOn() {
  delegate_->SetScreenPowerOn(
      base::BindOnce(&ScreenPowerControllerAura::OnScreenPoweredOn,
                     weak_factory_.GetWeakPtr()));
}

void ScreenPowerControllerAura::SetScreenPowerOff() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScreenPowerControllerAura::OnDisplayOffTimeoutCompleted,
                     weak_factory_.GetWeakPtr()),
      kDisplayPowerOffDelay + kScreenOnOffDuration);
}

void ScreenPowerControllerAura::OnScreenPoweredOn(bool succeeded) {
  if (!succeeded) {
    // Fatal since the user has no other way of turning the screen on if this
    // failed.
    LOG(FATAL) << "Failed to power on the screen";
  }
  LOG(INFO) << "Screen is powered on";
  screen_power_on_ = true;

  switch (pending_task_) {
    case PendingTask::kOn:
    case PendingTask::kBrightnessOff:
      // TODO(b/161268188): This can be simplified and the delays removed if
      // backlight timing is handled by the kernel
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &ScreenPowerControllerAura::OnDisplayOnTimeoutCompleted,
              weak_factory_.GetWeakPtr()),
          kDisplayPowerOnDelay);
      return;
    case PendingTask::kPowerOff:
      TriggerPendingTask();
      return;
    case PendingTask::kNone:
      NOTREACHED();
  }
}

void ScreenPowerControllerAura::OnScreenPoweredOff(bool succeeded) {
  if (!succeeded) {
    LOG(ERROR) << "Failed to power off the screen";
    return;
  }
  LOG(INFO) << "Screen is powered off";
  screen_power_on_ = false;

  switch (pending_task_) {
    case PendingTask::kOn:
    case PendingTask::kBrightnessOff:
      TriggerPendingTask();
      return;
    case PendingTask::kPowerOff:
      pending_task_ = PendingTask::kNone;
      return;
    case PendingTask::kNone:
      NOTREACHED();
  }
}

void ScreenPowerControllerAura::OnDisplayOnTimeoutCompleted() {
  switch (pending_task_) {
    case PendingTask::kBrightnessOff:
      pending_task_ = PendingTask::kNone;
      return;
    case PendingTask::kOn:
    case PendingTask::kPowerOff:
      TriggerPendingTask();
      return;
    case PendingTask::kNone:
      NOTREACHED();
  }
}

void ScreenPowerControllerAura::OnDisplayOffTimeoutCompleted() {
  switch (pending_task_) {
    case PendingTask::kOn:
      TriggerPendingTask();
      return;
    case PendingTask::kBrightnessOff:
      pending_task_ = PendingTask::kNone;
      return;
    case PendingTask::kPowerOff:
      delegate_->SetScreenPowerOff(
          base::BindOnce(&ScreenPowerControllerAura::OnScreenPoweredOff,
                         weak_factory_.GetWeakPtr()));
      return;
    case PendingTask::kNone:
      NOTREACHED();
  }
}

}  // namespace chromecast
