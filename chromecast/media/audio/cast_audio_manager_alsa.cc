// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager_alsa.h"

#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/api/cma_backend_factory.h"
#include "chromecast/media/audio/audio_buildflags.h"
#include "chromecast/media/audio/cast_audio_input_stream.h"
#include "media/audio/alsa/alsa_input.h"
#include "media/audio/alsa/alsa_wrapper.h"

namespace chromecast {
namespace media {

namespace {

// TODO(alokp): Query the preferred value from media backend.
const int kDefaultSampleRate = BUILDFLAG(AUDIO_INPUT_SAMPLE_RATE);

// TODO(jyw): Query the preferred value from media backend.
const int kDefaultInputBufferSize = 1024;

#if BUILDFLAG(ENABLE_AUDIO_CAPTURE_SERVICE)
const int kCommunicationsSampleRate = 16000;
const int kCommunicationsInputBufferSize = 160;  // 10 ms.
#endif

// Since "default" and "dmix" devices are virtual devices mapped to real
// devices, we remove them from the list to avoiding duplicate counting.
constexpr std::string_view kInvalidAudioInputDevices[] = {
    "default",
    "dmix",
    "null",
    "communications",
};

// Constants specified by the ALSA API for device hints.
constexpr char kPcmInterfaceName[] = "pcm";
constexpr char kIoHintName[] = "IOID";
constexpr char kNameHintName[] = "NAME";
constexpr char kDescriptionHintName[] = "DESC";

bool IsAlsaDeviceAvailable(CastAudioManagerAlsa::StreamType type,
                           const char* device_name) {
  if (!device_name)
    return false;

  // We do prefix matches on the device name to see whether to include
  // it or not.
  if (type == CastAudioManagerAlsa::kStreamCapture) {
    // Check if the device is in the list of invalid devices.
    for (size_t i = 0; i < std::size(kInvalidAudioInputDevices); ++i) {
      if (kInvalidAudioInputDevices[i] == device_name)
        return false;
    }
    return true;
  } else {
    DCHECK_EQ(CastAudioManagerAlsa::kStreamPlayback, type);
    // We prefer the device type that maps straight to hardware but
    // goes through software conversion if needed (e.g. incompatible
    // sample rate).
    // TODO(joi): Should we prefer "hw" instead?
    const std::string kDeviceTypeDesired = "plughw";
    return kDeviceTypeDesired == device_name;
  }
}

std::string UnwantedDeviceTypeWhenEnumerating(
    CastAudioManagerAlsa::StreamType wanted_type) {
  return wanted_type == CastAudioManagerAlsa::kStreamPlayback ? "Input"
                                                              : "Output";
}

}  // namespace

CastAudioManagerAlsa::CastAudioManagerAlsa(
    std::unique_ptr<::media::AudioThread> audio_thread,
    ::media::AudioLogFactory* audio_log_factory,
    CastAudioManagerHelper::Delegate* delegate,
    base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
    scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    bool use_mixer)
    : CastAudioManager(std::move(audio_thread),
                       audio_log_factory,
                       delegate,
                       std::move(backend_factory_getter),
                       browser_task_runner,
                       media_task_runner,
                       use_mixer),
      wrapper_(new ::media::AlsaWrapper()) {}

CastAudioManagerAlsa::~CastAudioManagerAlsa() {}

bool CastAudioManagerAlsa::HasAudioInputDevices() {
  return true;
}

void CastAudioManagerAlsa::GetAudioInputDeviceNames(
    ::media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());

  // Prepend the default device since we always want it to be on the top of the
  // list for all platforms. Note, pulse has exclusively opened the default
  // device, so we must open the device via the "default" moniker.
  device_names->push_front(::media::AudioDeviceName::CreateDefault());
#if BUILDFLAG(ENABLE_AUDIO_CAPTURE_SERVICE)
  device_names->push_back(::media::AudioDeviceName::CreateCommunications());
#endif  // BUILDFLAG(ENABLE_AUDIO_CAPTURE_SERVICE)

