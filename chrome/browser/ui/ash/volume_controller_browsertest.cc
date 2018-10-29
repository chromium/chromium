// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/ash/volume_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "media/audio/sounds/sounds_manager.h"
#include "ui/base/accelerators/accelerator.h"

namespace {

class SoundsManagerTestImpl : public media::SoundsManager {
 public:
  SoundsManagerTestImpl()
      : is_sound_initialized_(chromeos::SOUND_COUNT),
        num_play_requests_(chromeos::SOUND_COUNT) {}

  ~SoundsManagerTestImpl() override = default;

  bool Initialize(SoundKey key, const base::StringPiece& /* data */) override {
    is_sound_initialized_[key] = true;
    return true;
  }

  bool Play(SoundKey key) override {
    ++num_play_requests_[key];
    return true;
  }

  bool Stop(SoundKey key) override { return true; }

  base::TimeDelta GetDuration(SoundKey /* key */) override {
    return base::TimeDelta();
  }

  bool is_sound_initialized(SoundKey key) const {
    return is_sound_initialized_[key];
  }

  int num_play_requests(SoundKey key) const { return num_play_requests_[key]; }

 private:
  std::vector<bool> is_sound_initialized_;
  std::vector<int> num_play_requests_;

  DISALLOW_COPY_AND_ASSIGN(SoundsManagerTestImpl);
};

class VolumeControllerTest : public InProcessBrowserTest {
 public:
  VolumeControllerTest() = default;
  ~VolumeControllerTest() override = default;

  void SetUpOnMainThread() override {
    volume_controller_ = std::make_unique<VolumeController>();
    audio_handler_ = chromeos::CrasAudioHandler::Get();
  }

