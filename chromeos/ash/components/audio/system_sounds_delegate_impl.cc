// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/system_sounds_delegate_impl.h"

#include "chromeos/ash/components/audio/public/cpp/sounds/sounds_manager.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"

SystemSoundsDelegateImpl::SystemSoundsDelegateImpl() = default;

SystemSoundsDelegateImpl::~SystemSoundsDelegateImpl() = default;

void SystemSoundsDelegateImpl::Init() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  audio::SoundsManager* manager = audio::SoundsManager::Get();

  // Initialize sounds used for power and battery.
  manager->Initialize(
      static_cast<int>(ash::Sound::kChargeHighBattery),
      bundle.GetRawDataResource(IDR_SOUND_CHARGE_HIGH_BATTERY_FLAC),
      media::AudioCodec::kFLAC);
  manager->Initialize(
      static_cast<int>(ash::Sound::kChargeMediumBattery),
      bundle.GetRawDataResource(IDR_SOUND_CHARGE_MEDIUM_BATTERY_FLAC),
      media::AudioCodec::kFLAC);
  manager->Initialize(
      static_cast<int>(ash::Sound::kChargeLowBattery),
      bundle.GetRawDataResource(IDR_SOUND_CHARGE_LOW_BATTERY_FLAC),
      media::AudioCodec::kFLAC);
  manager->Initialize(
      static_cast<int>(ash::Sound::kNoChargeLowBattery),
      bundle.GetRawDataResource(IDR_SOUND_NO_CHARGE_LOW_BATTERY_FLAC),
      media::AudioCodec::kFLAC);

  // Initialize sounds used for Focus mode.
  manager->Initialize(
      static_cast<int>(ash::Sound::kFocusModeEndingMoment),
      bundle.GetRawDataResource(IDR_SOUND_FOCUS_MODE_ENDING_MOMENT_FLAC),
      media::AudioCodec::kFLAC);
}

void SystemSoundsDelegateImpl::Play(ash::Sound sound_key) {
  audio::SoundsManager::Get()->Play(static_cast<int>(sound_key));
}
