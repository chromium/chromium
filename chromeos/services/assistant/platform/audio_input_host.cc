// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_host.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/services/assistant/platform/audio_input_impl.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr const char kDefaultLocale[] = "en_us";

AudioInputImpl::LidState ConvertLidState(
    chromeos::PowerManagerClient::LidState state) {
  switch (state) {
    case chromeos::PowerManagerClient::LidState::CLOSED:
      return AudioInputImpl::LidState::kClosed;
    case chromeos::PowerManagerClient::LidState::OPEN:
      return AudioInputImpl::LidState::kOpen;
    case chromeos::PowerManagerClient::LidState::NOT_PRESENT:
      // If there is no lid, it can't be closed.
      return AudioInputImpl::LidState::kOpen;
  }
}

// Hotword model is expected to have <language>_<region> format with lower
// case, while the locale in pref is stored as <language>-<region> with region
// code in capital letters. So we need to convert the pref locale to the
// correct format.
// Examples:
//     "fr"     ->  "fr_fr"
//     "nl-BE"  ->  "nl_be"
base::Optional<std::string> ToHotwordModel(std::string pref_locale) {
  std::vector<std::string> code_strings = base::SplitString(
      pref_locale, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (code_strings.size() == 0) {
    // Note: I am not sure this happens during real operations, but it
    // definitely happens during the ChromeOS performance tests.
    return base::nullopt;
  }

  DCHECK_LT(code_strings.size(), 3u);

  // For locales with language code "en", use "en_all" hotword model.
  if (code_strings[0] == "en")
    return "en_all";

  // If the language code and country code happen to be the same, e.g.
  // France (FR) and French (fr), the locale will be stored as "fr" instead
  // of "fr-FR" in the profile on Chrome OS.
  if (code_strings.size() == 1)
    return code_strings[0] + "_" + code_strings[0];

  return code_strings[0] + "_" + base::ToLowerASCII(code_strings[1]);
}

}  // namespace

chromeos::assistant::AudioInputHost::AudioInputHost(
    AudioInputImpl* audio_input,
    CrasAudioHandler* cras_audio_handler,
    chromeos::PowerManagerClient* power_manager_client)
    : audio_input_(audio_input),
      cras_audio_handler_(cras_audio_handler),
      power_manager_client_(power_manager_client),
      power_manager_client_observer_(this) {
  DCHECK(audio_input_);
  DCHECK(cras_audio_handler_);
  DCHECK(power_manager_client_);

  power_manager_client_observer_.Observe(power_manager_client);
  power_manager_client->GetSwitchStates(base::BindOnce(
      &AudioInputHost::OnInitialLidStateReceived, weak_factory_.GetWeakPtr()));
}

AudioInputHost::~AudioInputHost() = default;

void AudioInputHost::SetMicState(bool mic_open) {
  audio_input_->SetMicState(mic_open);
}

void AudioInputHost::SetDeviceId(const std::string& device_id) {
  audio_input_->SetDeviceId(device_id);
}

void AudioInputHost::OnConversationTurnStarted() {
  audio_input_->OnConversationTurnStarted();
  // Inform power manager of a wake notification when Libassistant
  // recognized hotword and started a conversation. We intentionally
  // avoid using |NotifyUserActivity| because it is not suitable for
  // this case according to the Platform team.
  power_manager_client_->NotifyWakeNotification();
}

void AudioInputHost::OnConversationTurnFinished() {
  audio_input_->OnConversationTurnFinished();
}

void AudioInputHost::OnHotwordEnabled(bool enable) {
  audio_input_->OnHotwordEnabled(enable);
}

void AudioInputHost::SetHotwordDeviceId(const std::string& device_id) {
  hotword_device_id_ = device_id;
  audio_input_->SetHotwordDeviceId(device_id);
}

void AudioInputHost::SetDspHotwordLocale(std::string pref_locale) {
  if (!features::IsDspHotwordEnabled())
    return;

  std::string hotword_model =
      ToHotwordModel(pref_locale).value_or(kDefaultLocale);

  cras_audio_handler_->SetHotwordModel(
      GetDspNodeId(), hotword_model,
      base::BindOnce(&AudioInputHost::SetDspHotwordLocaleCallback,
                     weak_factory_.GetWeakPtr(), hotword_model));
}

void AudioInputHost::SetDspHotwordLocaleCallback(std::string pref_locale,
                                                 bool success) {
  base::UmaHistogramBoolean("Assistant.SetDspHotwordLocale", success);
  if (success)
    return;

  LOG(ERROR) << "Set " << pref_locale
             << " hotword model failed, fallback to default locale.";
  // Reset the locale to the default value if we failed to sync it to the locale
  // stored in user's pref.
  cras_audio_handler_->SetHotwordModel(
      GetDspNodeId(), /* hotword_model */ kDefaultLocale,
      base::BindOnce([](bool success) {
        if (!success)
          LOG(ERROR) << "Reset to default hotword model failed.";
      }));
}

uint64_t AudioInputHost::GetDspNodeId() const {
  DCHECK(!hotword_device_id_.empty());
  uint64_t result;
  bool success = base::StringToUint64(hotword_device_id_, &result);
  DCHECK(success) << "Invalid hotword device id '" << hotword_device_id_ << "'";
  return result;
}

void AudioInputHost::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  // Lid switch event still gets fired during system suspend, which enables
  // us to stop DSP recording correctly when user closes lid after the device
  // goes to sleep.
  audio_input_->OnLidStateChanged(ConvertLidState(state));
}

void AudioInputHost::OnInitialLidStateReceived(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value())
    audio_input_->OnLidStateChanged(ConvertLidState(switch_states->lid_state));
}

}  // namespace assistant
}  // namespace chromeos