  GetAlsaAudioDevices(kStreamCapture, device_names);
}

::media::AudioParameters CastAudioManagerAlsa::GetInputStreamParameters(
    const std::string& device_id) {
  if (device_id == ::media::AudioDeviceDescription::kCommunicationsDeviceId) {
#if !BUILDFLAG(ENABLE_AUDIO_CAPTURE_SERVICE)
    NOTIMPLEMENTED()
        << "Capture Service is not enabled, return a fake AudioParameters.";
    return ::media::AudioParameters();
#else
    return ::media::AudioParameters(::media::AudioParameters::AUDIO_PCM_LINEAR,
                                    ::media::CHANNEL_LAYOUT_MONO,
                                    kCommunicationsSampleRate,
                                    kCommunicationsInputBufferSize);
#endif  // BUILDFLAG(ENABLE_AUDIO_CAPTURE_SERVICE)
  }
  // TODO(jyw): Be smarter about sample rate instead of hardcoding it.
  // Need to send a valid AudioParameters object even when it will be unused.
  return ::media::AudioParameters(
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ::media::ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
      kDefaultInputBufferSize);
}

::media::AudioInputStream* CastAudioManagerAlsa::MakeLinearInputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id,
    const ::media::AudioManager::LogCallback& log_callback) {
  DCHECK_EQ(::media::AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

::media::AudioInputStream* CastAudioManagerAlsa::MakeLowLatencyInputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id,
    const ::media::AudioManager::LogCallback& log_callback) {
  DCHECK_EQ(::media::AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

::media::AudioInputStream* CastAudioManagerAlsa::MakeInputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id) {
  std::string device_name =
      (device_id == ::media::AudioDeviceDescription::kDefaultDeviceId)
          ? ::media::AlsaPcmInputStream::kAutoSelectDevice
          : device_id;
  if (device_name == ::media::AudioDeviceDescription::kCommunicationsDeviceId) {
#if !BUILDFLAG(ENABLE_AUDIO_CAPTURE_SERVICE)
    NOTIMPLEMENTED() << "Capture Service is not enabled, return nullptr.";
    return nullptr;
#else
    return new CastAudioInputStream(this, params, device_name);
#endif  // BUILDFLAG(ENABLE_AUDIO_CAPTURE_SERVICE)
  }
  return new ::media::AlsaPcmInputStream(this, device_name, params,
                                         wrapper_.get());
}

void CastAudioManagerAlsa::GetAlsaAudioDevices(
    StreamType type,
    ::media::AudioDeviceNames* device_names) {
  int card = -1;

  // Loop through the sound cards to get ALSA device hints.
  while (!wrapper_->CardNext(&card) && card >= 0) {
    void** hints = NULL;
    int error = wrapper_->DeviceNameHint(card, kPcmInterfaceName, &hints);
    if (!error) {
      GetAlsaDevicesInfo(type, hints, device_names);

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
    } else {
      DLOG(WARNING) << "GetAlsaAudioDevices: unable to get device hints: "
                    << wrapper_->StrError(error);
    }
  }
}

void CastAudioManagerAlsa::GetAlsaDevicesInfo(
    StreamType type,
    void** hints,
    ::media::AudioDeviceNames* device_names) {
  const std::string unwanted_device_type =
      UnwantedDeviceTypeWhenEnumerating(type);

  for (void** hint_iter = hints; *hint_iter != NULL; hint_iter++) {
    // Only examine devices of the right type.  Valid values are
    // "Input", "Output", and NULL which means both input and output.
    std::unique_ptr<char, base::FreeDeleter> io(
        wrapper_->DeviceNameGetHint(*hint_iter, kIoHintName));
    if (io && unwanted_device_type == io.get())
      continue;

    // Get the unique device name for the device.
    std::unique_ptr<char, base::FreeDeleter> unique_device_name(
        wrapper_->DeviceNameGetHint(*hint_iter, kNameHintName));

    // Find out if the device is available.
    if (IsAlsaDeviceAvailable(type, unique_device_name.get())) {
      // Get the description for the device.
      std::unique_ptr<char, base::FreeDeleter> desc(
          wrapper_->DeviceNameGetHint(*hint_iter, kDescriptionHintName));

      ::media::AudioDeviceName name;
      name.unique_id = unique_device_name.get();
      if (desc) {
        name.device_name = desc.get();
        // Use the more user friendly description as name.
        // Replace '\n' with '-'.
        name.device_name.replace(name.device_name.find('\n'), 1, 1, '-');
      } else {
        // Virtual devices don't necessarily have descriptions.
        // Use their names instead.
        name.device_name = unique_device_name.get();
      }

      // Store the device information.
      device_names->push_back(name);
    }
  }
}

}  // namespace media
}  // namespace chromecast
