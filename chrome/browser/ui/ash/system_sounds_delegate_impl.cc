// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_sounds_delegate_impl.h"

#include "base/check.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "ui/base/resource/resource_bundle.h"

SystemSoundsDelegateImpl::SystemSoundsDelegateImpl() = default;

SystemSoundsDelegateImpl::~SystemSoundsDelegateImpl() = default;

void SystemSoundsDelegateImpl::Init() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  audio::SoundsManager* manager = audio::SoundsManager::Get();

  // Initialize sounds used for power and battery.
  manager->Initialize(
      static_cast<int>(ash::Sound::kChargeHighBattery),
      bundle.GetRawDataResource(IDR_SOUND_CHARGE_HIGH_BATTERY_WAV));
  manager->Initialize(
      static_cast<int>(ash::Sound::kChargeMediumBattery),
      bundle.GetRawDataResource(IDR_SOUND_CHARGE_MEDIUM_BATTERY_WAV));
  manager->Initialize(
      static_cast<int>(ash::Sound::kChargeLowBattery),
      bundle.GetRawDataResource(IDR_SOUND_CHARGE_LOW_BATTERY_WAV));
  manager->Initialize(
      static_cast<int>(ash::Sound::kNoChargeLowBattery),
      bundle.GetRawDataResource(IDR_SOUND_NO_CHARGE_LOW_BATTERY_WAV));
}

void SystemSoundsDelegateImpl::Play(ash::Sound sound_key) {
  audio::SoundsManager::Get()->Play(static_cast<int>(sound_key));
}
