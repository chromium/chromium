// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_AUDIO_SERVICE_H_
#define COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_AUDIO_SERVICE_H_

#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "components/media_effects/test/fake_audio_system_info.h"
#include "media/audio/audio_device_description.h"
#include "services/audio/public/mojom/audio_service.mojom.h"

namespace media_effects {

class FakeAudioService : public audio::mojom::AudioService {
 public:
  FakeAudioService();
  ~FakeAudioService() override;

  FakeAudioService(const FakeAudioService&) = delete;
  FakeAudioService& operator=(const FakeAudioService&) = delete;

  // Simulate connecting and disconnecting a mic device with the given
  // `descriptor`.
  void AddFakeInputDevice(const media::AudioDeviceDescription& descriptor);
  bool AddFakeInputDeviceBlocking(
      const media::AudioDeviceDescription& descriptor);
  void RemoveFakeInputDevice(const std::string& device_id);
  bool RemoveFakeInputDeviceBlocking(const std::string& device_id);

  // `callback` will be triggered after the system info replies back to its
  // client in GetInputDeviceDescriptions(). Useful as a stopping point for a
  // base::RunLoop.
  void SetOnRepliedWithInputDeviceDescriptionsCallback(
      base::OnceClosure callback);

  // `callback` will be triggered when the system info receives a
  // GetInputStreamParameters call.
  void SetOnGetInputStreamParametersCallback(
      base::RepeatingCallback<void(const std::string&)> callback);

  // `callback` will be triggered when the system info receives a
  // BindStreamFactory call.
  void SetBindStreamFactoryCallback(base::RepeatingClosure callback) {
    on_bind_stream_factory_callback_ = std::move(callback);
  }

  // audio::mojom::AudioService implementation
  void BindSystemInfo(
      mojo::PendingReceiver<audio::mojom::SystemInfo> receiver) override;
  void BindStreamFactory(mojo::PendingReceiver<media::mojom::AudioStreamFactory>
                             receiver) override;
  void BindDebugRecording(
      mojo::PendingReceiver<audio::mojom::DebugRecording> receiver) override {}
  void BindDeviceNotifier(
      mojo::PendingReceiver<audio::mojom::DeviceNotifier> receiver) override {}
  void BindLogFactoryManager(
      mojo::PendingReceiver<audio::mojom::LogFactoryManager> receiver)
      override {}
  void BindTestingApi(
      mojo::PendingReceiver<audio::mojom::TestingApi> receiver) override {}

 private:
  FakeAudioSystemInfo fake_system_info_;

  base::RepeatingClosure on_bind_stream_factory_callback_ = base::DoNothing();
};

class ScopedFakeAudioService : public FakeAudioService {
 public:
  ScopedFakeAudioService();
  ~ScopedFakeAudioService() override;

  ScopedFakeAudioService(const ScopedFakeAudioService&) = delete;
  ScopedFakeAudioService& operator=(const ScopedFakeAudioService&) = delete;

 private:
  std::optional<base::AutoReset<audio::mojom::AudioService*>>
      fake_audio_service_auto_reset_;
};

}  // namespace media_effects

#endif  // COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_AUDIO_SERVICE_H_
