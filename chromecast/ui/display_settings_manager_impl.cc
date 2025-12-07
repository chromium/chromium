// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings_manager_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/ui/display_settings/brightness_animation.h"
#include "chromecast/ui/display_settings/color_temperature_animation.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

#if defined(USE_AURA)
#include "chromecast/browser/cast_display_configurator.h"  // nogncheck
#include "chromecast/ui/display_settings/gamma_configurator.h"
#endif  // defined(USE_AURA)

namespace chromecast {

namespace {

constexpr base::TimeDelta kAnimationDuration = base::Seconds(2);

const float kMinApiBrightness = 0.0f;
const float kMaxApiBrightness = 1.0f;
const float kDefaultApiBrightness = kMaxApiBrightness;

#if defined(USE_AURA)
// A wrapper function for converting a PowerToggleCallback to a
// display::ConfigureCallback. Calls `callback` with the resulting status.
void AsConfigureCallback(
    ScreenPowerController::Delegate::PowerToggleCallback callback,
    const std::vector<display::DisplayConfigurationParams>& request_results,
    bool status) {
  std::move(callback).Run(status);
}
#endif  // defined(USE_AURA)

}  // namespace

DisplaySettingsManagerImpl::DisplaySettingsManagerImpl(
    CastWindowManager* window_manager,
    shell::CastDisplayConfigurator* display_configurator)
    : window_manager_(window_manager),
      display_configurator_(display_configurator),
#if defined(USE_AURA)
      gamma_configurator_(
          std::make_unique<GammaConfigurator>(display_configurator_)),
#endif  // defined(USE_AURA)
      brightness_(-1.0f),
      screen_power_controller_(ScreenPowerController::Create(this)),
      weak_factory_(this) {
  DCHECK(window_manager_);
#if defined(USE_AURA)
  DCHECK(display_configurator_);
#endif  // defined(USE_AURA)
}

DisplaySettingsManagerImpl::~DisplaySettingsManagerImpl() = default;

void DisplaySettingsManagerImpl::SetDelegate(
    DisplaySettingsManager::Delegate* delegate) {
  brightness_animation_ = std::make_unique<BrightnessAnimation>(delegate);
}

void DisplaySettingsManagerImpl::ResetDelegate() {
  // Skip a brightess animation to its last step and stop
  // This is important for the final brightness to be cached on reboot
  brightness_animation_.reset();
}

void DisplaySettingsManagerImpl::SetColorTemperatureConfig(
    const DisplaySettingsManager::ColorTemperatureConfig& config) {
  DCHECK(!color_temperature_animation_);
  color_temperature_animation_ = std::make_unique<ColorTemperatureAnimation>(
      display_configurator_, config);
}

void DisplaySettingsManagerImpl::SetGammaCalibration(
    const std::vector<display::GammaRampRGBEntry>& gamma) {
#if defined(USE_AURA)
  gamma_configurator_->OnCalibratedGammaLoaded(gamma);
#endif  // defined(USE_AURA)
}

void DisplaySettingsManagerImpl::NotifyBrightnessChanged(float new_brightness,
                                                         float old_brightness) {
  for (auto& observer : observers_)
    observer->OnDisplayBrightnessChanged(new_brightness);
}

void DisplaySettingsManagerImpl::SetColorInversion(bool enable) {
#if defined(USE_AURA)
  gamma_configurator_->SetColorInversion(enable);
#endif  // defined(USE_AURA)
}

void DisplaySettingsManagerImpl::AddReceiver(
    mojo::PendingReceiver<mojom::DisplaySettings> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DisplaySettingsManagerImpl::SetScreenPowerOn(PowerToggleCallback callback) {
#if defined(USE_AURA)
  display_configurator_->EnableDisplay(
      base::BindOnce(&AsConfigureCallback, std::move(callback)));
#endif  // defined(USE_AURA)
}

void DisplaySettingsManagerImpl::SetScreenPowerOff(PowerToggleCallback callback) {
#if defined(USE_AURA)
  display_configurator_->DisableDisplay(
      base::BindOnce(&AsConfigureCallback, std::move(callback)));
#endif  // defined(USE_AURA)
}

void DisplaySettingsManagerImpl::SetScreenBrightnessOn(
    bool brightness_on,
    base::TimeDelta duration) {
  UpdateBrightness(brightness_on ? brightness_ : 0, duration);
  window_manager_->SetTouchInputDisabled(!brightness_on);
}

void DisplaySettingsManagerImpl::SetColorTemperature(float temperature) {
  DVLOG(4) << "Setting color temperature to " << temperature << " Kelvin.";
  if (!color_temperature_animation_) {
    return;
  }
  color_temperature_animation_->AnimateToNewValue(temperature,
                                                  kAnimationDuration);
}

void DisplaySettingsManagerImpl::SetColorTemperatureSmooth(
    float temperature,
    base::TimeDelta duration) {
  DVLOG(4) << "Setting color temperature to " << temperature
           << " Kelvin. Duration: " << duration;
  if (!color_temperature_animation_) {
    return;
  }
  color_temperature_animation_->AnimateToNewValue(temperature, duration);
}

void DisplaySettingsManagerImpl::ResetColorTemperature() {
  if (!color_temperature_animation_) {
    return;
  }
  color_temperature_animation_->AnimateToNeutral(kAnimationDuration);
}

void DisplaySettingsManagerImpl::SetBrightness(float brightness) {
  SetBrightnessSmooth(brightness, base::Seconds(0));
}

void DisplaySettingsManagerImpl::SetBrightnessSmooth(float brightness,
                                                     base::TimeDelta duration) {
  if (brightness < kMinApiBrightness) {
    LOG(ERROR) << "brightness " << brightness
               << " is less than minimum brightness " << kMinApiBrightness
               << ".";
    return;
  }

  if (brightness > kMaxApiBrightness) {
    LOG(ERROR) << "brightness " << brightness
               << " is greater than maximum brightness " << kMaxApiBrightness
               << ".";
    return;
  }

  brightness_ = brightness;

  // If the screen is off, keep the new brightness but don't apply it
  if (!screen_power_controller_->IsScreenOn()) {
    return;
  }

  UpdateBrightness(brightness_, duration);
}

void DisplaySettingsManagerImpl::ResetBrightness() {
  SetBrightness(kDefaultApiBrightness);
}

void DisplaySettingsManagerImpl::SetScreenOn(bool screen_on) {
  if (screen_on) {
    screen_power_controller_->SetScreenOn();
  } else {
    screen_power_controller_->SetScreenOff();
  }
}

void DisplaySettingsManagerImpl::SetAllowScreenPowerOff(bool allow_power_off) {
  screen_power_controller_->SetAllowScreenPowerOff(allow_power_off);
}

void DisplaySettingsManagerImpl::AddDisplaySettingsObserver(
    mojo::PendingRemote<mojom::DisplaySettingsObserver> observer) {
  mojo::Remote<mojom::DisplaySettingsObserver> observer_remote(
      std::move(observer));
  observers_.Add(std::move(observer_remote));
}

void DisplaySettingsManagerImpl::UpdateBrightness(float brightness,
                                                  base::TimeDelta duration) {
  if (brightness_animation_)
    brightness_animation_->AnimateToNewValue(brightness, duration);
}

}  // namespace chromecast
