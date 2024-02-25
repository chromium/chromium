// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/platform/audio_devices.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"

namespace ash::assistant {

namespace {

constexpr const char kDefaultLocale[] = "en_us";

// Hotword model is expected to have <language>_<region> format with lower
// case, while the locale in pref is stored as <language>-<region> with region
// code in capital letters. So we need to convert the pref locale to the
// correct format.
// Examples:
//     "fr"     ->  "fr_fr"
//     "nl-BE"  ->  "nl_be"
std::optional<std::string> ToHotwordModel(std::string pref_locale) {
  std::vector<std::string> code_strings = base::SplitString(
      pref_locale, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (code_strings.size() == 0) {
    // Note: I am not sure this happens during real operations, but it
    // definitely happens during the ChromeOS performance tests.
    return std::nullopt;
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

const AudioDevice* GetHighestPriorityDevice(const AudioDevice* left,
                                            const AudioDevice* right) {
  if (!left)
    return right;
  if (!right)
    return left;
  return left->priority < right->priority ? right : left;
}

std::optional<uint64_t> IdToOptional(const AudioDevice* device) {
  if (!device)
    return std::nullopt;
  return device->id;
}

std::optional<uint64_t> GetHotwordDeviceId(const AudioDeviceList& devices) {
  const AudioDevice* result = nullptr;

  for (const AudioDevice& device : devices) {
    if (!device.is_input)
      continue;

    switch (device.type) {
      case AudioDeviceType::kHotword:
        result = GetHighestPriorityDevice(result, &device);
        break;
      default:
        // ignore other devices
        break;
    }
  }

  return IdToOptional(result);
}

std::optional<uint64_t> GetPreferredDeviceId(const AudioDeviceList& devices) {
  const AudioDevice* result = nullptr;

  for (const AudioDevice& device : devices) {
    if (!device.is_input)
      continue;

    switch (device.type) {
      case AudioDeviceType::kMic:
      case AudioDeviceType::kUsb:
      case AudioDeviceType::kHeadphone:
      case AudioDeviceType::kInternalMic:
      case AudioDeviceType::kFrontMic:
      case AudioDeviceType::kRearMic:
      case AudioDeviceType::kKeyboardMic:
        result = GetHighestPriorityDevice(result, &device);
        break;
      default:
        // Ignore other devices. Note that we ignore bluetooth devices due to
        // battery consumption concerns.
        break;
    }
  }

  return IdToOptional(result);
}

std::optional<std::string> ToString(std::optional<uint64_t> int_value) {
  if (!int_value)
    return std::nullopt;
  return base::NumberToString(int_value.value());
}

}  // namespace

// Observer that will report all changes to the audio devices.
// It will unsubscribe from |CrasAudioHandler| in its destructor.
class AudioDevices::ScopedCrasAudioHandlerObserver
    : private CrasAudioHandler::AudioObserver {
 public:
  ScopedCrasAudioHandlerObserver(CrasAudioHandler* cras_audio_handler,
                                 AudioDevices* parent)
      : parent_(parent), cras_audio_handler_(cras_audio_handler) {}
  ScopedCrasAudioHandlerObserver(const ScopedCrasAudioHandlerObserver&) =
      delete;
  ScopedCrasAudioHandlerObserver& operator=(
      const ScopedCrasAudioHandlerObserver&) = delete;
  ~ScopedCrasAudioHandlerObserver() override = default;

  // Start the observer, which means it will
  //    - Subscribe for changes
  //    - Fetch the current state.
  void StartObserving() {
    scoped_observer_.Observe(cras_audio_handler_.get());
    FetchAudioNodes();
  }

 private:
  // CrasAudioHandler::AudioObserver implementation:
  void OnAudioNodesChanged() override { FetchAudioNodes(); }

  void FetchAudioNodes() {
    if (!base::SysInfo::IsRunningOnChromeOS())
      return;

    AudioDeviceList audio_devices;
    cras_audio_handler_->GetAudioDevices(&audio_devices);
    parent_->SetAudioDevices(audio_devices);
  }

  const raw_ptr<AudioDevices> parent_;
  // Owned by |AssistantManagerServiceImpl|.
  const raw_ptr<CrasAudioHandler> cras_audio_handler_;
  base::ScopedObservation<CrasAudioHandler, CrasAudioHandler::AudioObserver>
      scoped_observer_{this};
};

// Sends the new hotword model to |cras_audio_handler|. If that fails this class
// will attempt to set the hotword model to |kDefaultLocale|.
class AudioDevices::HotwordModelUpdater {
 public:
  HotwordModelUpdater(CrasAudioHandler* cras_audio_handler,
                      uint64_t hotword_device,
                      const std::string& locale)
      : cras_audio_handler_(cras_audio_handler),
        hotword_device_(hotword_device),
        locale_(locale) {
    SendUpdate();
  }

  HotwordModelUpdater(const HotwordModelUpdater&) = delete;
  HotwordModelUpdater& operator=(const HotwordModelUpdater&) = delete;
  ~HotwordModelUpdater() = default;

 private:
  void SendUpdate() {
    std::string hotword_model =
        ToHotwordModel(locale_).value_or(kDefaultLocale);

    VLOG(2) << "Changing audio hotword model of device " << hotword_device_
            << " to '" << hotword_model << "'";

    cras_audio_handler_->SetHotwordModel(
        hotword_device_, hotword_model,
        base::BindOnce(&HotwordModelUpdater::SetDspHotwordLocaleCallback,
                       weak_factory_.GetWeakPtr(), hotword_model));
  }

  void SetDspHotwordLocaleCallback(std::string pref_locale, bool success) {
    base::UmaHistogramBoolean("Assistant.SetDspHotwordLocale", success);
    if (success) {
      VLOG(2) << "Successfully changed audio hotword model";
      return;
    }

    LOG(ERROR) << "Set " << pref_locale
               << " hotword model failed, fallback to default locale.";
    // Reset the locale to the default value if we failed to sync it to the
    // locale stored in user's pref.
    cras_audio_handler_->SetHotwordModel(
        hotword_device_, /* hotword_model */ kDefaultLocale,
        base::BindOnce([](bool success) {
          if (!success)
            LOG(ERROR) << "Reset to default hotword model failed.";
        }));
  }

  const raw_ptr<CrasAudioHandler> cras_audio_handler_;
  uint64_t hotword_device_;
  std::string locale_;

  base::WeakPtrFactory<HotwordModelUpdater> weak_factory_{this};
};

AudioDevices::AudioDevices(CrasAudioHandler* cras_audio_handler,
                           const std::string& locale)
    : cras_audio_handler_(cras_audio_handler),
      locale_(locale),
      scoped_cras_audio_handler_observer_(
          std::make_unique<ScopedCrasAudioHandlerObserver>(cras_audio_handler,
                                                           this)) {
  // Note we can only start the observer here, at the end of the constructor,
  // to ensure this class is properly initialized when we receive the current
  // list of audio devices.
  scoped_cras_audio_handler_observer_->StartObserving();
}

AudioDevices::~AudioDevices() = default;

void AudioDevices::AddAndFireObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);

  observer->SetHotwordDeviceId(ToString(hotword_device_id_));
  observer->SetDeviceId(ToString(device_id_));
}

void AudioDevices::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AudioDevices::SetLocale(const std::string& locale) {
  locale_ = locale;
  UpdateHotwordModel();
}

void AudioDevices::SetAudioDevicesForTest(
    const AudioDeviceList& audio_devices) {
  SetAudioDevices(audio_devices);
}

void AudioDevices::SetAudioDevices(const AudioDeviceList& devices) {
  UpdateHotwordDeviceId(devices);
  UpdateDeviceId(devices);
  UpdateHotwordModel();
}

void AudioDevices::UpdateHotwordDeviceId(const AudioDeviceList& devices) {
  hotword_device_id_ = GetHotwordDeviceId(devices);

  VLOG(2) << "Changed audio hotword input device to "
          << ToString(hotword_device_id_).value_or("<none>");

  for (auto& observer : observers_)
    observer.SetHotwordDeviceId(ToString(hotword_device_id_));
}

void AudioDevices::UpdateDeviceId(const AudioDeviceList& devices) {
  device_id_ = GetPreferredDeviceId(devices);

  VLOG(2) << "Changed audio input device to "
          << ToString(device_id_).value_or("<none>");

  for (auto& observer : observers_)
    observer.SetDeviceId(ToString(device_id_));
}

void AudioDevices::UpdateHotwordModel() {
  if (!hotword_device_id_)
    return;

  if (!features::IsDspHotwordEnabled())
    return;

  hotword_model_updater_ = std::make_unique<HotwordModelUpdater>(
      cras_audio_handler_, hotword_device_id_.value(), locale_);
}

}  // namespace ash::assistant
