// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/volume_controller.h"

#include "ash/public/interfaces/accelerator_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "media/audio/sounds/sounds_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Percent by which the volume should be changed when a volume key is pressed.
const double kStepPercentage = 4.0;

bool VolumeAdjustSoundEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableVolumeAdjustSound);
}

void PlayVolumeAdjustSound() {
  if (VolumeAdjustSoundEnabled()) {
    chromeos::AccessibilityManager::Get()->PlayEarcon(
        chromeos::SOUND_VOLUME_ADJUST,
        chromeos::PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED);
  }
}

}  // namespace

VolumeController::VolumeController() : binding_(this) {
  // Connect to the accelerator controller interface in the ash service.
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  ash::mojom::AcceleratorControllerPtr accelerator_controller_ptr;
  connector->BindInterface(ash::mojom::kServiceName,
                           &accelerator_controller_ptr);

  // Register this object as the volume controller.
  ash::mojom::VolumeControllerPtr controller;
  binding_.Bind(mojo::MakeRequest(&controller));
  accelerator_controller_ptr->SetVolumeController(std::move(controller));

  if (VolumeAdjustSoundEnabled()) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    media::SoundsManager::Get()->Initialize(
        chromeos::SOUND_VOLUME_ADJUST,
        bundle.GetRawDataResource(IDR_SOUND_VOLUME_ADJUST_WAV));
  }
}

VolumeController::~VolumeController() = default;

void VolumeController::VolumeMuteToggle() {
  chromeos::CrasAudioHandler* audio = chromeos::CrasAudioHandler::Get();
  audio->SetOutputMute(!audio->IsOutputMuted());
}

void VolumeController::VolumeDown() {
  chromeos::CrasAudioHandler* audio = chromeos::CrasAudioHandler::Get();
  audio->AdjustOutputVolumeByPercent(-kStepPercentage);
  audio->SetOutputMute(audio->IsOutputVolumeBelowDefaultMuteLevel());
  if (!audio->IsOutputMuted())
    PlayVolumeAdjustSound();
}

void VolumeController::VolumeUp() {
  chromeos::CrasAudioHandler* audio = chromeos::CrasAudioHandler::Get();
  const bool play_sound = audio->GetOutputVolumePercent() != 100;
  audio->AdjustOutputVolumeByPercent(kStepPercentage);
  if (audio->IsOutputVolumeBelowDefaultMuteLevel())
    audio->AdjustOutputVolumeToAudibleLevel();
  audio->SetOutputMute(false);
  if (play_sound)
    PlayVolumeAdjustSound();
}
