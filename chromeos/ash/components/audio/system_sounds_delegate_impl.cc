// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/system_sounds_delegate_impl.h"

#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "services/audio/public/cpp/sounds/global_sounds_manager.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

SystemSoundsDelegateImpl::SystemSoundsDelegateImpl() = default;

SystemSoundsDelegateImpl::~SystemSoundsDelegateImpl() = default;

void SystemSoundsDelegateImpl::Init() {
  audio::SoundsManager& manager = audio::GlobalSoundsManager::Get();

  // Initialize sounds used for power and battery.
  manager.Initialize(static_cast<int>(ash::Sound::kChargeHighBattery),
                     IDR_SOUND_CHARGE_HIGH_BATTERY_FLAC,
                     media::AudioCodec::kFLAC, /*loop=*/false);
  manager.Initialize(static_cast<int>(ash::Sound::kChargeMediumBattery),
                     IDR_SOUND_CHARGE_MEDIUM_BATTERY_FLAC,
                     media::AudioCodec::kFLAC, /*loop=*/false);
  manager.Initialize(static_cast<int>(ash::Sound::kChargeLowBattery),
                     IDR_SOUND_CHARGE_LOW_BATTERY_FLAC,
                     media::AudioCodec::kFLAC, /*loop=*/false);
  manager.Initialize(static_cast<int>(ash::Sound::kNoChargeLowBattery),
                     IDR_SOUND_NO_CHARGE_LOW_BATTERY_FLAC,
                     media::AudioCodec::kFLAC, /*loop=*/false);

  // Initialize sounds used for Focus mode.
  manager.Initialize(static_cast<int>(ash::Sound::kFocusModeEndingMoment),
                     IDR_SOUND_FOCUS_MODE_ENDING_MOMENT_FLAC,
                     media::AudioCodec::kFLAC, /*loop=*/false);
}

void SystemSoundsDelegateImpl::Play(ash::Sound sound_key) {
  audio::GlobalSoundsManager::Get().Play(static_cast<int>(sound_key));
}