 protected:
  chromeos::CrasAudioHandler* audio_handler_;  // Not owned.
  std::unique_ptr<VolumeController> volume_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VolumeControllerTest);
};

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpAndDown) {
  // Set initial value as 50%.
  const int kInitVolume = 50;
  audio_handler_->SetOutputVolumePercent(kInitVolume);

  EXPECT_EQ(audio_handler_->GetOutputVolumePercent(), kInitVolume);

  volume_controller_->VolumeUp();
  EXPECT_LT(kInitVolume, audio_handler_->GetOutputVolumePercent());
  volume_controller_->VolumeDown();
  EXPECT_EQ(kInitVolume, audio_handler_->GetOutputVolumePercent());
  volume_controller_->VolumeDown();
  EXPECT_GT(kInitVolume, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeDownToZero) {
  // Setting to very small volume.
  audio_handler_->SetOutputVolumePercent(1);

  volume_controller_->VolumeDown();
  EXPECT_EQ(0, audio_handler_->GetOutputVolumePercent());
  volume_controller_->VolumeDown();
  EXPECT_EQ(0, audio_handler_->GetOutputVolumePercent());
  volume_controller_->VolumeUp();
  EXPECT_LT(0, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpTo100) {
  // Setting to almost max.
  audio_handler_->SetOutputVolumePercent(99);

  volume_controller_->VolumeUp();
  EXPECT_EQ(100, audio_handler_->GetOutputVolumePercent());
  volume_controller_->VolumeUp();
  EXPECT_EQ(100, audio_handler_->GetOutputVolumePercent());
  volume_controller_->VolumeDown();
  EXPECT_GT(100, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, Mute) {
  ASSERT_FALSE(audio_handler_->IsOutputMuted());
  const int initial_volume = audio_handler_->GetOutputVolumePercent();

  // Toggling mute should work without changing the volume level.
  volume_controller_->VolumeMuteToggle();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(initial_volume, audio_handler_->GetOutputVolumePercent());
  volume_controller_->VolumeMuteToggle();
  EXPECT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(initial_volume, audio_handler_->GetOutputVolumePercent());

  // Volume up should un-mute and raise the volume.
  volume_controller_->VolumeMuteToggle();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());
  volume_controller_->VolumeUp();
  EXPECT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_LT(initial_volume, audio_handler_->GetOutputVolumePercent());

  // Volume down should un-mute and lower the volume.
  audio_handler_->SetOutputMute(true);
  volume_controller_->VolumeDown();
  EXPECT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(initial_volume, audio_handler_->GetOutputVolumePercent());

  // Volume down will mute if the level falls below the default mute volume (1).
  audio_handler_->SetOutputVolumePercent(2);
  audio_handler_->SetOutputMute(false);
  volume_controller_->VolumeDown();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());
  EXPECT_TRUE(audio_handler_->IsOutputVolumeBelowDefaultMuteLevel());

  // Volume up will un-mute and bring the level above the default mute volume.
  audio_handler_->SetOutputVolumePercent(0);
  audio_handler_->SetOutputMute(true);
  volume_controller_->VolumeUp();
  EXPECT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_FALSE(audio_handler_->IsOutputVolumeBelowDefaultMuteLevel());
}

class VolumeControllerSoundsTest : public VolumeControllerTest {
 public:
  VolumeControllerSoundsTest() = default;
  ~VolumeControllerSoundsTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    sounds_manager_ = new SoundsManagerTestImpl();
    media::SoundsManager::InitializeForTesting(sounds_manager_);
  }

  bool is_sound_initialized() const {
    return sounds_manager_->is_sound_initialized(chromeos::SOUND_VOLUME_ADJUST);
  }

  int num_play_requests() const {
    return sounds_manager_->num_play_requests(chromeos::SOUND_VOLUME_ADJUST);
  }

 private:
  SoundsManagerTestImpl* sounds_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VolumeControllerSoundsTest);
};

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsTest, Simple) {
  audio_handler_->SetOutputVolumePercent(50);

  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(false);
  volume_controller_->VolumeUp();
  volume_controller_->VolumeDown();
  EXPECT_EQ(0, num_play_requests());

  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(true);
  volume_controller_->VolumeUp();
  volume_controller_->VolumeDown();
  EXPECT_EQ(2, num_play_requests());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsTest, EdgeCases) {
  EXPECT_TRUE(is_sound_initialized());
  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // Check that sound is played on volume up and volume down.
  audio_handler_->SetOutputVolumePercent(50);
  volume_controller_->VolumeUp();
  EXPECT_EQ(1, num_play_requests());
  volume_controller_->VolumeDown();
  EXPECT_EQ(2, num_play_requests());

  audio_handler_->SetOutputVolumePercent(99);
  volume_controller_->VolumeUp();
  EXPECT_EQ(3, num_play_requests());

  audio_handler_->SetOutputVolumePercent(100);
  volume_controller_->VolumeUp();
  EXPECT_EQ(3, num_play_requests());

  // Check that sound isn't played when audio is muted. Note that the volume
  // will be un-muted if the volume up/down key is pressed and the resulting
  // level is above the default mute level (1), so press down with a low level.
  audio_handler_->SetOutputVolumePercent(1);
  audio_handler_->SetOutputMute(true);
  volume_controller_->VolumeDown();
  ASSERT_TRUE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(3, num_play_requests());

  // Check that audio is unmuted and sound is played.
  volume_controller_->VolumeUp();
  ASSERT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(4, num_play_requests());
}

class VolumeControllerSoundsDisabledTest : public VolumeControllerSoundsTest {
 public:
  VolumeControllerSoundsDisabledTest() = default;
  ~VolumeControllerSoundsDisabledTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    VolumeControllerSoundsTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kDisableVolumeAdjustSound);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VolumeControllerSoundsDisabledTest);
};

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsDisabledTest, VolumeAdjustSounds) {
  EXPECT_FALSE(is_sound_initialized());

  // Check that sound isn't played on volume up and volume down.
  audio_handler_->SetOutputVolumePercent(50);
  volume_controller_->VolumeUp();
  volume_controller_->VolumeDown();
  EXPECT_EQ(0, num_play_requests());
}

}  // namespace
